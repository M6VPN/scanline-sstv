// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/pd_120.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/pd_120.hpp>

#include <sstv/analog/vis.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
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

const std::array<Pd120FixedToneSegment, 0> preImageSchedule{};

const std::array<Pd120Segment, 6> pairSchedule{{
	Pd120FixedToneSegment{Duration::fromMicroseconds(20'000), 1'200.0},
	Pd120FixedToneSegment{Duration::fromMicroseconds(2'080), 1'500.0},
	Pd120ScanSegment{Pd120Component::firstLuma,
	    Duration::fromMicroseconds(121'600), 640},
	Pd120ScanSegment{Pd120Component::redDifference,
	    Duration::fromMicroseconds(121'600), 640},
	Pd120ScanSegment{Pd120Component::blueDifference,
	    Duration::fromMicroseconds(121'600), 640},
	Pd120ScanSegment{Pd120Component::secondLuma,
	    Duration::fromMicroseconds(121'600), 640},
}};

const Pd120Descriptor descriptor{
	"pd-120",
	"PD120",
	95,
	640,
	496,
	preImageSchedule,
	pairSchedule,
	1'500.0,
	2'300.0,
};

[[nodiscard]] std::size_t
checkedAdd(const std::size_t left, const std::size_t right)
{
	if (left > std::numeric_limits<std::size_t>::max() - right) {
		throw std::overflow_error("PD120 event count overflow");
	}
	return left + right;
}

[[nodiscard]] std::size_t
checkedMultiply(const std::size_t left, const std::size_t right)
{
	if (left != 0U && right > std::numeric_limits<std::size_t>::max() / left) {
		throw std::overflow_error("PD120 event count overflow");
	}
	return left * right;
}

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
averageRows(const core::Rgb8View frame, const std::uint16_t x,
	const std::uint16_t y, Converter converter)
{
	const std::uint16_t sum = static_cast<std::uint16_t>(converter(frame.pixel(x, y)))
	    + static_cast<std::uint16_t>(converter(
	        frame.pixel(x, static_cast<std::uint16_t>(y + 1U))));
	return static_cast<std::uint8_t>(sum / 2U);
}

[[nodiscard]] Duration
pairDuration()
{
	Duration duration(0, 1);
	for (const Pd120Segment& segment : descriptor.pairSchedule) {
		if (const auto* fixed = std::get_if<Pd120FixedToneSegment>(&segment)) {
			duration = duration + fixed->duration;
		} else {
			duration = duration + std::get<Pd120ScanSegment>(segment).duration;
		}
	}
	return duration;
}

void
appendFixedSchedule(std::vector<core::ToneEvent>& events,
	const std::span<const Pd120FixedToneSegment> schedule, const float amplitude)
{
	for (const Pd120FixedToneSegment& segment : schedule) {
		events.emplace_back(segment.duration, segment.frequencyHz, amplitude);
	}
}

void
appendScan(std::vector<core::ToneEvent>& events,
	const LumaColourDifference440View colour, const Pd120ScanSegment& scan,
	const std::uint16_t pair, const float amplitude)
{
	const Duration pixelDuration = scan.duration / scan.sampleWidth;
	const std::uint16_t firstRow = static_cast<std::uint16_t>(pair * 2U);
	for (std::uint16_t x = 0; x < scan.sampleWidth; ++x) {
		std::uint8_t value = 0;
		switch (scan.component) {
		case Pd120Component::firstLuma:
			value = colour.luma(x, firstRow);
			break;
		case Pd120Component::redDifference:
			value = colour.redDifference(x, pair);
			break;
		case Pd120Component::blueDifference:
			value = colour.blueDifference(x, pair);
			break;
		case Pd120Component::secondLuma:
			value = colour.luma(x, static_cast<std::uint16_t>(firstRow + 1U));
			break;
		}
		events.emplace_back(pixelDuration, pd120ComponentFrequency(value), amplitude);
	}
}

void
appendPair(std::vector<core::ToneEvent>& events,
	const LumaColourDifference440View colour, const std::uint16_t pair,
	const float amplitude)
{
	for (const Pd120Segment& segment : descriptor.pairSchedule) {
		if (const auto* fixed = std::get_if<Pd120FixedToneSegment>(&segment)) {
			events.emplace_back(fixed->duration, fixed->frequencyHz, amplitude);
		} else {
			appendScan(events, colour, std::get<Pd120ScanSegment>(segment), pair,
			    amplitude);
		}
	}
}

[[nodiscard]] std::size_t
eventCount(const std::size_t headerSize)
{
	std::size_t pairEvents = 0;
	for (const Pd120Segment& segment : descriptor.pairSchedule) {
		if (std::holds_alternative<Pd120FixedToneSegment>(segment)) {
			pairEvents = checkedAdd(pairEvents, 1U);
		} else {
			pairEvents = checkedAdd(pairEvents,
			    std::get<Pd120ScanSegment>(segment).sampleWidth);
		}
	}
	const std::size_t pairCount = descriptor.height / 2U;
	return checkedAdd(checkedAdd(headerSize, descriptor.preImageSchedule.size()),
	    checkedMultiply(pairEvents, pairCount));
}

void
validateDescriptor()
{
	if (descriptor.id.empty() || descriptor.displayName.empty()
	    || descriptor.width == 0U || descriptor.height == 0U
	    || descriptor.height % 2U != 0U || descriptor.pairSchedule.empty()
	    || !std::isfinite(descriptor.blackHz) || !std::isfinite(descriptor.whiteHz)
	    || descriptor.blackHz <= 0.0 || descriptor.whiteHz <= descriptor.blackHz) {
		throw std::logic_error("invalid PD120 descriptor");
	}
	for (const Pd120Segment& segment : descriptor.pairSchedule) {
		if (const auto* scan = std::get_if<Pd120ScanSegment>(&segment)) {
			if (scan->sampleWidth != descriptor.width) {
				throw std::logic_error("invalid PD120 scan width");
			}
		}
	}
}

} // namespace

LumaColourDifference440Frame
convertPd120Colour(const core::Rgb8View frame)
{
	if (frame.height() % 2U != 0U) {
		throw std::invalid_argument("PD120 colour conversion requires even height");
	}
	const std::size_t lumaSize = static_cast<std::size_t>(frame.width()) * frame.height();
	const std::size_t chromaSize = static_cast<std::size_t>(frame.width())
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
		for (std::uint16_t x = 0; x < frame.width(); ++x) {
			const std::size_t index = static_cast<std::size_t>(y / 2U) * frame.width()
			    + x;
			redDifferences[index] = averageRows(frame, x, y, convertRedDifference);
			blueDifferences[index] = averageRows(frame, x, y, convertBlueDifference);
		}
	}
	return LumaColourDifference440Frame(frame.width(), frame.height(),
	    std::move(lumas), std::move(redDifferences), std::move(blueDifferences));
}

std::vector<core::ToneEvent>
encodePd120(const core::Rgb8View frame, const float amplitude)
{
	validateDescriptor();
	if (frame.width() != descriptor.width || frame.height() != descriptor.height) {
		throw std::invalid_argument("PD120 requires a 640 by 496 RGB8 frame");
	}
	const VisHeader header = makeVisHeader(descriptor.visCode, amplitude);
	const LumaColourDifference440Frame converted = convertPd120Colour(frame);
	const LumaColourDifference440View colour = converted.view();
	std::vector<core::ToneEvent> events;
	events.reserve(eventCount(header.size()));
	events.insert(events.end(), header.begin(), header.end());
	appendFixedSchedule(events, descriptor.preImageSchedule, amplitude);
	for (std::uint16_t pair = 0; pair < descriptor.height / 2U; ++pair) {
		appendPair(events, colour, pair, amplitude);
	}
	return events;
}

const Pd120Descriptor&
pd120Descriptor()
{
	return descriptor;
}

double
pd120ComponentFrequency(const std::uint8_t value) noexcept
{
	return descriptor.blackHz
	    + (descriptor.whiteHz - descriptor.blackHz) * value / 255.0;
}

Duration
pd120TransmissionDuration()
{
	validateDescriptor();
	Duration duration(0, 1);
	for (const core::ToneEvent& event : makeVisHeader(descriptor.visCode, 0.8F)) {
		duration = duration + event.duration();
	}
	for (const Pd120FixedToneSegment& segment : descriptor.preImageSchedule) {
		duration = duration + segment.duration;
	}
	return duration + pairDuration() * (descriptor.height / 2U);
}

} // namespace sstv::analog
