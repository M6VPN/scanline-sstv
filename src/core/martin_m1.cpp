// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/martin_m1.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/martin_m1.hpp>

#include <sstv/analog/vis.hpp>

#include <cstddef>
#include <stdexcept>

namespace sstv::analog {
namespace {

/* Evidence and resolved divergence: docs/protocols/analogue/martin-m1.md. */
const SequentialRgbDescriptor descriptor{
	"martin-m1",
	"Martin M1",
	44,
	320,
	256,
	{RgbChannel::green, RgbChannel::blue, RgbChannel::red},
	core::Duration::fromMicroseconds(4'862),
	core::Duration::fromMicroseconds(572),
	core::Duration::fromMicroseconds(572),
	core::Duration::fromMicroseconds(146'432),
	1'200.0,
	1'500.0,
	2'300.0,
};

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

[[nodiscard]] core::Duration
headerDuration()
{
	const VisHeader header = makeVisHeader(descriptor.visCode, 0.8F);
	core::Duration duration(0, 1);
	for (const core::ToneEvent& event : header) {
		duration = duration + event.duration();
	}
	return duration;
}

[[nodiscard]] core::Duration
lineDuration()
{
	return descriptor.lineSync + descriptor.porch
	    + descriptor.channelScan * descriptor.channelOrder.size()
	    + descriptor.separator * descriptor.channelOrder.size();
}

void
appendLine(std::vector<core::ToneEvent>& events, const core::Rgb8View frame,
    const std::uint16_t y, const float amplitude)
{
	const core::Duration pixelDuration = descriptor.channelScan / descriptor.width;
	events.emplace_back(descriptor.lineSync, descriptor.syncHz, amplitude);
	events.emplace_back(descriptor.porch, descriptor.blackHz, amplitude);
	for (const RgbChannel channel : descriptor.channelOrder) {
		for (std::uint16_t x = 0; x < descriptor.width; ++x) {
			const std::uint8_t value = component(frame.pixel(x, y), channel);
			events.emplace_back(pixelDuration, martinM1PixelFrequency(value), amplitude);
		}
		events.emplace_back(descriptor.separator, descriptor.blackHz, amplitude);
	}
}

} // namespace

const SequentialRgbDescriptor&
martinM1Descriptor()
{
	return descriptor;
}

double
martinM1PixelFrequency(const std::uint8_t value) noexcept
{
	return descriptor.blackHz
	    + (descriptor.whiteHz - descriptor.blackHz) * value / 255.0;
}

core::Duration
martinM1TransmissionDuration()
{
	return headerDuration() + lineDuration() * descriptor.height;
}

std::vector<core::ToneEvent>
encodeMartinM1(const core::Rgb8View frame, const float amplitude)
{
	if (frame.width() != descriptor.width || frame.height() != descriptor.height) {
		throw std::invalid_argument("Martin M1 requires a 320 by 256 RGB8 frame");
	}
	constexpr std::size_t headerEventCount = 13;
	const std::size_t eventsPerLine = 1U + 1U + descriptor.channelOrder.size()
	    * (static_cast<std::size_t>(descriptor.width) + 1U);
	std::vector<core::ToneEvent> events;
	events.reserve(headerEventCount + eventsPerLine * descriptor.height);
	const VisHeader header = makeVisHeader(descriptor.visCode, amplitude);
	events.insert(events.end(), header.begin(), header.end());
	for (std::uint16_t y = 0; y < descriptor.height; ++y) {
		appendLine(events, frame, y, amplitude);
	}
	return events;
}

} // namespace sstv::analog
