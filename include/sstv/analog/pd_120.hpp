// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/analog/pd_120.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/analog/paired_luma_chroma.hpp>
#include <sstv/core/rgb8_frame.hpp>
#include <sstv/core/timing.hpp>
#include <sstv/core/tone.hpp>

#include <cstdint>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace sstv::analog {

enum class Pd120Component : std::uint8_t {
	firstLuma,
	redDifference,
	blueDifference,
	secondLuma,
};

struct Pd120FixedToneSegment {
	core::Duration duration;
	double frequencyHz;
};

struct Pd120ScanSegment {
	Pd120Component component;
	core::Duration duration;
	std::uint16_t sampleWidth;
};

using Pd120Segment = std::variant<Pd120FixedToneSegment, Pd120ScanSegment>;

/** Immutable evidence-backed schedule for one paired-line PD120 scan group. */
struct Pd120Descriptor {
	std::string_view id;
	std::string_view displayName;
	std::uint8_t visCode;
	std::uint16_t width;
	std::uint16_t height;
	std::span<const Pd120FixedToneSegment> preImageSchedule;
	std::span<const Pd120Segment> pairSchedule;
	double blackHz;
	double whiteHz;
};

/** Convert nonlinear sRGB RGB8 into the frozen PD120 component definition. */
[[nodiscard]] LumaColourDifference440Frame convertPd120Colour(core::Rgb8View);

/** Encode one validated RGB8 frame into the complete PD120 event stream. */
[[nodiscard]] std::vector<core::ToneEvent> encodePd120(core::Rgb8View, float);

/** Return the immutable evidence-backed PD120 descriptor. */
[[nodiscard]] const Pd120Descriptor& pd120Descriptor();

/** Map one component code linearly between the PD120 video tone endpoints. */
[[nodiscard]] double pd120ComponentFrequency(std::uint8_t) noexcept;

/** Return the exact duration of a complete PD120 transmission including VIS. */
[[nodiscard]] core::Duration pd120TransmissionDuration();

} // namespace sstv::analog
