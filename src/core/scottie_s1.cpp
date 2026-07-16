// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/scottie_s1.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/scottie_s1.hpp>

#include <array>

namespace sstv::analog {
namespace {

/* Evidence and resolved divergences: docs/protocols/analogue/scottie-s1.md. */
const std::array<SequentialRgbSegment, 7> lineSchedule{{
	FixedToneSegment{core::Duration::fromMicroseconds(1'500), 1'500.0},
	RgbScanSegment{RgbChannel::green, core::Duration::fromMicroseconds(138'240)},
	FixedToneSegment{core::Duration::fromMicroseconds(1'500), 1'500.0},
	RgbScanSegment{RgbChannel::blue, core::Duration::fromMicroseconds(138'240)},
	FixedToneSegment{core::Duration::fromMicroseconds(9'000), 1'200.0},
	FixedToneSegment{core::Duration::fromMicroseconds(1'500), 1'500.0},
	RgbScanSegment{RgbChannel::red, core::Duration::fromMicroseconds(138'240)},
}};

const SequentialRgbDescriptor descriptor{
	"scottie-s1",
	"Scottie S1",
	60,
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
scottieS1Descriptor()
{
	return descriptor;
}

double
scottieS1PixelFrequency(const std::uint8_t value) noexcept
{
	return sequentialRgbPixelFrequency(descriptor, value);
}

core::Duration
scottieS1TransmissionDuration()
{
	return sequentialRgbTransmissionDuration(descriptor);
}

std::vector<core::ToneEvent>
encodeScottieS1(const core::Rgb8View frame, const float amplitude)
{
	return encodeSequentialRgb(descriptor, frame, amplitude);
}

} // namespace sstv::analog
