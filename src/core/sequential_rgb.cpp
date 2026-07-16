// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/sequential_rgb.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/sequential_rgb.hpp>

#include <sstv/analog/vis.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

namespace sstv::analog {
namespace {

[[nodiscard]] std::size_t
checkedAdd(const std::size_t left, const std::size_t right)
{
	if (right > std::numeric_limits<std::size_t>::max() - left) {
		throw std::overflow_error("sequential RGB event count overflow");
	}
	return left + right;
}

[[nodiscard]] std::size_t
checkedMultiply(const std::size_t left, const std::size_t right)
{
	if (left != 0U && right > std::numeric_limits<std::size_t>::max() / left) {
		throw std::overflow_error("sequential RGB event count overflow");
	}
	return left * right;
}

[[nodiscard]] std::uint8_t
component(const core::Rgb8Pixel& pixel, const RgbChannel channel)
{
	switch (channel) {
	case RgbChannel::red:
		return pixel.red;
	case RgbChannel::green:
		return pixel.green;
	case RgbChannel::blue:
		return pixel.blue;
	}
	throw std::logic_error("invalid RGB channel");
}

void
appendFixedSchedule(std::vector<core::ToneEvent>& events,
	const std::span<const FixedToneSegment> schedule, const float amplitude)
{
	for (const FixedToneSegment& segment : schedule) {
		events.emplace_back(segment.duration, segment.frequencyHz, amplitude);
	}
}

void
appendLineSchedule(std::vector<core::ToneEvent>& events,
	const SequentialRgbDescriptor& descriptor,
	const std::span<const SequentialRgbSegment> schedule,
	const core::Rgb8View frame, const std::uint16_t y, const float amplitude)
{
	for (const SequentialRgbSegment& segment : schedule) {
		if (const auto* fixed = std::get_if<FixedToneSegment>(&segment)) {
			events.emplace_back(fixed->duration, fixed->frequencyHz, amplitude);
			continue;
		}
		const RgbScanSegment& scan = std::get<RgbScanSegment>(segment);
		const core::Duration pixelDuration = scan.duration / descriptor.width;
		for (std::uint16_t x = 0; x < descriptor.width; ++x) {
			const std::uint8_t value = component(frame.pixel(x, y), scan.channel);
			events.emplace_back(pixelDuration,
			    sequentialRgbPixelFrequency(descriptor, value), amplitude);
		}
	}
}

[[nodiscard]] core::Duration
fixedScheduleDuration(const std::span<const FixedToneSegment> schedule)
{
	core::Duration duration(0, 1);
	for (const FixedToneSegment& segment : schedule) {
		duration = duration + segment.duration;
	}
	return duration;
}

[[nodiscard]] std::size_t
lineEventCount(const SequentialRgbDescriptor& descriptor,
	const std::span<const SequentialRgbSegment> schedule)
{
	std::size_t count = 0;
	for (const SequentialRgbSegment& segment : schedule) {
		count = checkedAdd(count, std::holds_alternative<FixedToneSegment>(segment)
		    ? 1U : static_cast<std::size_t>(descriptor.width));
	}
	return count;
}

[[nodiscard]] core::Duration
lineScheduleDuration(const std::span<const SequentialRgbSegment> schedule)
{
	core::Duration duration(0, 1);
	for (const SequentialRgbSegment& segment : schedule) {
		if (const auto* fixed = std::get_if<FixedToneSegment>(&segment)) {
			duration = duration + fixed->duration;
		} else {
			duration = duration + std::get<RgbScanSegment>(segment).duration;
		}
	}
	return duration;
}

void
validateDescriptor(const SequentialRgbDescriptor& descriptor)
{
	if (descriptor.id.empty() || descriptor.displayName.empty()
	    || descriptor.width == 0U || descriptor.height == 0U
	    || descriptor.firstLineSchedule.empty() || descriptor.lineSchedule.empty()) {
		throw std::invalid_argument("invalid sequential RGB descriptor");
	}
	if (!std::isfinite(descriptor.blackHz) || !std::isfinite(descriptor.whiteHz)
	    || descriptor.blackHz <= 0.0 || descriptor.whiteHz <= descriptor.blackHz) {
		throw std::invalid_argument("invalid sequential RGB frequency range");
	}
}

} // namespace

std::vector<core::ToneEvent>
encodeSequentialRgb(const SequentialRgbDescriptor& descriptor,
	const core::Rgb8View frame, const float amplitude)
{
	validateDescriptor(descriptor);
	if (frame.width() != descriptor.width || frame.height() != descriptor.height) {
		throw std::invalid_argument(std::string(descriptor.displayName)
		    + " requires a " + std::to_string(descriptor.width) + " by "
		    + std::to_string(descriptor.height) + " RGB8 frame");
	}
	const VisHeader header = makeVisHeader(descriptor.visCode, amplitude);
	const std::size_t firstLineCount = lineEventCount(
	    descriptor, descriptor.firstLineSchedule);
	const std::size_t regularLineCount = lineEventCount(
	    descriptor, descriptor.lineSchedule);
	const std::size_t regularLines = static_cast<std::size_t>(descriptor.height - 1U);
	std::size_t eventCount = checkedAdd(header.size(), descriptor.preImageSchedule.size());
	eventCount = checkedAdd(eventCount, firstLineCount);
	eventCount = checkedAdd(eventCount, checkedMultiply(regularLineCount, regularLines));
	std::vector<core::ToneEvent> events;
	events.reserve(eventCount);
	events.insert(events.end(), header.begin(), header.end());
	appendFixedSchedule(events, descriptor.preImageSchedule, amplitude);
	appendLineSchedule(events, descriptor, descriptor.firstLineSchedule,
	    frame, 0, amplitude);
	for (std::uint16_t y = 1; y < descriptor.height; ++y) {
		appendLineSchedule(events, descriptor, descriptor.lineSchedule,
		    frame, y, amplitude);
	}
	return events;
}

double
sequentialRgbPixelFrequency(const SequentialRgbDescriptor& descriptor,
	const std::uint8_t value) noexcept
{
	return descriptor.blackHz
	    + (descriptor.whiteHz - descriptor.blackHz) * value / 255.0;
}

core::Duration
sequentialRgbTransmissionDuration(const SequentialRgbDescriptor& descriptor)
{
	validateDescriptor(descriptor);
	const VisHeader header = makeVisHeader(descriptor.visCode, 0.8F);
	core::Duration duration(0, 1);
	for (const core::ToneEvent& event : header) {
		duration = duration + event.duration();
	}
	duration = duration + fixedScheduleDuration(descriptor.preImageSchedule);
	duration = duration + lineScheduleDuration(descriptor.firstLineSchedule);
	return duration + lineScheduleDuration(descriptor.lineSchedule)
	    * static_cast<std::uint64_t>(descriptor.height - 1U);
}

} // namespace sstv::analog
