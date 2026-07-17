// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/audio_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/audio/audio_discovery.hpp>

#include "miniaudio_adapter.hpp"
#include "audio_text.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

namespace {

void
require(const bool condition, const std::string& message)
{
	if (!condition) {
		throw std::runtime_error(message);
	}
}

[[nodiscard]] sstv::audio::AudioDevice
makeDevice(const sstv::audio::AudioBackend backend,
    const sstv::audio::AudioDirection direction, std::string identity,
    std::string name, const bool isDefault = false)
{
	return sstv::audio::AudioDevice{
	    {backend, direction, std::move(identity), sstv::audio::IdentityStability::persistent},
	    std::move(name), isDefault, sstv::audio::AudioTransport::unknown, {}, false, {}};
}

class FakeProvider final : public sstv::audio::AudioDiscoveryProvider {
public:
	std::map<sstv::audio::AudioBackend, sstv::audio::BackendDiscovery> responses;
	std::map<sstv::audio::AudioBackend, std::size_t> attempts;
	std::size_t forbiddenDeviceInitializations = 0;
	bool shouldBlock = false;
	bool hasEntered = false;
	bool isReleased = false;

	[[nodiscard]] bool isBackendCompiled(
	    const sstv::audio::AudioBackend backend) const override
	{
		return responses.contains(backend) && responses.at(backend).isCompiled;
	}

	[[nodiscard]] sstv::audio::BackendDiscovery discoverBackend(
	    const sstv::audio::AudioBackend backend, const std::stop_token stopToken) override
	{
		{
			std::unique_lock lock(mutex_);
			++attempts[backend];
			hasEntered = true;
			condition_.notify_all();
			condition_.wait(lock, [this, &stopToken] {
				return !shouldBlock || isReleased || stopToken.stop_requested();
			});
		}
		if (stopToken.stop_requested()) {
			return {backend, std::string(sstv::audio::audioBackendApiName(backend)),
			    true, sstv::audio::BackendStatus::cancelled, {}, "cancelled"};
		}
		return responses.at(backend);
	}

	void release()
	{
		std::lock_guard lock(mutex_);
		isReleased = true;
		condition_.notify_all();
	}

	void waitUntilEntered()
	{
		std::unique_lock lock(mutex_);
		condition_.wait(lock, [this] { return hasEntered; });
	}

private:
	std::mutex mutex_;
	std::condition_variable condition_;
};

[[nodiscard]] sstv::audio::BackendDiscovery
availableBackend(const sstv::audio::AudioBackend backend,
    std::vector<sstv::audio::AudioDevice> devices = {})
{
	return {backend, std::string(sstv::audio::audioBackendApiName(backend)), true,
	    sstv::audio::BackendStatus::available, std::move(devices), {}};
}

[[nodiscard]] sstv::audio::BackendDiscovery
failedBackend(const sstv::audio::AudioBackend backend)
{
	return {backend, std::string(sstv::audio::audioBackendApiName(backend)), true,
	    sstv::audio::BackendStatus::unavailable, {}, "mock failure"};
}

void
testMappingsAndSerialization()
{
	using sstv::audio::AudioBackend;
	require(sstv::audio::parseAudioBackend("pulseaudio") == AudioBackend::pulseAudio,
	    "PulseAudio parser mapping failed");
	require(sstv::audio::parseAudioBackend("audio4") == AudioBackend::audio4,
	    "audio(4) parser mapping failed");
	require(!sstv::audio::parseAudioBackend("null"),
	    "null must require the diagnostic include option");
	require(sstv::audio::audioBackendDisplayName(AudioBackend::jack) == "JACK",
	    "JACK display mapping failed");
	require(!sstv::audio::isRealAudioBackend(AudioBackend::nullDiagnostic),
	    "null backend was classified as hardware");
	ma_device_id identifier{};
	std::memcpy(identifier.pulse, "sink\nname", 10U);
	require(sstv::audio::detail::serializeDeviceId(AudioBackend::pulseAudio, identifier)
	        == "73696e6b0a6e616d65",
	    "PulseAudio ID serialization did not use initialized string bytes");
	identifier = {};
	std::memcpy(identifier.alsa, "hw:1,0", 7U);
	require(sstv::audio::detail::serializeDeviceId(AudioBackend::alsa, identifier)
	        == "68773a312c30",
	    "ALSA ID serialization is not deterministic");
	identifier = {};
	identifier.jack = 0;
	require(sstv::audio::detail::serializeDeviceId(AudioBackend::jack, identifier) == "0",
	    "JACK initialized integer ID serialization failed");
	identifier.nullbackend = 7;
	require(sstv::audio::detail::serializeDeviceId(
	        AudioBackend::nullDiagnostic, identifier) == "7",
	    "null initialized integer ID serialization failed");
	const std::string unsafeName("safe\n\x1b\xff", 7U);
	require(escapeAudioTerminalText(unsafeName) == "safe\\x0A\\x1B\\xFF",
	    "terminal control or invalid UTF-8 bytes were not escaped");
	require(escapeAudioTerminalText("Gr\xC3\xBCn") == "Gr\xC3\xBCn",
	    "valid UTF-8 device text was not preserved");
}

void
testPartialSuccessAndDevices()
{
	using namespace sstv::audio;
	auto provider = std::make_shared<FakeProvider>();
	DeviceCapabilities capabilities;
	capabilities.detailsKnown = true;
	capabilities.nativeFormats.push_back(
	    {SampleFormat::signed16, 2U, 48'000U, false});
	AudioDevice playback = makeDevice(AudioBackend::alsa, AudioDirection::playback,
	    "a", "duplicate name", true);
	playback.capabilities = capabilities;
	AudioDevice capture = makeDevice(AudioBackend::alsa, AudioDirection::capture,
	    "a", "duplicate name");
	provider->responses.emplace(AudioBackend::alsa,
	    availableBackend(AudioBackend::alsa, {playback, capture}));
	provider->responses.emplace(AudioBackend::jack, failedBackend(AudioBackend::jack));
	AudioDiscoveryService service(provider);
	const DiscoveryResult result = service.refresh(
	    {{AudioBackend::alsa, AudioBackend::jack}, false});
	const auto* snapshot = std::get_if<std::shared_ptr<const AudioDiscoverySnapshot>>(&result);
	require(snapshot != nullptr && (*snapshot)->hasRealSuccess,
	    "partial backend success was rejected");
	require((*snapshot)->backends.size() == 2U
	    && (*snapshot)->backends[0].devices.size() == 2U,
	    "capture and playback devices were not preserved");
	require((*snapshot)->backends[0].devices[0].identity
	        != (*snapshot)->backends[0].devices[1].identity,
	    "direction was omitted from device identity");
	require(provider->attempts[AudioBackend::alsa] == 1U
	    && provider->attempts[AudioBackend::jack] == 1U,
	    "a requested backend was not attempted exactly once");
	require(provider->forbiddenDeviceInitializations == 0U,
	    "discovery invoked a prohibited device operation");
}

void
testZeroDevicesAndCollisions()
{
	using namespace sstv::audio;
	auto provider = std::make_shared<FakeProvider>();
	provider->responses.emplace(AudioBackend::pulseAudio,
	    availableBackend(AudioBackend::pulseAudio));
	AudioDiscoveryService service(provider);
	require(std::holds_alternative<std::shared_ptr<const AudioDiscoverySnapshot>>(
	        service.refresh({{AudioBackend::pulseAudio}, false})),
	    "zero-device backend success was rejected");
	AudioDevice first = makeDevice(AudioBackend::pulseAudio, AudioDirection::playback,
	    "same", "one");
	AudioDevice second = makeDevice(AudioBackend::pulseAudio, AudioDirection::playback,
	    "same", "two");
	provider->responses[AudioBackend::pulseAudio]
	    = availableBackend(AudioBackend::pulseAudio, {first, second});
	const auto result = service.refresh({{AudioBackend::pulseAudio}, false});
	const auto snapshot = std::get<std::shared_ptr<const AudioDiscoverySnapshot>>(result);
	const auto& devices = snapshot->backends.front().devices;
	require(devices.size() == 2U && devices[0].hasIdentityCollision
	    && devices[1].hasIdentityCollision,
	    "duplicate identities were discarded or not marked");
	require(devices[0].identity.stability == IdentityStability::sessionOnly
	    && devices[1].identity.stability == IdentityStability::sessionOnly,
	    "colliding identities retained a persistence claim");
}

void
testPublicationFailuresAndCancellation()
{
	using namespace sstv::audio;
	auto provider = std::make_shared<FakeProvider>();
	provider->responses.emplace(AudioBackend::alsa, availableBackend(AudioBackend::alsa));
	provider->responses.emplace(AudioBackend::jack, failedBackend(AudioBackend::jack));
	AudioDiscoveryService service(provider);
	const auto successful = std::get<std::shared_ptr<const AudioDiscoverySnapshot>>(
	    service.refresh({{AudioBackend::alsa}, false}));
	provider->responses[AudioBackend::alsa] = failedBackend(AudioBackend::alsa);
	const DiscoveryResult failed = service.refresh({{AudioBackend::alsa}, false});
	require(std::holds_alternative<DiscoveryError>(failed)
	    && service.snapshot() == successful,
	    "total failure replaced the previous valid snapshot");
	std::stop_source stopSource;
	stopSource.request_stop();
	const DiscoveryResult cancelled = service.refresh(
	    {{AudioBackend::alsa}, false}, stopSource.get_token());
	require(std::get<DiscoveryError>(cancelled).code == DiscoveryErrorCode::cancelled
	    && service.snapshot() == successful,
	    "cancelled refresh replaced the previous snapshot");
	const DiscoveryResult invalid = service.refresh(
	    {{AudioBackend::alsa, AudioBackend::alsa}, false});
	require(std::get<DiscoveryError>(invalid).code == DiscoveryErrorCode::invalidRequest,
	    "duplicate requested backends were accepted");
	const DiscoveryResult uncompiled = service.refresh({{AudioBackend::oss}, false});
	const DiscoveryError& uncompiledError = std::get<DiscoveryError>(uncompiled);
	require(uncompiledError.attemptedSnapshot
	        && uncompiledError.attemptedSnapshot->backends.front().status
	            == BackendStatus::uncompiled
	        && provider->attempts[AudioBackend::oss] == 0U,
	    "uncompiled backend was attempted or not reported");
}

void
testConcurrentRefreshRejection()
{
	using namespace sstv::audio;
	auto provider = std::make_shared<FakeProvider>();
	provider->responses.emplace(AudioBackend::alsa, availableBackend(AudioBackend::alsa));
	provider->shouldBlock = true;
	AudioDiscoveryService service(provider);
	std::jthread worker([&service] {
		(void)service.refresh({{AudioBackend::alsa}, false});
	});
	provider->waitUntilEntered();
	const DiscoveryResult concurrent = service.refresh({{AudioBackend::alsa}, false});
	require(std::get<DiscoveryError>(concurrent).code
	        == DiscoveryErrorCode::refreshInProgress,
	    "concurrent refresh was not rejected");
	provider->release();
}

} // namespace

int
main()
{
	testMappingsAndSerialization();
	testPartialSuccessAndDevices();
	testZeroDevicesAndCollisions();
	testPublicationFailuresAndCancellation();
	testConcurrentRefreshRejection();
	return 0;
}
