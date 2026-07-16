// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/analog/vis.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/core/tone.hpp>

#include <array>
#include <cstdint>

namespace sstv::analog {

using VisBits = std::array<bool, 8>;
using VisHeader = std::array<core::ToneEvent, 13>;

/** Build seven LSB-first data bits followed by the even-parity bit. */
[[nodiscard]] VisBits makeVisBits(std::uint8_t);

/** Build leader, break, second leader, start, data, parity, and stop events. */
[[nodiscard]] VisHeader makeVisHeader(std::uint8_t, float);

} // namespace sstv::analog
