// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/audio_stream_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/audio/audio_stream.hpp>
#include <sstv/audio/sample_ring.hpp>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <memory>
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
	if (!condition) {
		throw std::runtime_error(message);
	}
}

struct MockState {
	AudioCallbackBinding callback;
	NegotiatedStreamFacts facts;
	std::vector<float> output;
	std::size_t opens = 0;
	std::size_t primes = 0;
	std::size_t starts = 0;
	std::size_t requests = 0;
	std::size_t stops = 0;
	std::size_t closes = 0;
	std::size_t forbiddenHardwareOrPttCalls = 0;
	bool failOpen = false;
	bool failPrime = false;
	bool failStart = false;
	bool failRequest = false;
	bool failStop = false;
	bool failClose = false;
	bool finalCallbackOnStop = false;
	std::uint32_t outputChannels = 0;
	std::uint32_t inputChannels = 0;

	void pump(const std::uint32_t frames, const std::vector<float>& input = {})
	{
		output.assign(static_cast<std::size_t>(frames) * outputChannels, 9.0F);
		const float* inputPointer = input.empty() ? nullptr : input.data();
		callback.process(callback.state, output.empty() ? nullptr : output.data(),
		    inputPointer, frames);
	}

	void fault(const StreamFaultReason reason)
	{
		callback.notifyFault(callback.state, reason);
	}
};

[[nodiscard]] StreamError
mockError(const std::string& operation)
{
	return {StreamErrorCode::adapterFailure, operation, "injected mock failure", true};
}

class MockAdapter final : public AudioStreamAdapter {
public:
	explicit MockAdapter(std::shared_ptr<MockState> state) : state_(std::move(state))
	{
	}

	[[nodiscard]] AdapterOpenResult open(const AudioStreamConfiguration&,
	    const AudioCallbackBinding& callback, const std::stop_token) noexcept override
	{
		++state_->opens;
		state_->callback = callback;
		return state_->failOpen ? AdapterOpenResult(mockError("open"))
		                        : AdapterOpenResult(state_->facts);
	}

	[[nodiscard]] StreamOperationResult prime() noexcept override
	{
		++state_->primes;
		return state_->failPrime ? StreamOperationResult(mockError("prime")) : std::nullopt;
	}

	[[nodiscard]] StreamOperationResult start() noexcept override
	{
		++state_->starts;
		return state_->failStart ? StreamOperationResult(mockError("start")) : std::nullopt;
	}

	[[nodiscard]] StreamOperationResult requestStop() noexcept override
	{
		++state_->requests;
		return state_->failRequest ? StreamOperationResult(mockError("request-stop"))
		                           : std::nullopt;
	}

	[[nodiscard]] StreamOperationResult stop() noexcept override
	{
		++state_->stops;
		if (state_->finalCallbackOnStop) {
			state_->pump(1, std::vector<float>(state_->inputChannels, 0.25F));
		}
		return state_->failStop ? StreamOperationResult(mockError("stop")) : std::nullopt;
	}

	[[nodiscard]] StreamOperationResult close() noexcept override
	{
		++state_->closes;
		state_->callback = {};
		return state_->failClose ? StreamOperationResult(mockError("close")) : std::nullopt;
	}

private:
	std::shared_ptr<MockState> state_;
};

[[nodiscard]] DeviceIdentity
makeIdentity(const AudioDirection direction, const std::string& opaque)
{
	return {AudioBackend::nullDiagnostic, direction, opaque, IdentityStability::sessionOnly};
}

[[nodiscard]] AudioDevice
makeDevice(const AudioDirection direction, const std::string& opaque,
	const bool collision = false)
{
	return {makeIdentity(direction, opaque), "mock", false, AudioTransport::virtualDevice,
	    {}, collision, {}};
}

[[nodiscard]] AudioDiscoverySnapshot
makeSnapshot(const bool playback = true, const bool capture = true)
{
	std::vector<AudioDevice> devices;
	if (playback) {
		devices.push_back(makeDevice(AudioDirection::playback, "0"));
	}
	if (capture) {
		devices.push_back(makeDevice(AudioDirection::capture, "0"));
	}
	return {7, {{AudioBackend::nullDiagnostic, "null", true,
	    BackendStatus::available, std::move(devices), {}}}, false};
}

[[nodiscard]] AudioStreamConfiguration
makeConfiguration(const StreamDirection direction)
{
	AudioStreamConfiguration configuration;
	configuration.direction = direction;
	configuration.periodFrames = 3;
	configuration.periodCount = 2;
	configuration.playbackRingCapacity = 8;
	configuration.captureRingCapacity = 8;
	if (direction != StreamDirection::capture) {
		configuration.playbackDevice = StreamDeviceSelection{
		    makeIdentity(AudioDirection::playback, "0"), 7};
		configuration.playbackChannels = 2;
		configuration.selectedPlaybackChannel = 1;
		configuration.playbackPrefillFrames = 2;
	}
	if (direction != StreamDirection::playback) {
		configuration.captureDevice = StreamDeviceSelection{
		    makeIdentity(AudioDirection::capture, "0"), 7};
		configuration.captureChannels = 2;
		configuration.selectedCaptureChannel = 1;
	}
	return configuration;
}

[[nodiscard]] std::shared_ptr<MockState>
makeMockState(const AudioStreamConfiguration& configuration)
{
	auto state = std::make_shared<MockState>();
	state->facts.backend = AudioBackend::nullDiagnostic;
	state->facts.callbackSampleRate = nominalAudioSampleRate;
	state->facts.periodFrames = configuration.periodFrames;
	state->facts.periodCount = configuration.periodCount;
	if (configuration.direction != StreamDirection::capture) {
		state->facts.playback = NegotiatedEndpointFacts{SampleFormat::float32,
		    configuration.playbackChannels, SampleFormat::float32,
		    configuration.playbackChannels, nominalAudioSampleRate};
		state->outputChannels = configuration.playbackChannels;
	}
	if (configuration.direction != StreamDirection::playback) {
		state->facts.capture = NegotiatedEndpointFacts{SampleFormat::float32,
		    configuration.captureChannels, SampleFormat::float32,
		    configuration.captureChannels, nominalAudioSampleRate};
		state->inputChannels = configuration.captureChannels;
	}
	return state;
}

[[nodiscard]] std::unique_ptr<AudioStream>
makeStream(const AudioStreamConfiguration& configuration,
	const std::shared_ptr<MockState>& state)
{
	AudioStreamCreateResult result = createAudioStream(
	    configuration, std::make_unique<MockAdapter>(state));
	require(std::holds_alternative<std::unique_ptr<AudioStream>>(result),
	    "valid mock stream configuration was rejected");
	return std::move(std::get<std::unique_ptr<AudioStream>>(result));
}

void
openPrimeStart(AudioStream& stream, const AudioDiscoverySnapshot& snapshot,
	const std::vector<float>& prefill = {0.1F, 0.2F})
{
	require(!stream.open(snapshot), "mock stream open failed");
	require(!stream.prime(), "mock stream prime failed");
	if (stream.configuration().direction != StreamDirection::capture) {
		require(stream.queuePlayback(prefill) == prefill.size(), "playback prefill failed");
	}
	require(!stream.start(), "mock stream start failed");
}

void
testRingBasics()
{
	for (const std::size_t capacity : {1U, 3U, 64U}) {
		FloatSpscRing ring(capacity);
		require(ring.capacity() == capacity && ring.availableRead() == 0,
		    "ring initial state is invalid");
		std::vector<float> values(capacity + 1U);
		for (std::size_t index = 0; index < values.size(); ++index) {
			values[index] = static_cast<float>(index);
		}
		require(ring.push(values) == capacity && ring.availableWrite() == 0,
		    "ring did not stop at exact capacity");
		std::vector<float> output(capacity + 1U, -1.0F);
		require(ring.pop(output) == capacity && ring.availableRead() == 0,
		    "ring exact-capacity pop failed");
		require(std::equal(values.begin(), std::next(values.begin(),
		        static_cast<std::ptrdiff_t>(capacity)), output.begin()),
		    "ring changed sample order");
		require(ring.pop(output) == 0, "empty ring pop was not deterministic");
		ring.reset();
	}
	FloatSpscRing ring(3);
	require(ring.push(std::vector<float>{1, 2}) == 2, "partial ring push failed");
	std::vector<float> output(1);
	require(ring.pop(output) == 1 && output[0] == 1, "partial ring pop failed");
	require(ring.push(std::vector<float>{3, 4}) == 2, "wrapped ring push failed");
	output.resize(3);
	require(ring.pop(output) == 3
	    && output == std::vector<float>({2, 3, 4}), "wrapped ring order failed");
	bool rejectedZero = false;
	try {
		FloatSpscRing invalid(0);
	} catch (const std::invalid_argument&) {
		rejectedZero = true;
	}
	require(rejectedZero, "zero-capacity ring was accepted");
	bool rejectedLarge = false;
	try {
		FloatSpscRing invalid(maximumAudioRingCapacity + 1U);
	} catch (const std::length_error&) {
		rejectedLarge = true;
	}
	require(rejectedLarge, "oversized ring was accepted");
}

void
testRingThreads()
{
	constexpr std::uint32_t count = 100'000;
	FloatSpscRing ring(127);
	std::atomic<bool> failed{false};
	std::thread producer([&ring] {
		for (std::uint32_t value = 0; value < count;) {
			const float sample = static_cast<float>(value);
			if (ring.push(std::span<const float>(&sample, 1)) == 1) {
				++value;
			} else {
				std::this_thread::yield();
			}
		}
	});
	std::thread consumer([&ring, &failed] {
		for (std::uint32_t expected = 0; expected < count;) {
			float sample = 0;
			if (ring.pop(std::span<float>(&sample, 1)) == 1) {
				if (sample != static_cast<float>(expected)) {
					failed.store(true, std::memory_order_relaxed);
				}
				++expected;
			} else {
				std::this_thread::yield();
			}
		}
	});
	producer.join();
	consumer.join();
	require(!failed.load(std::memory_order_relaxed),
	    "two-thread ring sequence changed");
}

void
testPlaybackAndCaptureCallbacks()
{
	AudioStreamConfiguration configuration = makeConfiguration(StreamDirection::duplex);
	configuration.playbackPrefillFrames = 0;
	configuration.captureRingCapacity = 3;
	auto mock = makeMockState(configuration);
	auto stream = makeStream(configuration, mock);
	openPrimeStart(*stream, makeSnapshot(), {});
	const std::vector<float> playback{1.0F, -2.0F, std::numeric_limits<float>::infinity()};
	require(stream->queuePlayback(playback) == playback.size(), "playback queue failed");
	mock->pump(5, {10, 11, 20, 21, 30, 31, 40, 41, 50, 51});
	require(mock->output == std::vector<float>({0, 1, 0, -2, 0,
	    std::numeric_limits<float>::infinity(), 0, 0, 0, 0}),
	    "selected playback channel or zero-fill policy failed");
	std::vector<float> capture(4, 0);
	require(stream->readCapture(capture) == 3
	    && std::vector<float>(capture.begin(), capture.begin() + 3)
	        == std::vector<float>({11, 21, 31}),
	    "capture selection or preserve-oldest overflow policy failed");
	const AudioStreamStatistics statistics = stream->statistics();
	require(statistics.playbackFramesDelivered == 3
	    && statistics.playbackFramesZeroFilled == 2
	    && statistics.underrunCallbacks == 1 && statistics.underrunFrames == 2,
	    "playback underrun counters are inconsistent");
	require(statistics.captureFramesReceived == 5
	    && statistics.captureFramesWritten == 3
	    && statistics.captureFramesDropped == 2
	    && statistics.overrunCallbacks == 1 && statistics.overrunFrames == 2,
	    "capture overrun counters are inconsistent");
	require(mock->forbiddenHardwareOrPttCalls == 0,
	    "mock reached a forbidden hardware or PTT operation");
	require(!stream->stop() && !stream->close(), "duplex teardown failed");

	configuration = makeConfiguration(StreamDirection::playback);
	configuration.playbackMapping = PlaybackMappingPolicy::duplicateAllChannels;
	configuration.playbackPrefillFrames = 0;
	mock = makeMockState(configuration);
	stream = makeStream(configuration, mock);
	openPrimeStart(*stream, makeSnapshot(true, false), {});
	require(stream->queuePlayback(std::vector<float>{0.5F}) == 1,
	    "duplicate playback queue failed");
	mock->pump(1);
	require(mock->output == std::vector<float>({0.5F, 0.5F}),
	    "explicit duplicate-all mapping failed");
	require(!stream->close(), "playback close failed");
}

void
testLifecycleAndFaults()
{
	AudioStreamConfiguration configuration = makeConfiguration(StreamDirection::playback);
	auto mock = makeMockState(configuration);
	auto stream = makeStream(configuration, mock);
	require(stream->start().has_value(), "start before prime was accepted");
	require(!stream->open(makeSnapshot(true, false)), "playback open failed");
	require(stream->open(makeSnapshot(true, false)).has_value(), "duplicate open was accepted");
	require(!stream->prime(), "playback prime failed");
	require(stream->start()->code == StreamErrorCode::insufficientPrefill,
	    "insufficient prefill was accepted");
	require(stream->queuePlayback(std::vector<float>{1, 2}) == 2,
	    "required prefill failed");
	require(!stream->start() && stream->state() == StreamState::running,
	    "playback start failed");
	require(stream->resetRings().has_value(), "running ring reset was accepted");
	mock->fault(StreamFaultReason::deviceRemoved);
	require(stream->poll() == StreamState::faulted,
	    "callback fault was not converted on the control thread");
	mock->finalCallbackOnStop = true;
	require(!stream->stop() && stream->state() == StreamState::opened,
	    "faulted stream stop failed");
	require(stream->statistics().callbacksExecuted == 1,
	    "final callback racing with stop lost its valid user data");
	require(!stream->stop() && !stream->close() && !stream->close(),
	    "idempotent stop or close failed");
	require(mock->opens == 1 && mock->primes == 1 && mock->starts == 1
	    && mock->stops == 1 && mock->closes == 1,
	    "adapter operation counts are inconsistent");

	configuration = makeConfiguration(StreamDirection::capture);
	mock = makeMockState(configuration);
	stream = makeStream(configuration, mock);
	openPrimeStart(*stream, makeSnapshot(false, true), {});
	require(stream->state() == StreamState::running,
	    "capture-only stream did not prime without playback data");
	mock->pump(0);
	require(stream->statistics().callbacksExecuted == 1,
	    "zero-frame callback was not bounded and counted");
	require(!stream->close(), "capture-only close failed");
}

void
testPlaybackSignalGate()
{
	AudioStreamConfiguration configuration = makeConfiguration(StreamDirection::playback);
	configuration.playbackPrefillFrames = 0;
	auto mock = makeMockState(configuration);
	auto stream = makeStream(configuration, mock);
	openPrimeStart(*stream, makeSnapshot(true, false), {});
	require(stream->queuePlayback(std::vector<float>{0.5F, -0.5F, 0.25F, -0.25F}) == 4,
	    "signal-gate fixture queue failed");
	mock->pump(2);
	require(mock->output == std::vector<float>({0.0F, 0.5F, 0.0F, -0.5F}),
	    "pre-gate playback changed samples");
	stream->gatePlaybackSignal();
	require(stream->queuePlayback(std::vector<float>{1.0F}) == 0,
	    "gated stream accepted new signal data");
	mock->pump(2);
	require(mock->output == std::vector<float>(4, 0.0F),
	    "queued signal escaped after the callback observed the gate");
	AudioStreamStatistics statistics = stream->statistics();
	require(statistics.isPlaybackSignalGated
	    && statistics.playbackFramesDiscardedByGate == 2,
	    "signal gate state or discarded-frame count is incorrect");
	require(!stream->stop(), "gated stream stop failed");
	require(!stream->resetRings(), "stopped gate reset failed");
	require(!stream->statistics().isPlaybackSignalGated,
	    "stopped ring reset did not rearm the signal path");
	require(stream->queuePlayback(std::vector<float>{0.75F}) == 1,
	    "rearmed stopped stream rejected playback");
	require(!stream->close(), "gated stream close failed");

	configuration = makeConfiguration(StreamDirection::playback);
	configuration.playbackPrefillFrames = 0;
	mock = makeMockState(configuration);
	stream = makeStream(configuration, mock);
	openPrimeStart(*stream, makeSnapshot(true, false), {});
	require(stream->queuePlayback(std::vector<float>{0.5F, 0.5F, 0.5F}) == 3,
	    "callback-race fixture queue failed");
	std::thread callback([&mock] { mock->pump(1); });
	stream->gatePlaybackSignal();
	callback.join();
	mock->pump(2);
	require(mock->output == std::vector<float>(4, 0.0F),
	    "signal escaped after the gate/callback race boundary");
	require(!stream->close(), "callback-race stream close failed");
}

void
testCancellationIdentityAndFailures()
{
	AudioStreamConfiguration configuration = makeConfiguration(StreamDirection::playback);
	auto mock = makeMockState(configuration);
	auto stream = makeStream(configuration, mock);
	std::stop_source stop;
	stop.request_stop();
	require(stream->open(makeSnapshot(true, false), stop.get_token())->code
	        == StreamErrorCode::cancelled
	    && mock->opens == 0, "opening cancellation reached the adapter");
	AudioDiscoverySnapshot stale = makeSnapshot(true, false);
	stale.generation = 8;
	require(stream->open(stale)->code == StreamErrorCode::staleIdentity,
	    "stale identity was accepted");
	AudioDiscoverySnapshot missing = makeSnapshot(false, false);
	require(stream->open(missing)->code == StreamErrorCode::deviceNotFound,
	    "missing device silently fell back");
	AudioDiscoverySnapshot collision = makeSnapshot(true, false);
	collision.backends[0].devices[0].hasIdentityCollision = true;
	require(stream->open(collision)->code == StreamErrorCode::identityCollision,
	    "colliding device identity was accepted");

	for (const int operation : {0, 1, 2}) {
		mock = makeMockState(configuration);
		mock->failOpen = operation == 0;
		mock->failPrime = operation == 1;
		mock->failStart = operation == 2;
		stream = makeStream(configuration, mock);
		const StreamOperationResult openResult = stream->open(makeSnapshot(true, false));
		if (operation == 0) {
			require(openResult.has_value() && stream->state() == StreamState::faulted,
			    "open failure did not fault the stream");
			continue;
		}
		require(!openResult, "setup open failed");
		const StreamOperationResult primeResult = stream->prime();
		if (operation == 1) {
			require(primeResult.has_value() && stream->state() == StreamState::faulted,
			    "prime failure did not fault the stream");
			continue;
		}
		require(!primeResult, "setup prime failed");
		require(stream->queuePlayback(std::vector<float>{1, 2}) == 2,
		    "failure-path prefill failed");
		require(stream->start().has_value() && stream->state() == StreamState::faulted,
		    "start failure did not fault the stream");
		require(stream->statistics().faultReason == StreamFaultReason::adapterFailure,
		    "adapter failure reason was absent from statistics");
	}

	for (const int operation : {3, 4, 5}) {
		mock = makeMockState(configuration);
		stream = makeStream(configuration, mock);
		openPrimeStart(*stream, makeSnapshot(true, false));
		mock->failRequest = operation == 3;
		mock->failStop = operation == 4;
		mock->failClose = operation == 5;
		if (operation == 3) {
			require(stream->requestStop().has_value(),
			    "request-stop failure was not returned");
		} else if (operation == 4) {
			require(!stream->requestStop() && stream->stop().has_value(),
			    "stop failure was not returned");
		} else {
			require(stream->close().has_value(), "close failure was not returned");
		}
		require(stream->state() == StreamState::faulted
		    && stream->statistics().faultReason == StreamFaultReason::adapterFailure,
		    "teardown failure did not remain typed and faulted");
	}

	AudioStreamConfiguration invalid = configuration;
	invalid.sampleRate = 44'100;
	require(std::holds_alternative<StreamError>(createAudioStream(
	        invalid, std::make_unique<MockAdapter>(makeMockState(invalid)))),
	    "unsupported internal sample rate was accepted");
	invalid = configuration;
	invalid.playbackDevice->identity.direction = AudioDirection::capture;
	require(std::get<StreamError>(createAudioStream(
	        invalid, std::make_unique<MockAdapter>(makeMockState(invalid)))).code
	        == StreamErrorCode::invalidIdentity,
	    "wrong-direction identity was accepted");
}

} // namespace

int
main()
{
	testRingBasics();
	testRingThreads();
	testPlaybackAndCaptureCallbacks();
	testPlaybackSignalGate();
	testLifecycleAndFaults();
	testCancellationIdentityAndFailures();
	return 0;
}
