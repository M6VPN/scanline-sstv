// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/core/timing.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace sstv::core {

/** Exact non-negative duration represented as a normalized rational number of seconds. */
class Duration {
public:
	Duration(std::uint64_t, std::uint64_t);

	/** Construct an exact duration from a whole number of microseconds. */
	[[nodiscard]] static Duration fromMicroseconds(std::uint64_t);

	[[nodiscard]] std::uint64_t denominator() const noexcept;
	[[nodiscard]] std::uint64_t numerator() const noexcept;

private:
	std::uint64_t denominatorValue;
	std::uint64_t numeratorValue;
};

[[nodiscard]] bool operator==(const Duration&, const Duration&) noexcept;
[[nodiscard]] Duration operator+(const Duration&, const Duration&);
[[nodiscard]] Duration operator*(const Duration&, std::uint64_t);
[[nodiscard]] Duration operator/(const Duration&, std::uint64_t);

/** Return the cumulative floor sample boundary for an exact duration. */
[[nodiscard]] std::uint64_t sampleCount(const Duration&, std::uint32_t);

/** Accumulate exact durations before converting each boundary to a sample index. */
class SampleBoundaryScheduler {
public:
	explicit SampleBoundaryScheduler(std::uint32_t);

	/** Add a duration and return the cumulative floor sample boundary. */
	[[nodiscard]] std::uint64_t advance(const Duration&);
	[[nodiscard]] const Duration& elapsed() const noexcept;
	[[nodiscard]] std::uint64_t frameBoundary() const noexcept;

private:
	Duration elapsedDuration;
	std::uint64_t frameBoundaryValue;
	std::uint32_t sampleRateValue;
};

} // namespace sstv::core
