// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/core/tone.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/core/timing.hpp>

namespace sstv::core {

/** Immutable description of one constant-frequency waveform interval. */
class ToneEvent {
public:
	ToneEvent(Duration, double, float);

	[[nodiscard]] float amplitude() const noexcept;
	[[nodiscard]] const Duration& duration() const noexcept;
	[[nodiscard]] double frequencyHz() const noexcept;

private:
	float amplitudeValue;
	Duration durationValue;
	double frequencyValue;
};

} // namespace sstv::core
