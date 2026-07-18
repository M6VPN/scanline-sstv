// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/app/rendered_transmit.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/analog/offline_tx.hpp>
#include <sstv/app/transmit_coordinator.hpp>
#include <sstv/audio/audio_stream.hpp>

#include <cstdint>
#include <memory>
#include <variant>

namespace sstv::app {

using FiniteSampleSourceCreateResult
	= std::variant<std::unique_ptr<FiniteSampleSource>, SampleSourceError>;

/** Create a bounded float source from the canonical prepared tone-event payload. */
[[nodiscard]] FiniteSampleSourceCreateResult createToneEventSampleSource(
	analog::OfflineTransmission, std::uint32_t);

struct AudioStreamTransmitEndpointRequest {
	audio::AudioStreamConfiguration configuration;
	std::shared_ptr<const audio::AudioDiscoverySnapshot> discovery;
};

using TransmitAudioEndpointCreateResult
	= std::variant<std::unique_ptr<TransmitAudioEndpoint>, TransmitAudioError>;

/** Create one exact playback endpoint over an injected M2B stream adapter. */
[[nodiscard]] TransmitAudioEndpointCreateResult createAudioStreamTransmitEndpoint(
	AudioStreamTransmitEndpointRequest,
	std::unique_ptr<audio::AudioStreamAdapter>);

} // namespace sstv::app
