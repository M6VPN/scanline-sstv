// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/vis.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/vis.hpp>

#include <sstv/core/timing.hpp>

#include <cstddef>
#include <stdexcept>

namespace sstv::analog {
namespace {

/* Evidence: the VIS framing sections in both accepted analogue mode records. */
constexpr double bitOneHz = 1'100.0;
constexpr double bitZeroHz = 1'300.0;
constexpr double leaderHz = 1'900.0;
constexpr double syncHz = 1'200.0;

[[nodiscard]] core::ToneEvent
event(const std::uint64_t microseconds, const double frequency, const float amplitude)
{
	return core::ToneEvent(core::Duration::fromMicroseconds(microseconds), frequency,
	    amplitude);
}

[[nodiscard]] double
visFrequency(const bool bit) noexcept
{
	return bit ? bitOneHz : bitZeroHz;
}

} // namespace

VisBits
makeVisBits(const std::uint8_t code)
{
	if (code > 0x7FU) {
		throw std::invalid_argument("VIS code must fit in seven bits");
	}
	VisBits bits{};
	std::uint8_t ones = 0;
	for (std::size_t index = 0; index < 7; ++index) {
		bits[index] = (code & (1U << index)) != 0U;
		ones = static_cast<std::uint8_t>(ones + (bits[index] ? 1U : 0U));
	}
	bits[7] = ones % 2U != 0U;
	return bits;
}

VisHeader
makeVisHeader(const std::uint8_t code, const float amplitude)
{
	const VisBits bits = makeVisBits(code);
	return {
		event(300'000, leaderHz, amplitude),
		event(10'000, syncHz, amplitude),
		event(300'000, leaderHz, amplitude),
		event(30'000, syncHz, amplitude),
		event(30'000, visFrequency(bits[0]), amplitude),
		event(30'000, visFrequency(bits[1]), amplitude),
		event(30'000, visFrequency(bits[2]), amplitude),
		event(30'000, visFrequency(bits[3]), amplitude),
		event(30'000, visFrequency(bits[4]), amplitude),
		event(30'000, visFrequency(bits[5]), amplitude),
		event(30'000, visFrequency(bits[6]), amplitude),
		event(30'000, visFrequency(bits[7]), amplitude),
		event(30'000, syncHz, amplitude),
	};
}

} // namespace sstv::analog
