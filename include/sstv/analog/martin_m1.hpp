// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/analog/martin_m1.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/analog/sequential_rgb.hpp>

#include <cstdint>
#include <vector>

namespace sstv::analog {

/** Return the immutable evidence-backed Martin M1 descriptor. */
[[nodiscard]] const SequentialRgbDescriptor& martinM1Descriptor();

/** Map one RGB8 component linearly between the descriptor's black and white tones. */
[[nodiscard]] double martinM1PixelFrequency(std::uint8_t) noexcept;

/** Return the exact duration of a complete Martin M1 transmission including VIS. */
[[nodiscard]] core::Duration martinM1TransmissionDuration();

/** Encode a validated 320 by 256 RGB8 frame into the full Martin M1 event stream. */
[[nodiscard]] std::vector<core::ToneEvent> encodeMartinM1(core::Rgb8View, float);

} // namespace sstv::analog
