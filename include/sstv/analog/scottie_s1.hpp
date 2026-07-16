// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/analog/scottie_s1.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/analog/sequential_rgb.hpp>

#include <cstdint>
#include <vector>

namespace sstv::analog {

/** Return the immutable evidence-backed Scottie S1 descriptor. */
[[nodiscard]] const SequentialRgbDescriptor& scottieS1Descriptor();

/** Map one RGB8 component to the Scottie S1 video tone range. */
[[nodiscard]] double scottieS1PixelFrequency(std::uint8_t) noexcept;

/** Return the exact duration of a complete Scottie S1 transmission including VIS. */
[[nodiscard]] core::Duration scottieS1TransmissionDuration();

/** Encode a validated 320 by 256 RGB8 frame into the full Scottie S1 event stream. */
[[nodiscard]] std::vector<core::ToneEvent> encodeScottieS1(core::Rgb8View, float);

} // namespace sstv::analog
