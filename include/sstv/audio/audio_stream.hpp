// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/audio/audio_stream.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/audio/audio_discovery.hpp>
#include <sstv/audio/sample_ring.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <variant>

namespace sstv::audio {

inline constexpr std::uint32_t nominalAudioSampleRate = 48'000;
inline constexpr std::uint32_t maximumAudioChannels = 64;
inline constexpr std::uint32_t maximumAudioPeriodFrames = 65'536;
inline constexpr std::uint32_t maximumAudioPeriodCount = 64;

enum class StreamDirection {
	playback,
	capture,
	duplex,
};

enum class PlaybackMappingPolicy {
	selectedChannel,
	duplicateAllChannels,
};

enum class StreamState {
	closed,
	opening,
	opened,
	primed,
	running,
	stopping,
	faulted,
};

enum class StreamFaultReason : std::uint32_t {
	none = 0,
	backendDisconnect,
	deviceRemoved,
	callbackContract,
	adapterFailure,
};

enum class StreamErrorCode {
	invalidConfiguration,
	invalidTransition,
	invalidIdentity,
	staleIdentity,
	identityCollision,
	deviceNotFound,
	insufficientPrefill,
	cancelled,
	adapterFailure,
	disconnected,
	resourceLimit,
};

struct StreamDeviceSelection {
	DeviceIdentity identity;
	std::uint64_t discoveryGeneration = 0;
};

struct AudioStreamConfiguration {
	StreamDirection direction = StreamDirection::playback;
	std::optional<StreamDeviceSelection> playbackDevice;
	std::optional<StreamDeviceSelection> captureDevice;
	std::uint32_t sampleRate = nominalAudioSampleRate;
	std::uint32_t playbackChannels = 0;
	std::uint32_t captureChannels = 0;
	std::uint32_t selectedPlaybackChannel = 0;
	std::uint32_t selectedCaptureChannel = 0;
	PlaybackMappingPolicy playbackMapping = PlaybackMappingPolicy::selectedChannel;
	std::uint32_t periodFrames = 480;
	std::uint32_t periodCount = 3;
	std::size_t playbackPrefillFrames = 0;
	std::size_t playbackRingCapacity = 4'800;
	std::size_t captureRingCapacity = 4'800;
};

struct NegotiatedEndpointFacts {
	SampleFormat callbackFormat = SampleFormat::float32;
	std::uint32_t callbackChannels = 0;
	SampleFormat nativeFormat = SampleFormat::unknown;
	std::uint32_t nativeChannels = 0;
	std::uint32_t nativeSampleRate = 0;
};

struct NegotiatedStreamFacts {
	AudioBackend backend = AudioBackend::nullDiagnostic;
	std::uint32_t callbackSampleRate = 0;
	std::uint32_t periodFrames = 0;
	std::uint32_t periodCount = 0;
	std::optional<NegotiatedEndpointFacts> playback;
	std::optional<NegotiatedEndpointFacts> capture;
};

struct StreamError {
	StreamErrorCode code;
	std::string operation;
	std::string message;
	bool isRecoverable = false;
};

struct AudioStreamStatistics {
	StreamState state = StreamState::closed;
	std::uint64_t callbacksExecuted = 0;
	std::uint64_t playbackFramesRequested = 0;
	std::uint64_t playbackFramesDelivered = 0;
	std::uint64_t playbackFramesZeroFilled = 0;
	std::uint64_t playbackFramesDiscardedByGate = 0;
	std::uint64_t underrunCallbacks = 0;
	std::uint64_t underrunFrames = 0;
	std::uint64_t captureFramesReceived = 0;
	std::uint64_t captureFramesWritten = 0;
	std::uint64_t captureFramesDropped = 0;
	std::uint64_t overrunCallbacks = 0;
	std::uint64_t overrunFrames = 0;
	std::uint64_t discontinuityGeneration = 0;
	std::size_t playbackRingFill = 0;
	std::size_t captureRingFill = 0;
	std::size_t playbackRingHighWater = 0;
	std::size_t captureRingHighWater = 0;
	std::uint64_t startCount = 0;
	std::uint64_t stopCount = 0;
	std::uint64_t faultCount = 0;
	StreamFaultReason faultReason = StreamFaultReason::none;
	std::optional<NegotiatedStreamFacts> negotiated;
	bool isPlaybackSignalGated = false;
};

struct AudioCallbackBinding {
	void* state = nullptr;
	void (*process)(void*, float*, const float*, std::uint32_t) noexcept = nullptr;
	void (*notifyFault)(void*, StreamFaultReason) noexcept = nullptr;
};

using AdapterOpenResult = std::variant<NegotiatedStreamFacts, StreamError>;
using StreamOperationResult = std::optional<StreamError>;

/** Backend device lifecycle interface used by AudioStream control-thread methods. */
class AudioStreamAdapter {
public:
	virtual ~AudioStreamAdapter() = default;
	[[nodiscard]] virtual AdapterOpenResult open(const AudioStreamConfiguration& configuration,
	    const AudioCallbackBinding& callback, std::stop_token stopToken) noexcept = 0;
	[[nodiscard]] virtual StreamOperationResult prime() noexcept = 0;
	[[nodiscard]] virtual StreamOperationResult start() noexcept = 0;
	[[nodiscard]] virtual StreamOperationResult requestStop() noexcept = 0;
	[[nodiscard]] virtual StreamOperationResult stop() noexcept = 0;
	[[nodiscard]] virtual StreamOperationResult close() noexcept = 0;
};

class AudioStream;
using AudioStreamCreateResult = std::variant<std::unique_ptr<AudioStream>, StreamError>;

/** Owns fixed audio rings, callback state, and one exact backend device lifecycle. */
class AudioStream {
public:
	~AudioStream();
	AudioStream(const AudioStream&) = delete;
	AudioStream& operator=(const AudioStream&) = delete;
	[[nodiscard]] StreamOperationResult open(const AudioDiscoverySnapshot& snapshot,
	    std::stop_token stopToken = {});
	[[nodiscard]] StreamOperationResult prime(std::stop_token stopToken = {});
	[[nodiscard]] StreamOperationResult start();
	[[nodiscard]] StreamOperationResult requestStop();
	[[nodiscard]] StreamOperationResult stop();
	[[nodiscard]] StreamOperationResult close();
	[[nodiscard]] StreamOperationResult resetRings();
	void gatePlaybackSignal() noexcept;
	[[nodiscard]] std::size_t queuePlayback(std::span<const float> samples) noexcept;
	[[nodiscard]] std::size_t readCapture(std::span<float> samples) noexcept;
	[[nodiscard]] StreamState poll() noexcept;
	[[nodiscard]] StreamState state() const noexcept;
	[[nodiscard]] AudioStreamStatistics statistics() const;
	[[nodiscard]] const AudioStreamConfiguration& configuration() const noexcept;

private:
	struct CallbackState;
	friend AudioStreamCreateResult createAudioStream(
	    AudioStreamConfiguration, std::unique_ptr<AudioStreamAdapter>);
	AudioStream(AudioStreamConfiguration configuration,
	    std::unique_ptr<AudioStreamAdapter> adapter);
	static void processCallback(void* state, float* output, const float* input,
	    std::uint32_t frameCount) noexcept;
	static void notifyCallbackFault(void* state, StreamFaultReason reason) noexcept;
	static void markAdapterFault(CallbackState& state) noexcept;
	[[nodiscard]] StreamOperationResult validateSnapshot(
	    const AudioDiscoverySnapshot& snapshot) const;
	[[nodiscard]] StreamOperationResult transitionError(
	    std::string operation, std::string message) const;

	AudioStreamConfiguration configuration_;
	FloatSpscRing playbackRing_;
	FloatSpscRing captureRing_;
	std::unique_ptr<CallbackState> callbackState_;
	std::unique_ptr<AudioStreamAdapter> adapter_;
	StreamState state_ = StreamState::closed;
	std::optional<NegotiatedStreamFacts> negotiated_;
};

[[nodiscard]] AudioStreamCreateResult createAudioStream(
    AudioStreamConfiguration configuration, std::unique_ptr<AudioStreamAdapter> adapter);
[[nodiscard]] std::unique_ptr<AudioStreamAdapter> createMiniaudioStreamAdapter();

} // namespace sstv::audio
