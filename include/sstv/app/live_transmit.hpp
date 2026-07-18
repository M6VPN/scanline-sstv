// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/app/live_transmit.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/app/offline_tx_editor.hpp>
#include <sstv/app/rendered_transmit.hpp>
#include <sstv/audio/audio_discovery.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <variant>

namespace sstv::app {

inline constexpr double minimumLiveTransmitGainDbfs = -60.0;
inline constexpr double maximumLiveTransmitGainDbfs = -6.0;

enum class LiveTransmitErrorCode {
	invalidPreparation,
	invalidGain,
	invalidDevice,
	deviceNotFound,
	identityCollision,
};

struct LiveTransmitError {
	LiveTransmitErrorCode code;
	std::string operation;
	std::string message;
};

struct LiveTransmitGain {
	double decibelsFullScale;
	float scalar;
};

struct LiveTransmitPreparationRequest {
	OfflineEditorRequest editor;
	double gainDecibelsFullScale;
};

struct LiveTransmitPrepared {
	std::shared_ptr<const OfflineEditorSnapshot> snapshot;
	LiveTransmitGain gain;
};

struct LivePlaybackSelectionRequest {
	audio::AudioBackend backend;
	std::string opaqueIdentity;
	std::uint32_t playbackChannels;
	std::uint32_t selectedOutputChannel;
};

using LiveTransmitPreparationResult
	= std::variant<LiveTransmitPrepared, LiveTransmitError>;
using LivePlaybackSelectionResult
	= std::variant<audio::AudioStreamConfiguration, LiveTransmitError>;

/** Prepare and validate one immutable live-transmit payload before acquisition. */
[[nodiscard]] LiveTransmitPreparationResult prepareLiveTransmit(
	const LiveTransmitPreparationRequest&);

/** Resolve one exact playback identity from one fresh discovery snapshot. */
[[nodiscard]] LivePlaybackSelectionResult selectLivePlaybackDevice(
	const audio::AudioDiscoverySnapshot&, const LivePlaybackSelectionRequest&);

/** Create the constant-gain float source for a validated prepared payload. */
[[nodiscard]] FiniteSampleSourceCreateResult createLiveTransmitSource(
	const LiveTransmitPrepared&);

} // namespace sstv::app
