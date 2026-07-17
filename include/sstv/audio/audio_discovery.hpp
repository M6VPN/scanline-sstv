// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/audio/audio_discovery.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sstv::audio {

enum class AudioBackend {
	pulseAudio,
	jack,
	alsa,
	oss,
	sndio,
	audio4,
	nullDiagnostic,
};

enum class AudioDirection {
	playback,
	capture,
};

enum class BackendStatus {
	available,
	unavailable,
	uncompiled,
	safeEnumerationUnsupported,
	cancelled,
};

enum class SampleFormat {
	unknown,
	unsigned8,
	signed16,
	signed24,
	signed32,
	float32,
};

enum class AudioTransport {
	usb,
	builtIn,
	virtualDevice,
	unknown,
};

enum class IdentityStability {
	persistent,
	sessionOnly,
};

enum class DiscoveryErrorCode {
	noBackendSucceeded,
	refreshInProgress,
	cancelled,
	invalidRequest,
};

struct DeviceIdentity {
	AudioBackend backend;
	AudioDirection direction;
	std::string opaque;
	IdentityStability stability;

	[[nodiscard]] bool operator==(const DeviceIdentity&) const = default;
};

struct NativeDataFormat {
	SampleFormat format = SampleFormat::unknown;
	std::optional<std::uint32_t> channels;
	std::optional<std::uint32_t> sampleRate;
	bool exclusive = false;

	[[nodiscard]] bool operator==(const NativeDataFormat&) const = default;
};

struct DeviceCapabilities {
	std::vector<NativeDataFormat> nativeFormats;
	std::optional<std::uint32_t> minimumChannels;
	std::optional<std::uint32_t> maximumChannels;
	std::optional<std::uint32_t> minimumSampleRate;
	std::optional<std::uint32_t> maximumSampleRate;
	bool detailsKnown = false;

	[[nodiscard]] bool operator==(const DeviceCapabilities&) const = default;
};

struct AudioDevice {
	DeviceIdentity identity;
	std::string name;
	bool isDefault = false;
	AudioTransport transport = AudioTransport::unknown;
	DeviceCapabilities capabilities;
	bool hasIdentityCollision = false;
	std::string diagnostic;

	[[nodiscard]] bool operator==(const AudioDevice&) const = default;
};

struct BackendDiscovery {
	AudioBackend backend;
	std::string apiName;
	bool isCompiled = false;
	BackendStatus status = BackendStatus::unavailable;
	std::vector<AudioDevice> devices;
	std::string diagnostic;

	[[nodiscard]] bool operator==(const BackendDiscovery&) const = default;
};

struct DiscoveryRequest {
	std::vector<AudioBackend> backends;
	bool includeNull = false;
};

struct AudioDiscoverySnapshot {
	std::uint64_t generation = 0;
	std::vector<BackendDiscovery> backends;
	bool hasRealSuccess = false;
};

struct DiscoveryError {
	DiscoveryErrorCode code;
	std::string message;
	std::shared_ptr<const AudioDiscoverySnapshot> attemptedSnapshot;
};

using DiscoveryResult = std::variant<std::shared_ptr<const AudioDiscoverySnapshot>, DiscoveryError>;

/** Supplies backend discovery data without exposing implementation-specific types. */
class AudioDiscoveryProvider {
public:
	virtual ~AudioDiscoveryProvider() = default;
	[[nodiscard]] virtual bool isBackendCompiled(AudioBackend backend) const = 0;
	[[nodiscard]] virtual BackendDiscovery discoverBackend(
	    AudioBackend backend, std::stop_token stopToken) = 0;
};

/** Optionally enriches transport metadata without changing device identity. */
class AudioTransportClassifier {
public:
	virtual ~AudioTransportClassifier() = default;
	[[nodiscard]] virtual AudioTransport classify(const AudioDevice& device) const = 0;
};

/** Serializes refreshes and atomically publishes complete immutable snapshots. */
class AudioDiscoveryService {
public:
	explicit AudioDiscoveryService(
	    std::shared_ptr<AudioDiscoveryProvider> provider,
	    std::shared_ptr<const AudioTransportClassifier> classifier = {});
	[[nodiscard]] DiscoveryResult refresh(
	    const DiscoveryRequest& request, std::stop_token stopToken = {});
	[[nodiscard]] std::shared_ptr<const AudioDiscoverySnapshot> snapshot() const;

private:
	std::shared_ptr<AudioDiscoveryProvider> provider_;
	std::shared_ptr<const AudioTransportClassifier> classifier_;
	mutable std::mutex snapshotMutex_;
	std::mutex refreshMutex_;
	std::shared_ptr<const AudioDiscoverySnapshot> snapshot_;
	std::uint64_t nextGeneration_ = 1;
};

[[nodiscard]] std::shared_ptr<AudioDiscoveryProvider> createMiniaudioDiscoveryProvider();
[[nodiscard]] std::vector<AudioBackend> defaultAudioBackends();
[[nodiscard]] std::optional<AudioBackend> parseAudioBackend(std::string_view value);
[[nodiscard]] std::string_view audioBackendApiName(AudioBackend backend);
[[nodiscard]] std::string_view audioBackendDisplayName(AudioBackend backend);
[[nodiscard]] bool isRealAudioBackend(AudioBackend backend);

} // namespace sstv::audio
