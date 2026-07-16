// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/robot_36.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/robot_36.hpp>

#include <sstv/analog/vis.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace sstv::analog {
namespace {

using core::Duration;

constexpr std::int64_t conversionDenominator = 1'000'000'000;
constexpr std::int64_t lumaOffset = 16 * conversionDenominator;
constexpr std::int64_t differenceOffset = 128 * conversionDenominator;
constexpr std::array<std::int64_t, 3> lumaWeights{
	256'772'628, 504'096'642, 97'899'984,
};
constexpr std::array<std::int64_t, 3> redDifferenceWeights{
	439'186'734, -367'765'524, -71'421'210,
};
constexpr std::array<std::int64_t, 3> blueDifferenceWeights{
	-148'213'170, -290'973'564, 439'186'734,
};

const std::array<Robot36FixedToneSegment, 0> preImageSchedule{};

const std::array<Robot36Segment, 6> evenLineSchedule{{
	Robot36FixedToneSegment{Duration::fromMicroseconds(9'000), 1'200.0},
	Robot36FixedToneSegment{Duration::fromMicroseconds(3'000), 1'500.0},
	Robot36ScanSegment{Robot36Component::lumaY,
	    Duration::fromMicroseconds(88'000)},
	Robot36FixedToneSegment{Duration::fromMicroseconds(4'500), 1'500.0},
	Robot36FixedToneSegment{Duration::fromMicroseconds(1'500), 1'900.0},
	Robot36ScanSegment{Robot36Component::redDifference,
	    Duration::fromMicroseconds(44'000)},
}};

const std::array<Robot36Segment, 6> oddLineSchedule{{
	Robot36FixedToneSegment{Duration::fromMicroseconds(9'000), 1'200.0},
	Robot36FixedToneSegment{Duration::fromMicroseconds(3'000), 1'500.0},
	Robot36ScanSegment{Robot36Component::lumaY,
	    Duration::fromMicroseconds(88'000)},
	Robot36FixedToneSegment{Duration::fromMicroseconds(4'500), 2'300.0},
	Robot36FixedToneSegment{Duration::fromMicroseconds(1'500), 1'900.0},
	Robot36ScanSegment{Robot36Component::blueDifference,
	    Duration::fromMicroseconds(44'000)},
}};

const Robot36Descriptor descriptor{
	"robot-36",
	"Robot 36",
	8,
	320,
	240,
	preImageSchedule,
	evenLineSchedule,
	oddLineSchedule,
	1'500.0,
	2'300.0,
};

[[nodiscard]] std::uint8_t
convertComponent(const core::Rgb8Pixel& pixel,
	const std::array<std::int64_t, 3>& weights, const std::int64_t offset)
{
	/* Dayton Appendix B decimals are exact rationals over 1,000,000,000. */
	std::int64_t numerator = offset;
	numerator += weights[0] * pixel.red;
	numerator += weights[1] * pixel.green;
	numerator += weights[2] * pixel.blue;
	const std::int64_t maximum = 255 * conversionDenominator;
	if (numerator <= 0) {
		return 0;
	}
	if (numerator >= maximum) {
		return 255;
	}
	return static_cast<std::uint8_t>(numerator / conversionDenominator);
}

[[nodiscard]] std::uint8_t
convertBlueDifference(const core::Rgb8Pixel& pixel)
{
	return convertComponent(pixel, blueDifferenceWeights, differenceOffset);
}

[[nodiscard]] std::uint8_t
convertLuma(const core::Rgb8Pixel& pixel)
{
	return convertComponent(pixel, lumaWeights, lumaOffset);
}

[[nodiscard]] std::uint8_t
convertRedDifference(const core::Rgb8Pixel& pixel)
{
	return convertComponent(pixel, redDifferenceWeights, differenceOffset);
}

template<typename Converter>
[[nodiscard]] std::uint8_t
averageBlock(const core::Rgb8View frame, const std::uint16_t x,
	const std::uint16_t y, Converter converter)
{
	const std::uint32_t sum = static_cast<std::uint32_t>(converter(frame.pixel(x, y)))
	    + static_cast<std::uint32_t>(converter(
	        frame.pixel(static_cast<std::uint16_t>(x + 1U), y)))
	    + static_cast<std::uint32_t>(converter(
	        frame.pixel(x, static_cast<std::uint16_t>(y + 1U))))
	    + static_cast<std::uint32_t>(converter(frame.pixel(
	        static_cast<std::uint16_t>(x + 1U),
	        static_cast<std::uint16_t>(y + 1U))));
	return static_cast<std::uint8_t>(sum / 4U);
}

[[nodiscard]] Duration
lineDuration(const std::span<const Robot36Segment> schedule)
{
	Duration duration(0, 1);
	for (const Robot36Segment& segment : schedule) {
		if (const auto* fixed = std::get_if<Robot36FixedToneSegment>(&segment)) {
			duration = duration + fixed->duration;
		} else {
			duration = duration + std::get<Robot36ScanSegment>(segment).duration;
		}
	}
	return duration;
}

void
appendFixedSchedule(std::vector<core::ToneEvent>& events,
	const std::span<const Robot36FixedToneSegment> schedule, const float amplitude)
{
	for (const Robot36FixedToneSegment& segment : schedule) {
		events.emplace_back(segment.duration, segment.frequencyHz, amplitude);
	}
}

void
appendScan(std::vector<core::ToneEvent>& events,
	const LumaColourDifference420View colour, const Robot36ScanSegment& scan,
	const std::uint16_t y, const float amplitude)
{
	if (scan.component == Robot36Component::lumaY) {
		const Duration pixelDuration = scan.duration / colour.width();
		for (std::uint16_t x = 0; x < colour.width(); ++x) {
			events.emplace_back(pixelDuration,
			    robot36ComponentFrequency(colour.luma(x, y)), amplitude);
		}
		return;
	}
	const Duration pixelDuration = scan.duration / colour.chromaWidth();
	const std::uint16_t chromaY = static_cast<std::uint16_t>(y / 2U);
	for (std::uint16_t x = 0; x < colour.chromaWidth(); ++x) {
		const std::uint8_t value = scan.component == Robot36Component::redDifference
		    ? colour.redDifference(x, chromaY)
		    : colour.blueDifference(x, chromaY);
		events.emplace_back(pixelDuration, robot36ComponentFrequency(value), amplitude);
	}
}

void
appendLine(std::vector<core::ToneEvent>& events,
	const LumaColourDifference420View colour,
	const std::span<const Robot36Segment> schedule, const std::uint16_t y,
	const float amplitude)
{
	for (const Robot36Segment& segment : schedule) {
		if (const auto* fixed = std::get_if<Robot36FixedToneSegment>(&segment)) {
			events.emplace_back(fixed->duration, fixed->frequencyHz, amplitude);
		} else {
			appendScan(events, colour, std::get<Robot36ScanSegment>(segment), y,
			    amplitude);
		}
	}
}

void
validateDescriptor()
{
	if (descriptor.id.empty() || descriptor.displayName.empty()
	    || descriptor.width == 0U || descriptor.height == 0U
	    || descriptor.width % 2U != 0U || descriptor.height % 2U != 0U
	    || descriptor.evenLineSchedule.empty() || descriptor.oddLineSchedule.empty()
	    || !std::isfinite(descriptor.blackHz) || !std::isfinite(descriptor.whiteHz)
	    || descriptor.blackHz <= 0.0 || descriptor.whiteHz <= descriptor.blackHz) {
		throw std::logic_error("invalid Robot 36 descriptor");
	}
}

} // namespace

LumaColourDifference420Frame
convertRobot36Colour(const core::Rgb8View frame)
{
	if (frame.width() % 2U != 0U || frame.height() % 2U != 0U) {
		throw std::invalid_argument("Robot 36 colour conversion requires even dimensions");
	}
	const std::size_t lumaSize = static_cast<std::size_t>(frame.width()) * frame.height();
	const std::size_t chromaSize = static_cast<std::size_t>(frame.width() / 2U)
	    * static_cast<std::size_t>(frame.height() / 2U);
	std::vector<std::uint8_t> lumas(lumaSize);
	std::vector<std::uint8_t> redDifferences(chromaSize);
	std::vector<std::uint8_t> blueDifferences(chromaSize);
	for (std::uint16_t y = 0; y < frame.height(); ++y) {
		for (std::uint16_t x = 0; x < frame.width(); ++x) {
			lumas[static_cast<std::size_t>(y) * frame.width() + x]
			    = convertLuma(frame.pixel(x, y));
		}
	}
	for (std::uint16_t y = 0; y < frame.height(); y += 2U) {
		for (std::uint16_t x = 0; x < frame.width(); x += 2U) {
			const std::size_t index = static_cast<std::size_t>(y / 2U)
			    * static_cast<std::size_t>(frame.width() / 2U) + x / 2U;
			redDifferences[index] = averageBlock(frame, x, y, convertRedDifference);
			blueDifferences[index] = averageBlock(frame, x, y, convertBlueDifference);
		}
	}
	return LumaColourDifference420Frame(frame.width(), frame.height(),
	    std::move(lumas), std::move(redDifferences), std::move(blueDifferences));
}

std::vector<core::ToneEvent>
encodeRobot36(const core::Rgb8View frame, const float amplitude)
{
	validateDescriptor();
	if (frame.width() != descriptor.width || frame.height() != descriptor.height) {
		throw std::invalid_argument("Robot 36 requires a 320 by 240 RGB8 frame");
	}
	const VisHeader header = makeVisHeader(descriptor.visCode, amplitude);
	const LumaColourDifference420Frame converted = convertRobot36Colour(frame);
	const LumaColourDifference420View colour = converted.view();
	constexpr std::size_t eventsPerLine = 4U + 320U + 160U;
	std::vector<core::ToneEvent> events;
	events.reserve(header.size() + descriptor.preImageSchedule.size()
	    + eventsPerLine * descriptor.height);
	events.insert(events.end(), header.begin(), header.end());
	appendFixedSchedule(events, descriptor.preImageSchedule, amplitude);
	for (std::uint16_t y = 0; y < descriptor.height; ++y) {
		appendLine(events, colour,
		    y % 2U == 0U ? descriptor.evenLineSchedule : descriptor.oddLineSchedule,
		    y, amplitude);
	}
	return events;
}

const Robot36Descriptor&
robot36Descriptor()
{
	return descriptor;
}

double
robot36ComponentFrequency(const std::uint8_t value) noexcept
{
	return descriptor.blackHz
	    + (descriptor.whiteHz - descriptor.blackHz) * value / 255.0;
}

Duration
robot36TransmissionDuration()
{
	validateDescriptor();
	Duration duration(0, 1);
	for (const core::ToneEvent& event : makeVisHeader(descriptor.visCode, 0.8F)) {
		duration = duration + event.duration();
	}
	for (const Robot36FixedToneSegment& segment : descriptor.preImageSchedule) {
		duration = duration + segment.duration;
	}
	const Duration evenDuration = lineDuration(descriptor.evenLineSchedule);
	const Duration oddDuration = lineDuration(descriptor.oddLineSchedule);
	return duration + evenDuration * (descriptor.height / 2U)
	    + oddDuration * (descriptor.height / 2U);
}

} // namespace sstv::analog
