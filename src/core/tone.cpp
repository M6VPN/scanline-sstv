// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/tone.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/core/tone.hpp>

#include <cmath>
#include <stdexcept>

namespace sstv::core {

ToneEvent::ToneEvent(Duration duration, const double frequencyHz,
    const float amplitude)
	: amplitudeValue(amplitude), durationValue(duration),
	  frequencyValue(frequencyHz)
{
	if (duration.numerator() == 0) {
		throw std::invalid_argument("tone duration must be positive");
	}
	if (!std::isfinite(frequencyHz) || frequencyHz <= 0.0) {
		throw std::invalid_argument("tone frequency must be finite and positive");
	}
	if (!std::isfinite(amplitude) || amplitude < 0.0F || amplitude > 1.0F) {
		throw std::invalid_argument("tone amplitude must be finite and within [0, 1]");
	}
}

float
ToneEvent::amplitude() const noexcept
{
	return amplitudeValue;
}

const Duration&
ToneEvent::duration() const noexcept
{
	return durationValue;
}

double
ToneEvent::frequencyHz() const noexcept
{
	return frequencyValue;
}

} // namespace sstv::core
