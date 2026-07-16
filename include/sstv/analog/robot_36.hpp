// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/analog/robot_36.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/analog/luma_chroma.hpp>
#include <sstv/core/rgb8_frame.hpp>
#include <sstv/core/timing.hpp>
#include <sstv/core/tone.hpp>

#include <cstdint>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace sstv::analog {

enum class Robot36Component : std::uint8_t {
	lumaY,
	redDifference,
	blueDifference,
};

struct Robot36FixedToneSegment {
	core::Duration duration;
	double frequencyHz;
};

struct Robot36ScanSegment {
	Robot36Component component;
	core::Duration duration;
};

using Robot36Segment = std::variant<Robot36FixedToneSegment, Robot36ScanSegment>;

/** Immutable evidence-backed alternating luma and colour-difference schedule. */
struct Robot36Descriptor {
	std::string_view id;
	std::string_view displayName;
	std::uint8_t visCode;
	std::uint16_t width;
	std::uint16_t height;
	std::span<const Robot36FixedToneSegment> preImageSchedule;
	std::span<const Robot36Segment> evenLineSchedule;
	std::span<const Robot36Segment> oddLineSchedule;
	double blackHz;
	double whiteHz;
};

/** Convert nonlinear sRGB RGB8 into the frozen Robot 36 component definition. */
[[nodiscard]] LumaColourDifference420Frame convertRobot36Colour(core::Rgb8View);

/** Encode one validated RGB8 frame into the complete Robot 36 event stream. */
[[nodiscard]] std::vector<core::ToneEvent> encodeRobot36(core::Rgb8View, float);

/** Return the immutable evidence-backed Robot 36 descriptor. */
[[nodiscard]] const Robot36Descriptor& robot36Descriptor();

/** Map one component code linearly between the Robot video tone endpoints. */
[[nodiscard]] double robot36ComponentFrequency(std::uint8_t) noexcept;

/** Return the exact duration of a complete Robot 36 transmission including VIS. */
[[nodiscard]] core::Duration robot36TransmissionDuration();

} // namespace sstv::analog
