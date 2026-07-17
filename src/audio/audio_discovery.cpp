// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/audio/audio_discovery.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/audio/audio_discovery.hpp>

#include <algorithm>
#include <array>
#include <set>
#include <utility>

namespace sstv::audio {
namespace {

constexpr std::size_t maximumRequestedBackends = 8;

class UnknownTransportClassifier final : public AudioTransportClassifier {
public:
	[[nodiscard]] AudioTransport classify(const AudioDevice& device) const override
	{
		if (device.identity.backend == AudioBackend::jack
		    || device.identity.backend == AudioBackend::nullDiagnostic) {
			return AudioTransport::virtualDevice;
		}
		return AudioTransport::unknown;
	}
};

[[nodiscard]] bool
hasDuplicateBackends(const std::vector<AudioBackend>& backends)
{
	std::set<AudioBackend> unique;
	return std::ranges::any_of(backends, [&unique](const AudioBackend backend) {
		return !unique.insert(backend).second;
	});
}

void
markIdentityCollisions(std::vector<AudioDevice>& devices)
{
	for (std::size_t first = 0; first < devices.size(); ++first) {
		for (std::size_t second = first + 1; second < devices.size(); ++second) {
			if (devices[first].identity.backend == devices[second].identity.backend
			    && devices[first].identity.direction == devices[second].identity.direction
			    && devices[first].identity.opaque == devices[second].identity.opaque) {
				devices[first].hasIdentityCollision = true;
				devices[second].hasIdentityCollision = true;
				devices[first].identity.stability = IdentityStability::sessionOnly;
				devices[second].identity.stability = IdentityStability::sessionOnly;
			}
		}
	}
}

} // namespace

AudioDiscoveryService::AudioDiscoveryService(
    std::shared_ptr<AudioDiscoveryProvider> provider,
    std::shared_ptr<const AudioTransportClassifier> classifier)
	: provider_(std::move(provider)), classifier_(std::move(classifier)),
	  snapshot_(std::make_shared<const AudioDiscoverySnapshot>())
{
	if (!classifier_) {
		classifier_ = std::make_shared<const UnknownTransportClassifier>();
	}
}

DiscoveryResult
AudioDiscoveryService::refresh(
    const DiscoveryRequest& request, const std::stop_token stopToken)
{
	std::unique_lock refreshLock(refreshMutex_, std::try_to_lock);
	if (!refreshLock.owns_lock()) {
		return DiscoveryError{DiscoveryErrorCode::refreshInProgress,
		    "audio discovery refresh is already in progress", {}};
	}
	if (!provider_) {
		return DiscoveryError{DiscoveryErrorCode::invalidRequest,
		    "audio discovery provider is unavailable", {}};
	}
	std::vector<AudioBackend> requested = request.backends.empty()
	    ? defaultAudioBackends() : request.backends;
	if (request.includeNull
	    && std::ranges::find(requested, AudioBackend::nullDiagnostic) == requested.end()) {
		requested.push_back(AudioBackend::nullDiagnostic);
	}
	if (requested.empty() || requested.size() > maximumRequestedBackends
	    || hasDuplicateBackends(requested)) {
		return DiscoveryError{DiscoveryErrorCode::invalidRequest,
		    "audio discovery request contains invalid or duplicate backends", {}};
	}
	auto attempted = std::make_shared<AudioDiscoverySnapshot>();
	for (const AudioBackend backend : requested) {
		if (stopToken.stop_requested()) {
			return DiscoveryError{DiscoveryErrorCode::cancelled,
			    "audio discovery was cancelled", attempted};
		}
		BackendDiscovery result;
		if (!provider_->isBackendCompiled(backend)) {
			result = BackendDiscovery{backend, std::string(audioBackendApiName(backend)),
			    false, BackendStatus::uncompiled, {}, "backend is not compiled"};
		} else {
			result = provider_->discoverBackend(backend, stopToken);
		}
		for (AudioDevice& device : result.devices) {
			device.transport = classifier_->classify(device);
		}
		markIdentityCollisions(result.devices);
		if (isRealAudioBackend(backend) && result.status == BackendStatus::available) {
			attempted->hasRealSuccess = true;
		}
		attempted->backends.push_back(std::move(result));
	}
	if (!attempted->hasRealSuccess) {
		return DiscoveryError{DiscoveryErrorCode::noBackendSucceeded,
		    "no requested real audio backend enumerated successfully", attempted};
	}
	{
		std::lock_guard snapshotLock(snapshotMutex_);
		attempted->generation = nextGeneration_++;
		snapshot_ = attempted;
	}
	return std::shared_ptr<const AudioDiscoverySnapshot>(std::move(attempted));
}

std::shared_ptr<const AudioDiscoverySnapshot>
AudioDiscoveryService::snapshot() const
{
	std::lock_guard snapshotLock(snapshotMutex_);
	return snapshot_;
}

std::vector<AudioBackend>
defaultAudioBackends()
{
#if defined(__linux__)
	return {AudioBackend::pulseAudio, AudioBackend::jack, AudioBackend::alsa};
#elif defined(__FreeBSD__)
	return {AudioBackend::oss};
#elif defined(__OpenBSD__)
	return {AudioBackend::sndio, AudioBackend::audio4};
#elif defined(__NetBSD__)
	return {AudioBackend::audio4};
#else
	return {};
#endif
}

std::optional<AudioBackend>
parseAudioBackend(const std::string_view value)
{
	constexpr std::array mappings{
	    std::pair{std::string_view{"pulseaudio"}, AudioBackend::pulseAudio},
	    std::pair{std::string_view{"jack"}, AudioBackend::jack},
	    std::pair{std::string_view{"alsa"}, AudioBackend::alsa},
	    std::pair{std::string_view{"oss"}, AudioBackend::oss},
	    std::pair{std::string_view{"sndio"}, AudioBackend::sndio},
	    std::pair{std::string_view{"audio4"}, AudioBackend::audio4}};
	const auto match = std::ranges::find(mappings, value, &decltype(mappings)::value_type::first);
	if (match == mappings.end()) {
		return std::nullopt;
	}
	return match->second;
}

std::string_view
audioBackendApiName(const AudioBackend backend)
{
	switch (backend) {
	case AudioBackend::pulseAudio: return "pulseaudio";
	case AudioBackend::jack: return "jack";
	case AudioBackend::alsa: return "alsa";
	case AudioBackend::oss: return "oss";
	case AudioBackend::sndio: return "sndio";
	case AudioBackend::audio4: return "audio4";
	case AudioBackend::nullDiagnostic: return "null";
	}
	return "unknown";
}

std::string_view
audioBackendDisplayName(const AudioBackend backend)
{
	switch (backend) {
	case AudioBackend::pulseAudio: return "PulseAudio";
	case AudioBackend::jack: return "JACK";
	case AudioBackend::alsa: return "ALSA";
	case AudioBackend::oss: return "OSS";
	case AudioBackend::sndio: return "sndio";
	case AudioBackend::audio4: return "audio(4)";
	case AudioBackend::nullDiagnostic: return "Null diagnostic";
	}
	return "Unknown";
}

bool
isRealAudioBackend(const AudioBackend backend)
{
	return backend != AudioBackend::nullDiagnostic;
}

} // namespace sstv::audio
