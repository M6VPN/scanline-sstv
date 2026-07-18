// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/app/live_transmit.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/app/live_transmit.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace sstv::app {
namespace {

[[nodiscard]] LiveTransmitError
makeError(const LiveTransmitErrorCode code, std::string operation,
	std::string message)
{
	return {code, std::move(operation), std::move(message)};
}

[[nodiscard]] std::variant<LiveTransmitGain, LiveTransmitError>
validateGain(const double decibels,
	const std::span<const core::ToneEvent> events)
{
	if (!std::isfinite(decibels) || decibels < minimumLiveTransmitGainDbfs
		|| decibels > maximumLiveTransmitGainDbfs) {
		return makeError(LiveTransmitErrorCode::invalidGain, "validate gain",
			"transmit gain must be finite and within -60 to -6 dBFS");
	}
	const double calculated = std::pow(10.0, decibels / 20.0);
	if (!std::isfinite(calculated) || calculated <= 0.0
		|| calculated > static_cast<double>(std::numeric_limits<float>::max())) {
		return makeError(LiveTransmitErrorCode::invalidGain, "validate gain",
			"transmit gain cannot be represented safely");
	}
	const float scalar = static_cast<float>(calculated);
	for (const core::ToneEvent& event : events) {
		if (!std::isfinite(event.amplitude())
			|| std::abs(event.amplitude()) * scalar > 1.0F) {
			return makeError(LiveTransmitErrorCode::invalidGain, "validate gain",
				"transmit gain can produce clipped or non-finite samples");
		}
	}
	return LiveTransmitGain{decibels, scalar};
}

} // namespace

LiveTransmitPreparationResult
prepareLiveTransmit(const LiveTransmitPreparationRequest& request)
{
	SnapshotResult result = prepareOfflineEditor(request.editor);
	if (const auto* error = std::get_if<EditorError>(&result)) {
		return makeError(LiveTransmitErrorCode::invalidPreparation,
			error->operation, error->message);
	}
	auto snapshot = std::get<std::shared_ptr<const OfflineEditorSnapshot>>(
		std::move(result));
	auto gain = validateGain(request.gainDecibelsFullScale,
		snapshot->transmission.events);
	if (const auto* error = std::get_if<LiveTransmitError>(&gain)) return *error;
	if (snapshot->frameCount == 0
		|| snapshot->frameCount > maximumTransmitSourceFrames) {
		return makeError(LiveTransmitErrorCode::invalidPreparation,
			"validate frame count", "prepared transmission exceeds live limits");
	}
	return LiveTransmitPrepared{std::move(snapshot),
		std::get<LiveTransmitGain>(gain)};
}

LivePlaybackSelectionResult
selectLivePlaybackDevice(const audio::AudioDiscoverySnapshot& snapshot,
	const LivePlaybackSelectionRequest& request)
{
	if (!audio::isRealAudioBackend(request.backend)) {
		return makeError(LiveTransmitErrorCode::invalidDevice, "select device",
			"live transmission requires a real explicitly selected backend");
	}
	if (request.opaqueIdentity.empty() || request.playbackChannels == 0
		|| request.playbackChannels > audio::maximumAudioChannels
		|| request.selectedOutputChannel >= request.playbackChannels) {
		return makeError(LiveTransmitErrorCode::invalidDevice, "select device",
			"playback identity, channel count, or output channel is invalid");
	}
	const audio::AudioDevice* selected = nullptr;
	std::size_t matches = 0;
	for (const audio::BackendDiscovery& backend : snapshot.backends) {
		if (backend.backend != request.backend) continue;
		for (const audio::AudioDevice& device : backend.devices) {
			if (device.identity.direction != audio::AudioDirection::playback
				|| device.identity.opaque != request.opaqueIdentity) continue;
			selected = &device;
			++matches;
		}
	}
	if (matches == 0 || selected == nullptr) {
		return makeError(LiveTransmitErrorCode::deviceNotFound, "select device",
			"the exact playback identity was not found in the fresh snapshot");
	}
	if (matches != 1 || selected->hasIdentityCollision) {
		return makeError(LiveTransmitErrorCode::identityCollision, "select device",
			"the playback identity is duplicated or collision-marked");
	}
	audio::AudioStreamConfiguration configuration;
	configuration.direction = audio::StreamDirection::playback;
	configuration.playbackDevice = audio::StreamDeviceSelection{
		selected->identity, snapshot.generation};
	configuration.sampleRate = audio::nominalAudioSampleRate;
	configuration.playbackChannels = request.playbackChannels;
	configuration.selectedPlaybackChannel = request.selectedOutputChannel;
	configuration.playbackMapping = audio::PlaybackMappingPolicy::selectedChannel;
	configuration.periodFrames = 480;
	configuration.periodCount = 3;
	configuration.playbackPrefillFrames = 1'440;
	configuration.playbackRingCapacity = 4'800;
	return configuration;
}

FiniteSampleSourceCreateResult
createLiveTransmitSource(const LiveTransmitPrepared& prepared)
{
	if (prepared.snapshot == nullptr) {
		return SampleSourceError{"prepared live transmission is missing"};
	}
	return createToneEventSampleSource(prepared.snapshot->transmission,
		prepared.snapshot->sampleRate, prepared.gain.scalar);
}

} // namespace sstv::app
