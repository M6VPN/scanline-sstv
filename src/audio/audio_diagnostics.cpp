// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/audio/audio_diagnostics.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/audio/audio_diagnostics.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <thread>

namespace sstv::audio {
namespace {

constexpr std::uint32_t LOOPBACK_PREROLL_MS = 100;
constexpr std::uint32_t LOOPBACK_MARKER_MS = 500;
constexpr std::uint32_t LOOPBACK_TRAILING_MS = 200;
constexpr std::size_t WORKER_BLOCK_FRAMES = 1'024;

[[nodiscard]] DiagnosticError
makeError(const DiagnosticErrorCode code, const DiagnosticState state,
	std::string operation, std::string message,
	std::optional<DeviceIdentity> identity = {}, const bool recoverable = false)
{
	return {code, state, std::move(operation), std::move(message),
	    std::move(identity), recoverable};
}

[[nodiscard]] bool
needsPlayback(const DiagnosticOperation operation) noexcept
{
	return operation != DiagnosticOperation::inputMeter;
}

[[nodiscard]] bool
needsCapture(const DiagnosticOperation operation) noexcept
{
	return operation != DiagnosticOperation::outputCalibration;
}

[[nodiscard]] double
toDbfs(const double value) noexcept
{
	return value > 0.0 ? 20.0 * std::log10(value) : silenceDbfsFloor;
}

class LevelAccumulator {
public:
	[[nodiscard]] bool add(const std::span<const float> samples) noexcept
	{
		for (const float sample : samples) {
			if (!std::isfinite(sample)) {
				return false;
			}
			const long double value = sample;
			sum_ += value;
			squares_ += value * value;
			peak_ = std::max(peak_, std::abs(static_cast<double>(sample)));
			clippedPositive_ += sample >= 1.0F ? 1U : 0U;
			clippedNegative_ += sample <= -1.0F ? 1U : 0U;
			++frames_;
		}
		return true;
	}

	[[nodiscard]] LevelMeasurement snapshot() const noexcept
	{
		LevelMeasurement result;
		result.state = peak_ == 0.0 ? LevelState::silence : LevelState::valid;
		result.peak = peak_;
		result.peakDbfs = toDbfs(peak_);
		result.frames = frames_;
		result.clippedPositive = clippedPositive_;
		result.clippedNegative = clippedNegative_;
		if (frames_ != 0) {
			result.dcMean = static_cast<double>(sum_ / frames_);
			result.rms = std::sqrt(static_cast<double>(squares_ / frames_));
			result.rmsDbfs = toDbfs(result.rms);
		}
		return result;
	}

private:
	long double sum_ = 0.0L;
	long double squares_ = 0.0L;
	double peak_ = 0.0;
	std::uint64_t frames_ = 0;
	std::uint64_t clippedPositive_ = 0;
	std::uint64_t clippedNegative_ = 0;
};

[[nodiscard]] std::optional<AudioDevice>
findExactDevice(const BackendDiscovery& backend, const DeviceIdentity& identity)
{
	std::optional<AudioDevice> match;
	for (const AudioDevice& device : backend.devices) {
		if (device.identity.backend == identity.backend
		    && device.identity.direction == identity.direction
		    && device.identity.opaque == identity.opaque) {
			if (match || device.hasIdentityCollision) {
				return std::nullopt;
			}
			match = device;
		}
	}
	return match;
}

[[nodiscard]] std::uint64_t
durationFrames(const DiagnosticRequest& request) noexcept
{
	return static_cast<std::uint64_t>(request.sampleRate) * request.durationMs / 1'000U;
}

void
publish(const DiagnosticSnapshotCallback& callback,
	const DiagnosticSnapshot& snapshot)
{
	if (callback) {
		callback(std::make_shared<const DiagnosticSnapshot>(snapshot));
	}
}

[[nodiscard]] DiagnosticError
streamError(const StreamError& error, const DiagnosticState state,
	const std::optional<DeviceIdentity>& identity = {})
{
	return makeError(DiagnosticErrorCode::streamFailure, state,
	    error.operation, error.message, identity, error.isRecoverable);
}

} // namespace

AudioDiagnosticsService::AudioDiagnosticsService(
	std::shared_ptr<AudioDiscoveryProvider> provider,
	AudioStreamAdapterFactory streamFactory)
	: discoveryProvider_(std::move(provider)), streamFactory_(std::move(streamFactory))
{
}

AudioDiagnosticsService::~AudioDiagnosticsService()
{
	requestStop();
}

DiagnosticDiscoveryResult
AudioDiagnosticsService::refresh(const AudioBackend backend,
	const std::stop_token stopToken)
{
	std::lock_guard lock(discoveryMutex_);
	if (!discoveryProvider_) {
		return makeError(DiagnosticErrorCode::discoveryFailed,
		    DiagnosticState::faulted, "refresh", "audio discovery provider is unavailable");
	}
	BackendDiscovery discovered = discoveryProvider_->discoverBackend(backend, stopToken);
	if (stopToken.stop_requested() || discovered.status == BackendStatus::cancelled) {
		return makeError(DiagnosticErrorCode::cancelled, DiagnosticState::cancelled,
		    "refresh", "audio device refresh was cancelled", {}, true);
	}
	if (discovered.status != BackendStatus::available) {
		return makeError(DiagnosticErrorCode::discoveryFailed,
		    DiagnosticState::faulted, "refresh",
		    discovered.diagnostic.empty() ? "requested backend is unavailable"
		                                  : discovered.diagnostic, {}, true);
	}
	const std::uint64_t generation
	    = nextGeneration_.fetch_add(1, std::memory_order_relaxed);
	auto snapshot = std::make_shared<AudioDiscoverySnapshot>(AudioDiscoverySnapshot{
	    generation, {std::move(discovered)}, isRealAudioBackend(backend)});
	publishedGeneration_.store(generation, std::memory_order_release);
	return std::shared_ptr<const AudioDiscoverySnapshot>(std::move(snapshot));
}

DiagnosticResult
AudioDiagnosticsService::run(const DiagnosticRequest& request,
	const std::stop_token stopToken, DiagnosticSnapshotCallback callback)
{
	bool expected = false;
	if (!isRunning_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
		return makeError(DiagnosticErrorCode::busy, DiagnosticState::faulted,
		    "start", "another audio diagnostic already owns this service", {}, true);
	}
	struct RunningGuard {
		std::atomic<bool>& flag;
		~RunningGuard() { flag.store(false, std::memory_order_release); }
	} guard{isRunning_};
	isStopRequested_.store(false, std::memory_order_release);
	std::stop_source runCancellation;
	{
		std::lock_guard lock(cancellationMutex_);
		cancellationSource_ = runCancellation;
	}
	std::stop_callback forwardCancellation(stopToken,
	    [runCancellation]() mutable { runCancellation.request_stop(); });
	const std::stop_token runToken = runCancellation.get_token();
	DiagnosticSnapshot progress;
	progress.operation = request.operation;
	progress.playbackIdentity = request.playbackIdentity;
	progress.captureIdentity = request.captureIdentity;
	if (request.playbackIdentity) progress.playbackChannel = request.playbackChannel;
	if (request.captureIdentity) progress.captureChannel = request.captureChannel;
	progress.state = DiagnosticState::validating;
	progress.totalFrames = durationFrames(request);
	publish(callback, progress);
	if ((needsPlayback(request.operation) && !request.isRealAudioArmed)
	    || request.durationMs < minimumDiagnosticDurationMs
	    || request.durationMs > maximumDiagnosticDurationMs
	    || request.sampleRate != nominalAudioSampleRate
	    || request.periodFrames == 0 || request.periodFrames > maximumAudioPeriodFrames
	    || request.periodCount == 0 || request.periodCount > maximumAudioPeriodCount
	    || (needsPlayback(request.operation) && !request.playbackIdentity)
	    || (needsCapture(request.operation) && !request.captureIdentity)
	    || !streamFactory_) {
		const DiagnosticErrorCode code = needsPlayback(request.operation)
		    && !request.isRealAudioArmed ? DiagnosticErrorCode::unarmed
		                               : DiagnosticErrorCode::invalidRequest;
		return makeError(code, DiagnosticState::faulted, "validate",
		    code == DiagnosticErrorCode::unarmed
		        ? "real audio output requires a fresh per-run arm action"
		        : "audio diagnostic request is invalid");
	}
	if (needsPlayback(request.operation)
	    && (request.levelDbfs < minimumDiagnosticLevelDbfs
	        || request.levelDbfs > maximumDiagnosticLevelDbfs
	        || !std::isfinite(request.levelDbfs))) {
		return makeError(DiagnosticErrorCode::invalidRequest,
		    DiagnosticState::faulted, "validate",
		    "output level is outside the -60 to -6 dBFS safety range");
	}
	if (request.expectedDiscoveryGeneration
	    && *request.expectedDiscoveryGeneration
	        != publishedGeneration_.load(std::memory_order_acquire)) {
		return makeError(DiagnosticErrorCode::deviceSelection,
		    DiagnosticState::faulted, "validate",
		    "selected devices belong to a stale GUI discovery generation");
	}
	DiagnosticDiscoveryResult refreshed = refresh(request.backend, runToken);
	if (const auto* error = std::get_if<DiagnosticError>(&refreshed)) {
		return *error;
	}
	const auto snapshot = std::get<std::shared_ptr<const AudioDiscoverySnapshot>>(refreshed);
	progress.discoveryGeneration = snapshot->generation;
	const BackendDiscovery& backend = snapshot->backends.front();
	if (request.playbackIdentity) {
		if (request.playbackIdentity->backend != request.backend
		    || request.playbackIdentity->direction != AudioDirection::playback
		    || request.playbackIdentity->opaque.empty()
		    || !findExactDevice(backend, *request.playbackIdentity)) {
			return makeError(DiagnosticErrorCode::deviceSelection,
			    DiagnosticState::faulted, "select-playback",
			    "exact playback identity is missing, malformed, stale, or colliding",
			    request.playbackIdentity, true);
		}
	}
	if (request.captureIdentity) {
		if (request.captureIdentity->backend != request.backend
		    || request.captureIdentity->direction != AudioDirection::capture
		    || request.captureIdentity->opaque.empty()
		    || !findExactDevice(backend, *request.captureIdentity)) {
			return makeError(DiagnosticErrorCode::deviceSelection,
			    DiagnosticState::faulted, "select-capture",
			    "exact capture identity is missing, malformed, stale, or colliding",
			    request.captureIdentity, true);
		}
	}
	AudioStreamConfiguration configuration;
	configuration.direction = request.operation == DiagnosticOperation::inputMeter
	    ? StreamDirection::capture
	    : request.operation == DiagnosticOperation::outputCalibration
	        ? StreamDirection::playback : StreamDirection::duplex;
	configuration.sampleRate = request.sampleRate;
	configuration.periodFrames = request.periodFrames;
	configuration.periodCount = request.periodCount;
	configuration.playbackRingCapacity = request.playbackRingCapacity;
	configuration.captureRingCapacity = request.captureRingCapacity;
	configuration.playbackPrefillFrames = needsPlayback(request.operation)
	    ? request.playbackPrefillFrames : 0;
	configuration.playbackMapping = request.playbackMapping;
	if (request.playbackIdentity) {
		configuration.playbackDevice = StreamDeviceSelection{
		    *request.playbackIdentity, snapshot->generation};
		configuration.playbackChannels = request.playbackChannels;
		configuration.selectedPlaybackChannel = request.playbackChannel;
	}
	if (request.captureIdentity) {
		configuration.captureDevice = StreamDeviceSelection{
		    *request.captureIdentity, snapshot->generation};
		configuration.captureChannels = request.captureChannels;
		configuration.selectedCaptureChannel = request.captureChannel;
	}
	AudioStreamCreateResult created = createAudioStream(
	    configuration, streamFactory_());
	if (const auto* error = std::get_if<StreamError>(&created)) {
		return streamError(*error, DiagnosticState::faulted);
	}
	std::unique_ptr<AudioStream> stream
	    = std::move(std::get<std::unique_ptr<AudioStream>>(created));
	struct StreamGuard {
		AudioStream& stream;
		~StreamGuard()
		{
			(void)stream.requestStop();
			(void)stream.stop();
			(void)stream.close();
		}
	} streamGuard{*stream};
	progress.state = DiagnosticState::opening;
	publish(callback, progress);
	if (StreamOperationResult error = stream->open(*snapshot, runToken)) {
		return streamError(*error, DiagnosticState::faulted);
	}
	progress.state = DiagnosticState::priming;
	progress.negotiated = stream->statistics().negotiated;
	publish(callback, progress);
	if (StreamOperationResult error = stream->prime(runToken)) {
		return streamError(*error, DiagnosticState::faulted);
	}
	std::vector<float> generated;
	if (request.operation == DiagnosticOperation::outputCalibration) {
		generated = generateCalibrationSignal(
		    request.sampleRate, request.durationMs, request.levelDbfs);
	} else if (request.operation == DiagnosticOperation::loopback) {
		generated = generateLoopbackSequence(request.sampleRate, request.levelDbfs);
		progress.totalFrames = generated.size();
	}
	const std::size_t generatedContentFrames = generated.size();
	if (!generated.empty()) {
		generated.resize(generated.size() + request.sampleRate / 10U, 0.0F);
	}
	std::size_t queued = 0;
	if (!generated.empty()) {
		const std::size_t initial = std::min(
		    generated.size(), std::max(request.playbackPrefillFrames,
		        static_cast<std::size_t>(request.periodFrames) * request.periodCount));
		queued = stream->queuePlayback(std::span<const float>(generated.data(), initial));
		if (queued < request.playbackPrefillFrames) {
			return makeError(DiagnosticErrorCode::streamFailure,
			    DiagnosticState::faulted, "prime",
			    "playback ring could not reach the required prime threshold");
		}
	}
	if (StreamOperationResult error = stream->start()) {
		return streamError(*error, DiagnosticState::faulted);
	}
	progress.state = DiagnosticState::running;
	publish(callback, progress);
	LevelAccumulator levels;
	std::vector<float> captureBlock(WORKER_BLOCK_FRAMES);
	std::vector<float> loopbackCapture;
	if (request.operation == DiagnosticOperation::loopback) {
		loopbackCapture.reserve(generated.size() + request.sampleRate);
	}
	const auto started = std::chrono::steady_clock::now();
	const auto deadline = started + std::chrono::milliseconds(
	    request.durationMs + 2'000U);
	std::uint64_t lastPublishedFrames = 0;
	for (;;) {
		if (runToken.stop_requested()
		    || isStopRequested_.load(std::memory_order_acquire)) {
			progress.state = DiagnosticState::stopping;
			publish(callback, progress);
			return makeError(DiagnosticErrorCode::cancelled,
			    DiagnosticState::cancelled, "run",
			    "audio diagnostic was cancelled after stream cleanup", {}, true);
		}
		if (!generated.empty() && queued < generated.size()) {
			queued += stream->queuePlayback(std::span<const float>(
			    generated.data() + queued, generated.size() - queued));
		}
		if (needsCapture(request.operation)) {
			const std::size_t maximumRead
			    = request.operation == DiagnosticOperation::inputMeter
			    ? std::min<std::size_t>(captureBlock.size(),
			          static_cast<std::size_t>(progress.totalFrames
			              - std::min(progress.totalFrames, levels.snapshot().frames)))
			    : captureBlock.size();
			const std::size_t read = stream->readCapture(
			    std::span<float>(captureBlock.data(), maximumRead));
			if (read != 0) {
				const std::span<const float> samples(captureBlock.data(), read);
				if (!levels.add(samples)) {
					return makeError(DiagnosticErrorCode::nonFiniteCapture,
					    DiagnosticState::faulted, "capture",
					    "capture contained NaN or infinite samples");
				}
				if (request.operation == DiagnosticOperation::loopback
				    && loopbackCapture.size() + read
				        <= generated.size() + request.sampleRate) {
					loopbackCapture.insert(loopbackCapture.end(),
					    samples.begin(), samples.end());
				}
			}
		}
		progress.stream = stream->statistics();
		progress.negotiated = progress.stream.negotiated;
		progress.level = levels.snapshot();
		progress.processedFrames = request.operation == DiagnosticOperation::inputMeter
		    ? progress.level->frames : progress.stream.playbackFramesDelivered;
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
		    std::chrono::steady_clock::now() - started).count();
		progress.elapsedMs = static_cast<std::uint32_t>(
		    std::min<std::int64_t>(elapsed, std::numeric_limits<std::uint32_t>::max()));
		progress.remainingMs = progress.elapsedMs >= request.durationMs
		    ? 0 : request.durationMs - progress.elapsedMs;
		if (progress.processedFrames >= lastPublishedFrames + request.sampleRate / 10U) {
			publish(callback, progress);
			lastPublishedFrames = progress.processedFrames;
		}
		if (stream->poll() == StreamState::faulted) {
			return makeError(DiagnosticErrorCode::streamFailure,
			    DiagnosticState::faulted, "run", "audio device disconnected",
			    request.playbackIdentity ? request.playbackIdentity : request.captureIdentity,
			    true);
		}
		if (progress.stream.playbackFramesZeroFilled != 0 && !generated.empty()) {
			return makeError(DiagnosticErrorCode::underrun,
			    DiagnosticState::faulted, "playback",
			    "playback underrun stopped the diagnostic", request.playbackIdentity, true);
		}
		const bool meterDone = request.operation == DiagnosticOperation::inputMeter
		    && progress.level->frames >= durationFrames(request);
		const bool outputDone = request.operation == DiagnosticOperation::outputCalibration
		    && progress.stream.playbackFramesDelivered >= generatedContentFrames;
		const bool loopbackDone = request.operation == DiagnosticOperation::loopback
		    && progress.stream.playbackFramesDelivered >= generatedContentFrames
		    && loopbackCapture.size() >= generatedContentFrames;
		if (meterDone || outputDone || loopbackDone) {
			break;
		}
		if (std::chrono::steady_clock::now() >= deadline) {
			return makeError(DiagnosticErrorCode::timeout,
			    DiagnosticState::faulted, "run", "audio diagnostic timed out", {}, true);
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}
	progress.state = request.operation == DiagnosticOperation::loopback
	    ? DiagnosticState::analysing : DiagnosticState::stopping;
	publish(callback, progress);
	if (StreamOperationResult error = stream->requestStop()) {
		return streamError(*error, DiagnosticState::faulted);
	}
	if (StreamOperationResult error = stream->stop()) {
		return streamError(*error, DiagnosticState::faulted);
	}
	if (StreamOperationResult error = stream->close()) {
		return streamError(*error, DiagnosticState::faulted);
	}
	progress.stream = stream->statistics();
	progress.level = levels.snapshot();
	if (request.operation == DiagnosticOperation::loopback) {
		if (progress.stream.captureFramesDropped != 0) {
			return makeError(DiagnosticErrorCode::inconclusive,
			    DiagnosticState::completed, "analyse-loopback",
			    "capture drops made the loopback result inconclusive", {}, true);
		}
		generated.resize(generatedContentFrames);
		progress.loopback = analyseLoopback(generated, loopbackCapture, request.sampleRate);
		if (!progress.loopback->isConclusive) {
			return makeError(DiagnosticErrorCode::inconclusive,
			    DiagnosticState::completed, "analyse-loopback",
			    progress.loopback->reason, {}, true);
		}
	}
	progress.state = DiagnosticState::completed;
	progress.message = "audio diagnostic completed";
	publish(callback, progress);
	return progress;
}

void
AudioDiagnosticsService::requestStop() noexcept
{
	isStopRequested_.store(true, std::memory_order_release);
	std::lock_guard lock(cancellationMutex_);
	cancellationSource_.request_stop();
}

bool
AudioDiagnosticsService::isRunning() const noexcept
{
	return isRunning_.load(std::memory_order_acquire);
}

std::variant<double, DiagnosticError>
convertDbfsToAmplitude(const double levelDbfs)
{
	if (!std::isfinite(levelDbfs) || levelDbfs < minimumDiagnosticLevelDbfs
	    || levelDbfs > maximumDiagnosticLevelDbfs) {
		return makeError(DiagnosticErrorCode::invalidRequest,
		    DiagnosticState::faulted, "level",
		    "output level is outside the -60 to -6 dBFS safety range");
	}
	return std::pow(10.0, levelDbfs / 20.0);
}

std::vector<float>
generateCalibrationSignal(const std::uint32_t sampleRate,
	const std::uint32_t durationMs, const double levelDbfs)
{
	if (sampleRate != nominalAudioSampleRate
	    || durationMs < minimumDiagnosticDurationMs
	    || durationMs > maximumDiagnosticDurationMs) {
		throw std::invalid_argument("calibration rate or duration is outside safe bounds");
	}
	const auto amplitudeResult = convertDbfsToAmplitude(levelDbfs);
	if (const auto* error = std::get_if<DiagnosticError>(&amplitudeResult)) {
		throw std::invalid_argument(error->message);
	}
	const double amplitude = std::get<double>(amplitudeResult);
	const std::size_t count = static_cast<std::size_t>(sampleRate) * durationMs / 1'000U;
	const std::size_t fade = std::min<std::size_t>(
	    static_cast<std::size_t>(sampleRate) * calibrationFadeMs / 1'000U, count / 2U);
	std::vector<float> result(count);
	for (std::size_t index = 0; index < count; ++index) {
		double envelope = 1.0;
		if (index < fade) {
			envelope = static_cast<double>(index) / static_cast<double>(fade);
		} else if (index >= count - fade) {
			envelope = static_cast<double>(count - 1U - index)
			    / static_cast<double>(fade);
		}
		const double phase = 2.0 * std::numbers::pi * calibrationFrequencyHz
		    * static_cast<double>(index) / sampleRate;
		result[index] = static_cast<float>(amplitude * envelope * std::sin(phase));
	}
	return result;
}

std::vector<float>
generateLoopbackSequence(const std::uint32_t sampleRate, const double levelDbfs)
{
	if (sampleRate != nominalAudioSampleRate) {
		throw std::invalid_argument("loopback requires the 48000 Hz nominal rate");
	}
	const auto amplitudeResult = convertDbfsToAmplitude(levelDbfs);
	if (const auto* error = std::get_if<DiagnosticError>(&amplitudeResult)) {
		throw std::invalid_argument(error->message);
	}
	const double amplitude = std::get<double>(amplitudeResult);
	const std::size_t pre = static_cast<std::size_t>(sampleRate) * LOOPBACK_PREROLL_MS / 1'000U;
	const std::size_t marker = static_cast<std::size_t>(sampleRate) * LOOPBACK_MARKER_MS / 1'000U;
	const std::size_t trailing = static_cast<std::size_t>(sampleRate) * LOOPBACK_TRAILING_MS / 1'000U;
	std::vector<float> result(pre + marker + trailing, 0.0F);
	std::uint16_t lfsr = 0x5a3U;
	const std::size_t halfCycle = sampleRate / (2U * calibrationFrequencyHz);
	const std::size_t fade = static_cast<std::size_t>(sampleRate) * calibrationFadeMs / 1'000U;
	for (std::size_t index = 0; index < marker; ++index) {
		if (index % halfCycle == 0) {
			const std::uint16_t bit = static_cast<std::uint16_t>(
			    ((lfsr >> 10U) ^ (lfsr >> 8U)) & 1U);
			const std::uint16_t shifted = static_cast<std::uint16_t>(lfsr << 1U);
			lfsr = static_cast<std::uint16_t>(
			    static_cast<std::uint16_t>(shifted | bit) & std::uint16_t{0x7ffU});
		}
		const double sign = (lfsr & 1U) != 0 ? 1.0 : -1.0;
		double envelope = 1.0;
		if (index < fade) {
			envelope = static_cast<double>(index) / static_cast<double>(fade);
		}
		if (index >= marker - fade) {
			envelope = static_cast<double>(marker - 1U - index)
			    / static_cast<double>(fade);
		}
		const double phase = 2.0 * std::numbers::pi * calibrationFrequencyHz
		    * static_cast<double>(index) / sampleRate;
		result[pre + index]
		    = static_cast<float>(amplitude * envelope * sign * std::sin(phase));
	}
	return result;
}

std::variant<LevelMeasurement, DiagnosticError>
analyseAudioLevels(const std::vector<float>& samples)
{
	LevelAccumulator accumulator;
	if (!accumulator.add(samples)) {
		return makeError(DiagnosticErrorCode::nonFiniteCapture,
		    DiagnosticState::faulted, "analyse-level",
		    "capture contained NaN or infinite samples");
	}
	return accumulator.snapshot();
}

LoopbackMeasurement
analyseLoopback(const std::vector<float>& generated,
	const std::vector<float>& captured, const std::uint32_t sampleRate)
{
	LoopbackMeasurement result;
	if (sampleRate == 0 || generated.empty() || captured.size() < generated.size()) {
		result.reason = "insufficient captured samples";
		return result;
	}
	if (!std::all_of(generated.begin(), generated.end(),
	        [](const float value) { return std::isfinite(value); })
	    || !std::all_of(captured.begin(), captured.end(),
	        [](const float value) { return std::isfinite(value); })) {
		result.reason = "non-finite samples prevent loopback analysis";
		return result;
	}
	long double generatedEnergy = 0.0L;
	for (const float sample : generated) generatedEnergy += sample * sample;
	if (generatedEnergy == 0.0L) {
		result.reason = "generated correlation marker is silent";
		return result;
	}
	double best = 0.0;
	std::size_t bestOffset = 0;
	double signedBest = 0.0;
	const auto correlateAt = [&generated, &captured, generatedEnergy](
	                             const std::size_t offset) {
		long double dot = 0.0L;
		long double capturedEnergy = 0.0L;
		for (std::size_t index = 0; index < generated.size(); ++index) {
			dot += static_cast<long double>(generated[index]) * captured[offset + index];
			capturedEnergy += static_cast<long double>(captured[offset + index])
			    * captured[offset + index];
		}
		if (capturedEnergy == 0.0L) return 0.0;
		return static_cast<double>(
		    dot / std::sqrt(generatedEnergy * capturedEnergy));
	};
	constexpr std::size_t SEARCH_STEP = 16;
	const std::size_t maximumOffset = captured.size() - generated.size();
	for (std::size_t offset = 0; offset <= maximumOffset; offset += SEARCH_STEP) {
		const double correlation = correlateAt(offset);
		const double magnitude = std::abs(correlation);
		if (magnitude > best) {
			best = magnitude;
			bestOffset = offset;
			signedBest = correlation;
		}
	}
	const std::size_t refineStart = bestOffset > SEARCH_STEP
	    ? bestOffset - SEARCH_STEP : 0;
	const std::size_t refineEnd = std::min(maximumOffset, bestOffset + SEARCH_STEP);
	for (std::size_t offset = refineStart; offset <= refineEnd; ++offset) {
		const double correlation = correlateAt(offset);
		if (std::abs(correlation) > best) {
			best = std::abs(correlation);
			bestOffset = offset;
			signedBest = correlation;
		}
	}
	result.correlation = best;
	result.correlationPassed = best >= loopbackCorrelationThreshold;
	result.latencyFrames = bestOffset;
	result.latencyMilliseconds = 1'000.0 * static_cast<double>(bestOffset)
	    / sampleRate;
	result.hasPolarityInversion = signedBest < 0.0;
	if (!result.correlationPassed) {
		result.reason = best == 0.0 ? "captured loopback is silent"
		                            : "correlation confidence is below 0.65";
		return result;
	}
	double competingPeak = 0.0;
	const std::size_t exclusion = sampleRate / 200U;
	for (std::size_t offset = 0; offset <= maximumOffset; offset += SEARCH_STEP) {
		if (offset + exclusion >= bestOffset && bestOffset + exclusion >= offset) continue;
		competingPeak = std::max(competingPeak, std::abs(correlateAt(offset)));
	}
	if (competingPeak > best * 0.95) {
		result.reason = "correlation has ambiguous competing peaks";
		return result;
	}
	long double generatedSquares = 0.0L;
	long double capturedSquares = 0.0L;
	for (std::size_t index = 0; index < generated.size(); ++index) {
		generatedSquares += static_cast<long double>(generated[index]) * generated[index];
		capturedSquares += static_cast<long double>(captured[bestOffset + index])
		    * captured[bestOffset + index];
	}
	result.gainDb = 10.0 * std::log10(
	    static_cast<double>(capturedSquares / generatedSquares));
	result.isConclusive = true;
	result.reason = "correlation passed";
	return result;
}

} // namespace sstv::audio
