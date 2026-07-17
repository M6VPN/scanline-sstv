// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/audio_diagnostics_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/audio/audio_diagnostics.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace sstv::audio;

void
require(const bool condition, const std::string& message)
{
	if (!condition) throw std::runtime_error(message);
}

class CountingProvider final : public AudioDiscoveryProvider {
public:
	std::size_t calls = 0;
	BackendDiscovery response;
	[[nodiscard]] bool isBackendCompiled(AudioBackend) const override { return true; }
	[[nodiscard]] BackendDiscovery discoverBackend(
	    AudioBackend, std::stop_token) override
	{
		++calls;
		return response;
	}
};

struct DiagnosticMockState {
	AudioCallbackBinding callback;
	NegotiatedStreamFacts facts;
	std::vector<float> output;
	std::atomic<bool> hasStarted{false};
	std::atomic<std::size_t> opens{0};
	std::atomic<std::size_t> primes{0};
	std::atomic<std::size_t> starts{0};
	std::atomic<std::size_t> requests{0};
	std::atomic<std::size_t> stops{0};
	std::atomic<std::size_t> closes{0};
	std::atomic<bool> hasClosed{false};
	std::mutex callbackMutex;
	std::uint32_t outputChannels = 0;
	std::uint32_t pumpFramesOnStart = 0;

	void pump(const std::uint32_t frames, const std::vector<float>& input = {})
	{
		std::lock_guard lock(callbackMutex);
		if (hasClosed.load(std::memory_order_acquire)) return;
		output.assign(static_cast<std::size_t>(frames) * outputChannels, 9.0F);
		callback.process(callback.state, output.empty() ? nullptr : output.data(),
		    input.empty() ? nullptr : input.data(), frames);
	}

	void disconnect()
	{
		std::lock_guard lock(callbackMutex);
		if (hasClosed.load(std::memory_order_acquire)) return;
		callback.notifyFault(callback.state, StreamFaultReason::deviceRemoved);
	}
};

class DiagnosticMockAdapter final : public AudioStreamAdapter {
public:
	explicit DiagnosticMockAdapter(std::shared_ptr<DiagnosticMockState> state)
		: state_(std::move(state))
	{
	}

	[[nodiscard]] AdapterOpenResult open(const AudioStreamConfiguration& configuration,
	    const AudioCallbackBinding& callback, std::stop_token) noexcept override
	{
		++state_->opens;
		{
			std::lock_guard lock(state_->callbackMutex);
			state_->callback = callback;
			state_->hasClosed.store(false, std::memory_order_release);
		}
		state_->facts.backend = configuration.playbackDevice
		    ? configuration.playbackDevice->identity.backend
		    : configuration.captureDevice->identity.backend;
		state_->facts.callbackSampleRate = configuration.sampleRate;
		state_->facts.periodFrames = configuration.periodFrames;
		state_->facts.periodCount = configuration.periodCount;
		if (configuration.direction != StreamDirection::capture) {
			state_->facts.playback = NegotiatedEndpointFacts{SampleFormat::float32,
			    configuration.playbackChannels, SampleFormat::float32,
			    configuration.playbackChannels, configuration.sampleRate};
			state_->outputChannels = configuration.playbackChannels;
		}
		if (configuration.direction != StreamDirection::playback) {
			state_->facts.capture = NegotiatedEndpointFacts{SampleFormat::float32,
			    configuration.captureChannels, SampleFormat::float32,
			    configuration.captureChannels, configuration.sampleRate};
		}
		return state_->facts;
	}

	[[nodiscard]] StreamOperationResult prime() noexcept override
	{
		++state_->primes;
		return std::nullopt;
	}

	[[nodiscard]] StreamOperationResult start() noexcept override
	{
		++state_->starts;
		if (state_->pumpFramesOnStart != 0) {
			state_->pump(state_->pumpFramesOnStart);
		}
		state_->hasStarted.store(true, std::memory_order_release);
		return std::nullopt;
	}

	[[nodiscard]] StreamOperationResult requestStop() noexcept override
	{
		++state_->requests;
		return std::nullopt;
	}

	[[nodiscard]] StreamOperationResult stop() noexcept override
	{
		++state_->stops;
		return std::nullopt;
	}

	[[nodiscard]] StreamOperationResult close() noexcept override
	{
		++state_->closes;
		std::lock_guard lock(state_->callbackMutex);
		state_->hasClosed.store(true, std::memory_order_release);
		return std::nullopt;
	}

private:
	std::shared_ptr<DiagnosticMockState> state_;
};

class BlockingProvider final : public AudioDiscoveryProvider {
public:
	std::atomic<bool> hasEntered{false};
	[[nodiscard]] bool isBackendCompiled(AudioBackend) const override { return true; }
	[[nodiscard]] BackendDiscovery discoverBackend(
	    const AudioBackend backend, const std::stop_token token) override
	{
		hasEntered.store(true, std::memory_order_release);
		while (!token.stop_requested()) std::this_thread::yield();
		return {backend, "null", true, BackendStatus::cancelled, {}, "cancelled"};
	}
};

[[nodiscard]] DeviceIdentity
makeDiagnosticIdentity(const AudioDirection direction)
{
	return {AudioBackend::nullDiagnostic, direction,
	    direction == AudioDirection::playback ? "play" : "capture",
	    IdentityStability::sessionOnly};
}

[[nodiscard]] BackendDiscovery
makeDiagnosticBackend()
{
	return {AudioBackend::nullDiagnostic, "null", true, BackendStatus::available,
	    {{makeDiagnosticIdentity(AudioDirection::playback), "Mock output", false,
	         AudioTransport::virtualDevice, {}, false, {}},
	        {makeDiagnosticIdentity(AudioDirection::capture), "Mock input", false,
	         AudioTransport::virtualDevice, {}, false, {}}}, {}};
}

[[nodiscard]] DiagnosticRequest
makeDiagnosticRequest(const DiagnosticOperation operation)
{
	DiagnosticRequest request;
	request.operation = operation;
	request.backend = AudioBackend::nullDiagnostic;
	request.durationMs = minimumDiagnosticDurationMs;
	request.periodFrames = 16;
	request.periodCount = 2;
	request.playbackChannels = 2;
	request.captureChannels = 2;
	request.playbackChannel = 1;
	request.captureChannel = 1;
	request.isRealAudioArmed = operation != DiagnosticOperation::inputMeter;
	if (operation != DiagnosticOperation::inputMeter) {
		request.playbackIdentity = makeDiagnosticIdentity(AudioDirection::playback);
	}
	if (operation != DiagnosticOperation::outputCalibration) {
		request.captureIdentity = makeDiagnosticIdentity(AudioDirection::capture);
	}
	return request;
}

void
waitForStart(const std::shared_ptr<DiagnosticMockState>& state)
{
	for (std::size_t attempts = 0; attempts < 1'000'000; ++attempts) {
		if (state->hasStarted.load(std::memory_order_acquire)) return;
		std::this_thread::yield();
	}
	throw std::runtime_error("mock diagnostic did not start");
}

void
testDbfsAndCalibration()
{
	const auto low = convertDbfsToAmplitude(-60.0);
	const auto high = convertDbfsToAmplitude(-6.0);
	require(std::holds_alternative<double>(low)
	    && std::abs(std::get<double>(low) - 0.001) < 1.0e-12,
	    "-60 dBFS conversion failed");
	require(std::holds_alternative<double>(high)
	    && std::abs(std::get<double>(high) - 0.5011872336272722) < 1.0e-12,
	    "-6 dBFS conversion failed");
	require(std::holds_alternative<DiagnosticError>(convertDbfsToAmplitude(-60.1))
	    && std::holds_alternative<DiagnosticError>(convertDbfsToAmplitude(-5.9))
	    && std::holds_alternative<DiagnosticError>(convertDbfsToAmplitude(
	        std::numeric_limits<double>::infinity())),
	    "unsafe dBFS value was accepted");
	const std::vector<float> signal = generateCalibrationSignal(48'000, 1'000, -30.0);
	require(signal.size() == 48'000 && signal.front() == 0.0F
	    && signal.back() == 0.0F, "calibration duration or fades are wrong");
	const float peak = *std::max_element(signal.begin(), signal.end());
	require(std::abs(peak - 0.0316228F) < 1.0e-5F,
	    "calibration amplitude was normalized or limited");
	require(std::abs(signal[600] - signal[648]) < 1.0e-6F,
	    "calibration oscillator did not retain continuous phase");
	const std::vector<float> repeated = generateCalibrationSignal(48'000, 1'000, -30.0);
	require(signal == repeated, "calibration signal is not deterministic");
}

void
testLevels()
{
	const std::vector<float> samples{-1.0F, -0.5F, 0.0F, 0.5F, 1.0F};
	const auto analysed = analyseAudioLevels(samples);
	require(std::holds_alternative<LevelMeasurement>(analysed),
	    "finite level samples were rejected");
	const LevelMeasurement level = std::get<LevelMeasurement>(analysed);
	require(level.frames == 5 && level.peak == 1.0 && level.dcMean == 0.0
	    && level.clippedPositive == 1 && level.clippedNegative == 1,
	    "level statistics are incorrect");
	require(std::abs(level.rms - std::sqrt(0.5)) < 1.0e-12,
	    "level RMS is incorrect");
	const LevelMeasurement silence = std::get<LevelMeasurement>(
	    analyseAudioLevels(std::vector<float>(16, 0.0F)));
	require(silence.state == LevelState::silence
	    && silence.peakDbfs == silenceDbfsFloor
	    && silence.rmsDbfs == silenceDbfsFloor,
	    "silence did not use the finite dBFS floor");
	require(std::holds_alternative<DiagnosticError>(analyseAudioLevels(
	        {0.0F, std::numeric_limits<float>::quiet_NaN()}))
	    && std::holds_alternative<DiagnosticError>(analyseAudioLevels(
	        {std::numeric_limits<float>::infinity()})),
	    "non-finite capture did not fail closed");
}

void
testLoopbackAnalysis()
{
	const std::vector<float> generated = generateLoopbackSequence(48'000, -30.0);
	require(generated.size() == 38'400, "loopback sequence duration changed");
	constexpr std::size_t delay = 137;
	std::vector<float> captured(delay + generated.size(), 0.0F);
	for (std::size_t index = 0; index < generated.size(); ++index) {
		captured[delay + index] = generated[index] * -0.5F + 0.001F;
	}
	const LoopbackMeasurement result = analyseLoopback(generated, captured, 48'000);
	require(result.isConclusive && result.correlationPassed
	    && result.latencyFrames == delay && result.hasPolarityInversion,
	    "known loopback delay or polarity was not recovered");
	require(std::abs(result.gainDb + 6.0206) < 0.1,
	    "known loopback gain was not recovered");
	const LoopbackMeasurement silent = analyseLoopback(
	    generated, std::vector<float>(generated.size(), 0.0F), 48'000);
	require(!silent.isConclusive && !silent.correlationPassed,
	    "silent loopback was reported as conclusive");
	const LoopbackMeasurement shortCapture = analyseLoopback(
	    generated, std::vector<float>(100), 48'000);
	require(!shortCapture.isConclusive
	    && shortCapture.reason.find("insufficient") != std::string::npos,
	    "short loopback capture was not inconclusive");
	std::vector<float> nonFiniteCapture = generated;
	nonFiniteCapture[100] = std::numeric_limits<float>::infinity();
	const LoopbackMeasurement nonFinite
	    = analyseLoopback(generated, nonFiniteCapture, 48'000);
	require(!nonFinite.isConclusive
	    && nonFinite.reason.find("non-finite") != std::string::npos,
	    "non-finite loopback analysis did not fail closed");
	const auto markerStart = generated.begin() + 4'800;
	const std::vector<float> shortMarker(markerStart, markerStart + 1'024);
	std::vector<float> ambiguous(shortMarker.size() * 2U);
	std::copy(shortMarker.begin(), shortMarker.end(), ambiguous.begin());
	std::copy(shortMarker.begin(), shortMarker.end(),
	    ambiguous.begin() + static_cast<std::ptrdiff_t>(shortMarker.size()));
	const LoopbackMeasurement ambiguousResult
	    = analyseLoopback(shortMarker, ambiguous, 48'000);
	require(!ambiguousResult.isConclusive
	    && ambiguousResult.reason.find("ambiguous") != std::string::npos,
	    "competing correlation peaks were accepted");
}

void
testUnarmedGate()
{
	auto provider = std::make_shared<CountingProvider>();
	std::size_t adapterCreations = 0;
	AudioDiagnosticsService service(provider, [&adapterCreations] {
		++adapterCreations;
		return std::unique_ptr<AudioStreamAdapter>();
	});
	DiagnosticRequest request;
	request.operation = DiagnosticOperation::outputCalibration;
	request.playbackIdentity = DeviceIdentity{AudioBackend::alsa,
	    AudioDirection::playback, "identity", IdentityStability::sessionOnly};
	const DiagnosticResult result = service.run(request);
	require(std::get<DiagnosticError>(result).code == DiagnosticErrorCode::unarmed,
	    "unarmed output did not use the typed safety failure");
	require(provider->calls == 0 && adapterCreations == 0,
	    "unarmed output reached discovery or adapter creation");
	request.operation = DiagnosticOperation::loopback;
	request.captureIdentity = DeviceIdentity{AudioBackend::alsa,
	    AudioDirection::capture, "capture", IdentityStability::sessionOnly};
	require(std::get<DiagnosticError>(service.run(request)).code
	        == DiagnosticErrorCode::unarmed
	    && provider->calls == 0 && adapterCreations == 0,
	    "unarmed loopback reached backend operations");
}

void
testRefreshAndExactSelection()
{
	auto provider = std::make_shared<CountingProvider>();
	provider->response = {AudioBackend::alsa, "alsa", true, BackendStatus::available,
	    {{{AudioBackend::alsa, AudioDirection::capture, "capture",
	          IdentityStability::sessionOnly}, "Input", false,
	        AudioTransport::unknown, {}, false, {}}}, {}};
	std::size_t adapterCreations = 0;
	AudioDiagnosticsService service(provider, [&adapterCreations] {
		++adapterCreations;
		return std::unique_ptr<AudioStreamAdapter>();
	});
	const DiagnosticDiscoveryResult refreshed = service.refresh(AudioBackend::alsa);
	require(std::holds_alternative<std::shared_ptr<const AudioDiscoverySnapshot>>(refreshed),
	    "diagnostic backend refresh failed");
	const auto snapshot = std::get<std::shared_ptr<const AudioDiscoverySnapshot>>(refreshed);
	DiagnosticRequest request;
	request.operation = DiagnosticOperation::inputMeter;
	request.backend = AudioBackend::alsa;
	request.captureIdentity = DeviceIdentity{AudioBackend::alsa,
	    AudioDirection::capture, "capture", IdentityStability::sessionOnly};
	request.expectedDiscoveryGeneration = snapshot->generation + 1U;
	require(std::get<DiagnosticError>(service.run(request)).code
	        == DiagnosticErrorCode::deviceSelection
	    && provider->calls == 1 && adapterCreations == 0,
	    "stale GUI selection reached backend operations");
	request.expectedDiscoveryGeneration = snapshot->generation;
	request.captureIdentity->opaque = "missing";
	require(std::get<DiagnosticError>(service.run(request)).code
	        == DiagnosticErrorCode::deviceSelection
	    && provider->calls == 2 && adapterCreations == 0,
	    "missing exact device silently selected a replacement");
}

void
testMockMeterAndCapturePolicies()
{
	auto provider = std::make_shared<CountingProvider>();
	provider->response = makeDiagnosticBackend();
	auto state = std::make_shared<DiagnosticMockState>();
	AudioDiagnosticsService service(provider, [state] {
		return std::make_unique<DiagnosticMockAdapter>(state);
	});
	DiagnosticRequest request = makeDiagnosticRequest(DiagnosticOperation::inputMeter);
	constexpr std::size_t frameCount = 12'001;
	request.periodFrames = 8'192;
	request.captureRingCapacity = 1'024;
	std::vector<float> input(frameCount * 2U, 0.0F);
	for (std::size_t frame = 0; frame < frameCount; ++frame) {
		input[frame * 2U] = 0.9F;
		input[frame * 2U + 1U] = frame % 2U == 0 ? 0.5F : -0.25F;
	}
	std::optional<DiagnosticResult> result;
	std::jthread worker([&] { result = service.run(request); });
	waitForStart(state);
	state->pump(static_cast<std::uint32_t>(frameCount), input);
	while (!state->hasClosed.load(std::memory_order_acquire)) {
		state->pump(1'024, input);
		std::this_thread::yield();
	}
	worker.join();
	require(result && std::holds_alternative<DiagnosticSnapshot>(*result),
	    "mock input meter did not complete");
	const DiagnosticSnapshot& snapshot = std::get<DiagnosticSnapshot>(*result);
	require(snapshot.level && snapshot.level->frames == frameCount - 1U
	    && snapshot.level->peak == 0.5 && snapshot.stream.captureFramesDropped > 0,
	    "meter channel selection or capture-drop accounting is wrong: frames="
	        + std::to_string(snapshot.level ? snapshot.level->frames : 0U)
	        + ", peak=" + std::to_string(snapshot.level ? snapshot.level->peak : 0.0)
	        + ", drops=" + std::to_string(snapshot.stream.captureFramesDropped));
	require(state->opens == 1 && state->starts == 1 && state->requests >= 1
	    && state->stops >= 1 && state->closes >= 1,
	    "meter lifecycle did not close its mock stream");
}

void
testMockOutputAndUnderrun()
{
	auto provider = std::make_shared<CountingProvider>();
	provider->response = makeDiagnosticBackend();
	auto state = std::make_shared<DiagnosticMockState>();
	AudioDiagnosticsService service(provider, [state] {
		return std::make_unique<DiagnosticMockAdapter>(state);
	});
	DiagnosticRequest request
	    = makeDiagnosticRequest(DiagnosticOperation::outputCalibration);
	const std::vector<float> expected = generateCalibrationSignal(
	    request.sampleRate, request.durationMs, request.levelDbfs);
	request.playbackRingCapacity = expected.size() + request.sampleRate / 10U;
	request.playbackPrefillFrames = request.playbackRingCapacity;
	std::optional<DiagnosticResult> result;
	std::jthread worker([&] { result = service.run(request); });
	waitForStart(state);
	state->pump(static_cast<std::uint32_t>(expected.size()));
	worker.join();
	require(result && std::holds_alternative<DiagnosticSnapshot>(*result),
	    "mock output calibration did not complete");
	for (std::size_t frame = 0; frame < expected.size(); ++frame) {
		require(state->output[frame * 2U] == 0.0F
		    && state->output[frame * 2U + 1U] == expected[frame],
		    "calibration samples reached the wrong output channel");
	}

	provider = std::make_shared<CountingProvider>();
	provider->response = makeDiagnosticBackend();
	state = std::make_shared<DiagnosticMockState>();
	AudioDiagnosticsService underrunService(provider, [state] {
		return std::make_unique<DiagnosticMockAdapter>(state);
	});
	request.playbackRingCapacity = 32;
	request.playbackPrefillFrames = 32;
	state->pumpFramesOnStart = 64;
	result = underrunService.run(request);
	require(result && std::holds_alternative<DiagnosticError>(*result)
	    && std::get<DiagnosticError>(*result).code == DiagnosticErrorCode::underrun,
	    "playback producer starvation did not fail safely");
}

void
testMockLoopbackAndDisconnect()
{
	auto provider = std::make_shared<CountingProvider>();
	provider->response = makeDiagnosticBackend();
	auto state = std::make_shared<DiagnosticMockState>();
	AudioDiagnosticsService service(provider, [state] {
		return std::make_unique<DiagnosticMockAdapter>(state);
	});
	DiagnosticRequest request = makeDiagnosticRequest(DiagnosticOperation::loopback);
	const std::vector<float> generated
	    = generateLoopbackSequence(request.sampleRate, request.levelDbfs);
	request.playbackRingCapacity = generated.size() + request.sampleRate / 10U;
	request.playbackPrefillFrames = request.playbackRingCapacity;
	constexpr std::size_t delay = 137;
	request.captureRingCapacity = generated.size() + delay;
	std::vector<float> input((generated.size() + delay) * 2U, 0.0F);
	for (std::size_t frame = 0; frame < generated.size(); ++frame) {
		input[(frame + delay) * 2U + 1U] = generated[frame] * -0.5F;
	}
	std::optional<DiagnosticResult> result;
	std::jthread worker([&] { result = service.run(request); });
	waitForStart(state);
	state->pump(static_cast<std::uint32_t>(generated.size() + delay), input);
	worker.join();
	require(result && std::holds_alternative<DiagnosticSnapshot>(*result),
	    "mock cable loopback did not complete");
	const DiagnosticSnapshot& snapshot = std::get<DiagnosticSnapshot>(*result);
	require(snapshot.loopback && snapshot.loopback->isConclusive
	    && snapshot.loopback->latencyFrames == delay
	    && snapshot.loopback->hasPolarityInversion,
	    "mock cable loopback result is incorrect");

	provider = std::make_shared<CountingProvider>();
	provider->response = makeDiagnosticBackend();
	state = std::make_shared<DiagnosticMockState>();
	AudioDiagnosticsService disconnectService(provider, [state] {
		return std::make_unique<DiagnosticMockAdapter>(state);
	});
	DiagnosticRequest meter = makeDiagnosticRequest(DiagnosticOperation::inputMeter);
	result.reset();
	std::jthread disconnectWorker([&] { result = disconnectService.run(meter); });
	waitForStart(state);
	state->disconnect();
	disconnectWorker.join();
	require(result && std::holds_alternative<DiagnosticError>(*result)
	    && std::get<DiagnosticError>(*result).code
	        == DiagnosticErrorCode::streamFailure
	    && state->closes >= 1,
	    "device removal did not fault and close without fallback");
}

void
testCancellationAndConcurrentStart()
{
	auto provider = std::make_shared<BlockingProvider>();
	std::atomic<std::size_t> adapterCreations{0};
	AudioDiagnosticsService service(provider, [&adapterCreations] {
		++adapterCreations;
		return std::unique_ptr<AudioStreamAdapter>();
	});
	const DiagnosticRequest request
	    = makeDiagnosticRequest(DiagnosticOperation::inputMeter);
	std::optional<DiagnosticResult> firstResult;
	std::jthread worker([&] { firstResult = service.run(request); });
	while (!provider->hasEntered.load(std::memory_order_acquire)) {
		std::this_thread::yield();
	}
	const DiagnosticResult concurrent = service.run(request);
	require(std::get<DiagnosticError>(concurrent).code == DiagnosticErrorCode::busy,
	    "concurrent diagnostic start was not rejected");
	service.requestStop();
	worker.join();
	require(firstResult && std::holds_alternative<DiagnosticError>(*firstResult)
	    && std::get<DiagnosticError>(*firstResult).code
	        == DiagnosticErrorCode::cancelled
	    && adapterCreations == 0,
	    "cancellation during discovery reached stream creation");
}

} // namespace

int
main()
{
	testDbfsAndCalibration();
	testLevels();
	testLoopbackAnalysis();
	testMockLoopbackAndDisconnect();
	testMockMeterAndCapturePolicies();
	testMockOutputAndUnderrun();
	testUnarmedGate();
	testRefreshAndExactSelection();
	testCancellationAndConcurrentStart();
	return 0;
}
