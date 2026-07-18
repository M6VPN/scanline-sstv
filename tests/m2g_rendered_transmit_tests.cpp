// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m2g_rendered_transmit_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/fsk_id.hpp>
#include <sstv/analog/offline_tx.hpp>
#include <sstv/app/rendered_transmit.hpp>
#include <sstv/core/mode.hpp>
#include <sstv/core/rgb8_frame.hpp>
#include <sstv/dsp/tone_renderer.hpp>
#include <sstv/offline/wav_writer.hpp>

#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace sstv;

void
require(const bool condition, const std::string& message)
{
	if (!condition) throw std::runtime_error(message);
}

[[nodiscard]] analog::OfflineTransmission
makeShortTransmission()
{
	std::vector<core::ToneEvent> events;
	events.emplace_back(core::Duration::fromMicroseconds(1'125), 1'200.0, 0.5F);
	events.emplace_back(core::Duration::fromMicroseconds(2'375), 1'900.0, 0.8F);
	events.emplace_back(core::Duration::fromMicroseconds(3'500), 2'300.0, 0.25F);
	return {std::move(events), core::Duration::fromMicroseconds(7'000)};
}

[[nodiscard]] std::vector<float>
readSource(analog::OfflineTransmission transmission, const std::size_t blockFrames)
{
	app::FiniteSampleSourceCreateResult result
		= app::createToneEventSampleSource(std::move(transmission), 48'000);
	require(std::holds_alternative<std::unique_ptr<app::FiniteSampleSource>>(result),
	    "valid tone-event source was rejected");
	auto source = std::get<std::unique_ptr<app::FiniteSampleSource>>(std::move(result));
	std::vector<float> output;
	output.reserve(static_cast<std::size_t>(source->facts().frameCount));
	std::vector<float> block(blockFrames);
	while (!source->isExhausted()) {
		const app::SampleReadResult read = source->read(block);
		require(std::holds_alternative<std::size_t>(read),
		    "valid source read returned an error");
		const std::size_t count = std::get<std::size_t>(read);
		require(count != 0, "source stopped before exhaustion");
		output.insert(output.end(), block.begin(),
			block.begin() + static_cast<std::ptrdiff_t>(count));
	}
	return output;
}

void
compareSourceWithRenderer(const analog::OfflineTransmission& transmission,
	const std::size_t blockFrames)
{
	app::FiniteSampleSourceCreateResult result
		= app::createToneEventSampleSource(transmission, 48'000);
	require(std::holds_alternative<std::unique_ptr<app::FiniteSampleSource>>(result),
	    "accepted transmission source creation failed");
	auto source = std::get<std::unique_ptr<app::FiniteSampleSource>>(std::move(result));
	dsp::ToneRenderer renderer(transmission.events, 48'000);
	require(source->facts().frameCount == renderer.frameCount(),
	    "stream and offline renderer frame counts differ");
	std::vector<float> sourceBlock(blockFrames);
	std::vector<float> directBlock(blockFrames);
	while (!source->isExhausted()) {
		const app::SampleReadResult read = source->read(sourceBlock);
		require(std::holds_alternative<std::size_t>(read),
		    "accepted transmission source read failed");
		const std::size_t sourceCount = std::get<std::size_t>(read);
		const std::size_t directCount = renderer.render(directBlock);
		require(sourceCount == directCount,
		    "stream and direct renderer block counts differ");
		require(std::equal(sourceBlock.begin(),
			sourceBlock.begin() + static_cast<std::ptrdiff_t>(sourceCount),
			directBlock.begin()),
		    "streamed float samples differ from ToneRenderer");
	}
	require(renderer.finished(), "direct renderer did not exhaust with source");
}

void
testStreamingSource()
{
	const analog::OfflineTransmission shortTransmission = makeShortTransmission();
	const std::vector<float> expected = readSource(shortTransmission, 4'096);
	const std::filesystem::path wavPath = std::filesystem::temp_directory_path()
		/ ("scanline-sstv-m2g-" + std::to_string(static_cast<long long>(::getpid()))
			+ ".wav");
	std::error_code removeError;
	std::filesystem::remove(wavPath, removeError);
	const offline::WavMetadata wav = offline::writePcm16WavAtomic(
		wavPath, shortTransmission.events, 48'000, false);
	require(wav.frameCount == expected.size(),
	    "streamed source and offline WAV frame counts differ");
	std::filesystem::remove(wavPath, removeError);
	for (const std::size_t blockFrames : {1U, 7U, 127U, 480U, 511U, 4'096U}) {
		require(readSource(shortTransmission, blockFrames) == expected,
		    "pull block size changed the streamed sample sequence");
	}
	for (const core::ModeDescriptor& mode : core::built_in_modes()) {
		if (!mode.capabilities.contains(core::ModeCapability::offlineTestPatternTx)) continue;
		require(!mode.capabilities.contains(core::ModeCapability::liveTx),
		    "M2G mode registry unexpectedly advertises live TX");
		const core::Rgb8Frame frame = core::makeDiagnosticPattern(mode.width, mode.height);
		for (const bool withFsk : {false, true}) {
			analog::OfflineTransmissionOptions options;
			if (withFsk) {
				analog::FskIdentifierResult identifier
					= analog::validateFskIdentifier("M6VPN-1");
				require(std::holds_alternative<analog::FskIdentifier>(identifier),
				    "FSK fixture identifier was rejected");
				options.fskIdentifier = std::get<analog::FskIdentifier>(std::move(identifier));
			}
			analog::OfflineTxResult encoded = analog::encodeOfflineTransmission(mode.id,
				core::ModeCapability::offlineTestPatternTx, frame.view(), options);
			require(std::holds_alternative<analog::OfflineTransmission>(encoded),
			    "accepted mode transmission encoding failed");
			compareSourceWithRenderer(
				std::get<analog::OfflineTransmission>(encoded), 4'096);
		}
	}

	auto cancelledResult = app::createToneEventSampleSource(shortTransmission, 48'000);
	auto cancelled = std::get<std::unique_ptr<app::FiniteSampleSource>>(
		std::move(cancelledResult));
	cancelled->cancel();
	std::vector<float> cancelledOutput(8, 9.0F);
	require(cancelled->isExhausted()
	    && std::get<std::size_t>(cancelled->read(cancelledOutput)) == 0
	    && std::ranges::all_of(cancelledOutput,
		    [](const float sample) { return sample == 9.0F; }),
	    "cancelled source produced additional samples");

	analog::OfflineTransmission mismatch = makeShortTransmission();
	mismatch.duration = core::Duration::fromMicroseconds(7'001);
	require(std::holds_alternative<app::SampleSourceError>(
	        app::createToneEventSampleSource(std::move(mismatch), 48'000)),
	    "mismatched transmission duration was accepted");
	analog::OfflineTransmission nyquist{{core::ToneEvent(
		core::Duration::fromMicroseconds(1'000), 24'000.0, 0.5F)},
		core::Duration::fromMicroseconds(1'000)};
	require(std::holds_alternative<app::SampleSourceError>(
	        app::createToneEventSampleSource(std::move(nyquist), 48'000)),
	    "Nyquist-invalid source was accepted");
	analog::OfflineTransmission oversized{{core::ToneEvent(
		core::Duration(601, 1), 1'000.0, 0.5F)}, core::Duration(601, 1)};
	require(std::holds_alternative<app::SampleSourceError>(
	        app::createToneEventSampleSource(std::move(oversized), 48'000)),
	    "source above the coordinator frame limit was accepted");
}

struct AdapterState {
	audio::AudioCallbackBinding callback;
	audio::NegotiatedStreamFacts facts;
	std::vector<std::string> operations;
	std::vector<float> output;
	bool isStarted = false;
	std::size_t forbiddenHardwareCalls = 0;

	void pump(const std::uint32_t frames)
	{
		output.assign(static_cast<std::size_t>(frames) * facts.playback->callbackChannels,
			9.0F);
		callback.process(callback.state, output.data(), nullptr, frames);
	}
	void fault(const audio::StreamFaultReason reason)
	{
		callback.notifyFault(callback.state, reason);
	}
};

class Adapter final : public audio::AudioStreamAdapter {
public:
	explicit Adapter(std::shared_ptr<AdapterState> state) : state_(std::move(state)) {}
	[[nodiscard]] audio::AdapterOpenResult open(
		const audio::AudioStreamConfiguration&, const audio::AudioCallbackBinding& callback,
		std::stop_token) noexcept override
	{
		state_->operations.push_back("open");
		state_->callback = callback;
		return state_->facts;
	}
	[[nodiscard]] audio::StreamOperationResult prime() noexcept override
	{
		state_->operations.push_back("prime");
		return std::nullopt;
	}
	[[nodiscard]] audio::StreamOperationResult start() noexcept override
	{
		state_->operations.push_back("start");
		state_->isStarted = true;
		return std::nullopt;
	}
	[[nodiscard]] audio::StreamOperationResult requestStop() noexcept override
	{
		state_->operations.push_back("request-stop");
		return std::nullopt;
	}
	[[nodiscard]] audio::StreamOperationResult stop() noexcept override
	{
		state_->operations.push_back("stop");
		state_->isStarted = false;
		return std::nullopt;
	}
	[[nodiscard]] audio::StreamOperationResult close() noexcept override
	{
		state_->operations.push_back("close");
		return std::nullopt;
	}

private:
	std::shared_ptr<AdapterState> state_;
};

[[nodiscard]] audio::DeviceIdentity
identity(const std::uint64_t suffix = 0)
{
	return {audio::AudioBackend::nullDiagnostic, audio::AudioDirection::playback,
		"device-" + std::to_string(suffix), audio::IdentityStability::sessionOnly};
}

[[nodiscard]] std::shared_ptr<const audio::AudioDiscoverySnapshot>
snapshot(const std::uint64_t generation = 7)
{
	audio::AudioDevice device{identity(), "mock", false,
		audio::AudioTransport::virtualDevice, {}, false, {}};
	return std::make_shared<audio::AudioDiscoverySnapshot>(audio::AudioDiscoverySnapshot{
		generation, {{audio::AudioBackend::nullDiagnostic, "null", true,
			audio::BackendStatus::available, {std::move(device)}, {}}}, false});
}

[[nodiscard]] audio::AudioStreamConfiguration
configuration()
{
	audio::AudioStreamConfiguration result;
	result.direction = audio::StreamDirection::playback;
	result.playbackDevice = audio::StreamDeviceSelection{identity(), 7};
	result.playbackChannels = 2;
	result.selectedPlaybackChannel = 1;
	result.periodFrames = 2;
	result.periodCount = 2;
	result.playbackPrefillFrames = 2;
	result.playbackRingCapacity = 4;
	return result;
}

[[nodiscard]] std::shared_ptr<AdapterState>
adapterState()
{
	auto state = std::make_shared<AdapterState>();
	state->facts = {audio::AudioBackend::nullDiagnostic, 48'000, 2, 2,
		audio::NegotiatedEndpointFacts{audio::SampleFormat::float32, 2,
			audio::SampleFormat::float32, 2, 48'000}, std::nullopt};
	return state;
}

void
testAudioEndpoint()
{
	auto state = adapterState();
	app::TransmitAudioEndpointCreateResult created
		= app::createAudioStreamTransmitEndpoint({configuration(), snapshot()},
			std::make_unique<Adapter>(state));
	require(std::holds_alternative<std::unique_ptr<app::TransmitAudioEndpoint>>(created),
	    "valid exact audio endpoint was rejected");
	auto endpoint = std::get<std::unique_ptr<app::TransmitAudioEndpoint>>(
		std::move(created));
	require(!endpoint->open(), "exact audio endpoint open failed");
	require(!endpoint->prefillSilence(2), "endpoint silence prefill failed");
	require(!endpoint->prime() && !endpoint->start(),
	    "endpoint prime/start failed");
	state->pump(2);
	require(state->output == std::vector<float>(4, 0.0F),
	    "initial callback output was not silent");
	const std::vector<float> signal{0.25F, -0.5F, 0.75F, -1.0F, 0.5F};
	require(endpoint->queue(signal) == 4, "endpoint did not report partial ring acceptance");
	require(endpoint->queue(std::span<const float>(signal.data() + 4, 1)) == 0,
	    "full endpoint did not report backpressure");
	state->pump(2);
	require(endpoint->queue(std::span<const float>(signal.data() + 4, 1)) == 1,
	    "endpoint did not resume the retained partial block");
	endpoint->gateSignal();
	state->pump(2);
	require(state->output == std::vector<float>(4, 0.0F),
	    "queued non-silent frames escaped after signal gating");
	require(endpoint->queue(std::span<const float>(signal.data(), 1)) == 0,
	    "gated endpoint accepted signal data");
	require(!endpoint->requestStop() && !endpoint->stop() && !endpoint->close(),
	    "endpoint teardown failed");
	require(state->operations == std::vector<std::string>({
		"open", "prime", "start", "request-stop", "stop", "close"}),
	    "endpoint lifecycle ordering changed");

	state = adapterState();
	created = app::createAudioStreamTransmitEndpoint({configuration(), snapshot(8)},
		std::make_unique<Adapter>(state));
	endpoint = std::get<std::unique_ptr<app::TransmitAudioEndpoint>>(
		std::move(created));
	require(endpoint->open().has_value() && state->operations.empty(),
	    "stale discovery reached the stream adapter");

	state = adapterState();
	state->facts.callbackSampleRate = 44'100;
	created = app::createAudioStreamTransmitEndpoint({configuration(), snapshot()},
		std::make_unique<Adapter>(state));
	endpoint = std::get<std::unique_ptr<app::TransmitAudioEndpoint>>(
		std::move(created));
	require(endpoint->open().has_value(),
	    "incompatible negotiated sample rate was accepted");

	state = adapterState();
	created = app::createAudioStreamTransmitEndpoint({configuration(), snapshot()},
		std::make_unique<Adapter>(state));
	endpoint = std::get<std::unique_ptr<app::TransmitAudioEndpoint>>(
		std::move(created));
	require(!endpoint->open() && !endpoint->prefillSilence(2)
	    && !endpoint->prime() && !endpoint->start(),
	    "fault-mapping endpoint setup failed");
	state->fault(audio::StreamFaultReason::deviceRemoved);
	require(endpoint->status().fault == app::TransmitAudioFault::deviceRemoved,
	    "device removal did not cross the transmit endpoint boundary");
	require(!endpoint->stop() && !endpoint->close(),
	    "faulted endpoint cleanup failed");

	audio::AudioStreamConfiguration duplex = configuration();
	duplex.direction = audio::StreamDirection::duplex;
	require(std::holds_alternative<app::TransmitAudioError>(
	        app::createAudioStreamTransmitEndpoint({duplex, snapshot()},
		        std::make_unique<Adapter>(adapterState()))),
	    "non-playback transmit endpoint was accepted");
}

class VirtualTime final : public rig::MonotonicClock,
	public rig::MonotonicScheduler {
public:
	explicit VirtualTime(std::shared_ptr<AdapterState> audioState)
		: audioState_(std::move(audioState)) {}
	[[nodiscard]] rig::MonotonicTime now() const noexcept override
	{
		return rig::MonotonicTime(nowNanoseconds_.load(std::memory_order_acquire));
	}
	[[nodiscard]] rig::WaitResult waitUntil(
		const rig::MonotonicTime deadline, std::stop_token) noexcept override
	{
		if (audioState_->isStarted && audioState_->callback.process != nullptr) {
			audioState_->pump(2);
		}
		nowNanoseconds_.store(deadline.count(), std::memory_order_release);
		return rig::WaitResult::deadlineReached;
	}
	void notify() noexcept override {}

private:
	std::shared_ptr<AdapterState> audioState_;
	std::atomic<std::int64_t> nowNanoseconds_{1};
};

class PttProvider final : public rig::PttProvider {
public:
	explicit PttProvider(std::shared_ptr<AdapterState> audioState)
		: audioState_(std::move(audioState)) {}
	[[nodiscard]] rig::PttOperationResult execute(
		const rig::PttRequest& request) noexcept override
	{
		operations.push_back(request.action);
		if (request.action == rig::PttAction::key && !audioState_->isStarted) {
			keyBeforeAudioStart = true;
		}
		if (request.action == rig::PttAction::key) {
			return {request.action, rig::PttObservedState::keyed,
				rig::PttCertainty::definitelyKeyed, rig::PttReadback::available,
				rig::PttErrorCategory::none, false, true, request.attempt,
				{}, {}, {}, {}, request.operationId};
		}
		return {request.action, rig::PttObservedState::unkeyed,
			rig::PttCertainty::definitelyUnkeyed, rig::PttReadback::available,
			rig::PttErrorCategory::none, false, false, request.attempt,
			{}, {}, {}, {}, request.operationId};
	}
	std::vector<rig::PttAction> operations;
	bool keyBeforeAudioStart = false;

private:
	std::shared_ptr<AdapterState> audioState_;
};

class Watchdog final : public rig::PttWatchdogControl {
public:
	[[nodiscard]] bool arm(std::chrono::milliseconds) noexcept override
	{
		isArmed_ = true;
		return true;
	}
	[[nodiscard]] bool heartbeat() noexcept override { return isArmed_; }
	void finish(rig::PttCertainty, bool) noexcept override { isArmed_ = false; }
	[[nodiscard]] bool isArmed() const noexcept override { return isArmed_; }
	[[nodiscard]] bool hasExpired() const noexcept override { return false; }

private:
	bool isArmed_ = false;
};

class WatchdogFactory final : public app::TransmitWatchdogFactory {
public:
	[[nodiscard]] std::unique_ptr<rig::PttWatchdogControl> create(
		std::shared_ptr<rig::PttSupervisor>, std::shared_ptr<rig::MonotonicClock>,
		std::shared_ptr<rig::MonotonicScheduler>,
		std::shared_ptr<rig::PttSafetyRecord>, rig::PttCleanupPolicy) override
	{
		return std::make_unique<Watchdog>();
	}
};

void
testCoordinatorIntegration()
{
	auto state = adapterState();
	audio::AudioStreamConfiguration streamConfiguration = configuration();
	streamConfiguration.playbackRingCapacity = 16;
	app::TransmitAudioEndpointCreateResult endpointResult
		= app::createAudioStreamTransmitEndpoint(
			{streamConfiguration, snapshot()}, std::make_unique<Adapter>(state));
	auto endpoint = std::get<std::unique_ptr<app::TransmitAudioEndpoint>>(
		std::move(endpointResult));
	app::FiniteSampleSourceCreateResult sourceResult
		= app::createToneEventSampleSource(makeShortTransmission(), 48'000);
	auto source = std::get<std::unique_ptr<app::FiniteSampleSource>>(
		std::move(sourceResult));
	auto time = std::make_shared<VirtualTime>(state);
	auto provider = std::make_shared<PttProvider>(state);
	auto supervisor = std::make_shared<rig::PttSupervisor>(provider, time);
	app::TransmitCoordinator coordinator(supervisor, time, time,
		std::make_shared<WatchdogFactory>());
	app::TransmitRequest request;
	request.prefillFrames = 4;
	request.blockFrames = 7;
	request.policy.preKeyDelay = std::chrono::milliseconds(0);
	request.policy.postAudioTail = std::chrono::milliseconds(0);
	request.policy.retryDelay = std::chrono::milliseconds(0);
	const app::TransmitRunResult result = coordinator.run(
		request, std::move(source), std::move(endpoint));
	require(result->outcome == app::TransmitOutcome::completed,
	    "rendered-source coordinator path did not complete");
	require(result->submittedFrames == result->sourceFrames,
	    "coordinator duplicated or omitted rendered source frames");
	require(!provider->keyBeforeAudioStart,
	    "PTT key preceded open, prime, and audio start");
	require(provider->operations == std::vector<rig::PttAction>({
		rig::PttAction::query, rig::PttAction::key, rig::PttAction::unkey}),
	    "coordinator PTT ordering changed");
	require(state->forbiddenHardwareCalls == 0,
	    "deterministic coordinator test reached a hardware operation");
}

} // namespace

int
main()
{
	testStreamingSource();
	testAudioEndpoint();
	testCoordinatorIntegration();
	return 0;
}
