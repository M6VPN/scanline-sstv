// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/martin_m1.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/martin_m1.hpp>

#include <array>

namespace sstv::analog {
namespace {

/* Evidence and resolved divergence: docs/protocols/analogue/martin-m1.md. */
const std::array<SequentialRgbSegment, 8> lineSchedule{{
	FixedToneSegment{core::Duration::fromMicroseconds(4'862), 1'200.0},
	FixedToneSegment{core::Duration::fromMicroseconds(572), 1'500.0},
	RgbScanSegment{RgbChannel::green, core::Duration::fromMicroseconds(146'432)},
	FixedToneSegment{core::Duration::fromMicroseconds(572), 1'500.0},
	RgbScanSegment{RgbChannel::blue, core::Duration::fromMicroseconds(146'432)},
	FixedToneSegment{core::Duration::fromMicroseconds(572), 1'500.0},
	RgbScanSegment{RgbChannel::red, core::Duration::fromMicroseconds(146'432)},
	FixedToneSegment{core::Duration::fromMicroseconds(572), 1'500.0},
}};

const SequentialRgbDescriptor descriptor{
	"martin-m1",
	"Martin M1",
	44,
	320,
	256,
	{},
	lineSchedule,
	lineSchedule,
	1'500.0,
	2'300.0,
};

} // namespace

const SequentialRgbDescriptor&
martinM1Descriptor()
{
	return descriptor;
}

double
martinM1PixelFrequency(const std::uint8_t value) noexcept
{
	return sequentialRgbPixelFrequency(descriptor, value);
}

core::Duration
martinM1TransmissionDuration()
{
	return sequentialRgbTransmissionDuration(descriptor);
}

std::vector<core::ToneEvent>
encodeMartinM1(const core::Rgb8View frame, const float amplitude)
{
	return encodeSequentialRgb(descriptor, frame, amplitude);
}

} // namespace sstv::analog
