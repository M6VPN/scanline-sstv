// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/analog/sequential_rgb.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/core/rgb8_frame.hpp>
#include <sstv/core/timing.hpp>
#include <sstv/core/tone.hpp>

#include <cstdint>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace sstv::analog {

enum class RgbChannel : std::uint8_t {
	red,
	green,
	blue,
};

/** One fixed-frequency interval in a sequential RGB schedule. */
struct FixedToneSegment {
	core::Duration duration;
	double frequencyHz;
};

/** One complete RGB component scan in a sequential RGB schedule. */
struct RgbScanSegment {
	RgbChannel channel;
	core::Duration duration;
};

using SequentialRgbSegment = std::variant<FixedToneSegment, RgbScanSegment>;

/** Immutable evidence-backed schedule for one sequential RGB mode. */
struct SequentialRgbDescriptor {
	std::string_view id;
	std::string_view displayName;
	std::uint8_t visCode;
	std::uint16_t width;
	std::uint16_t height;
	std::span<const FixedToneSegment> preImageSchedule;
	std::span<const SequentialRgbSegment> firstLineSchedule;
	std::span<const SequentialRgbSegment> lineSchedule;
	double blackHz;
	double whiteHz;
};

/** Encode a validated RGB8 frame with the descriptor's complete schedule. */
[[nodiscard]] std::vector<core::ToneEvent> encodeSequentialRgb(
	const SequentialRgbDescriptor&, core::Rgb8View, float);

/** Map one RGB8 component linearly between a descriptor's endpoint tones. */
[[nodiscard]] double sequentialRgbPixelFrequency(
	const SequentialRgbDescriptor&, std::uint8_t) noexcept;

/** Return the exact duration of the complete VIS and image schedule. */
[[nodiscard]] core::Duration sequentialRgbTransmissionDuration(
	const SequentialRgbDescriptor&);

} // namespace sstv::analog
