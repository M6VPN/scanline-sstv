// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/timing.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/core/timing.hpp>

#include <limits>
#include <numeric>
#include <stdexcept>

namespace sstv::core {
namespace {

[[nodiscard]] std::uint64_t
checkedAdd(const std::uint64_t left, const std::uint64_t right)
{
	if (right > std::numeric_limits<std::uint64_t>::max() - left) {
		throw std::overflow_error("duration addition overflow");
	}
	return left + right;
}

[[nodiscard]] std::uint64_t
checkedMultiply(const std::uint64_t left, const std::uint64_t right)
{
	if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
		throw std::overflow_error("duration multiplication overflow");
	}
	return left * right;
}

} // namespace

Duration::Duration(const std::uint64_t numerator, const std::uint64_t denominator)
	: denominatorValue(denominator), numeratorValue(numerator)
{
	if (denominator == 0) {
		throw std::invalid_argument("duration denominator must be nonzero");
	}
	const std::uint64_t divisor = std::gcd(numeratorValue, denominatorValue);
	numeratorValue /= divisor;
	denominatorValue /= divisor;
}

Duration
Duration::fromMicroseconds(const std::uint64_t microseconds)
{
	return Duration(microseconds, 1'000'000);
}

std::uint64_t
Duration::denominator() const noexcept
{
	return denominatorValue;
}

std::uint64_t
Duration::numerator() const noexcept
{
	return numeratorValue;
}

bool
operator==(const Duration& left, const Duration& right) noexcept
{
	return left.numerator() == right.numerator()
	    && left.denominator() == right.denominator();
}

Duration
operator+(const Duration& left, const Duration& right)
{
	const std::uint64_t divisor = std::gcd(left.denominator(), right.denominator());
	const std::uint64_t leftMultiplier = right.denominator() / divisor;
	const std::uint64_t rightMultiplier = left.denominator() / divisor;
	const std::uint64_t leftNumerator = checkedMultiply(left.numerator(), leftMultiplier);
	const std::uint64_t rightNumerator = checkedMultiply(right.numerator(), rightMultiplier);
	const std::uint64_t numerator = checkedAdd(leftNumerator, rightNumerator);
	const std::uint64_t denominator = checkedMultiply(left.denominator(), leftMultiplier);
	return Duration(numerator, denominator);
}

Duration
operator*(const Duration& duration, const std::uint64_t count)
{
	if (count == 0) {
		return Duration(0, 1);
	}
	const std::uint64_t divisor = std::gcd(duration.denominator(), count);
	const std::uint64_t numerator = checkedMultiply(duration.numerator(), count / divisor);
	return Duration(numerator, duration.denominator() / divisor);
}

Duration
operator/(const Duration& duration, const std::uint64_t divisor)
{
	if (divisor == 0) {
		throw std::invalid_argument("duration divisor must be nonzero");
	}
	const std::uint64_t common = std::gcd(duration.numerator(), divisor);
	const std::uint64_t denominator = checkedMultiply(duration.denominator(),
	    divisor / common);
	return Duration(duration.numerator() / common, denominator);
}

std::uint64_t
sampleCount(const Duration& duration, const std::uint32_t sampleRate)
{
	if (sampleRate == 0) {
		throw std::invalid_argument("sample rate must be nonzero");
	}
	const std::uint64_t divisor = std::gcd(duration.denominator(),
	    static_cast<std::uint64_t>(sampleRate));
	const std::uint64_t rate = static_cast<std::uint64_t>(sampleRate) / divisor;
	const std::uint64_t denominator = duration.denominator() / divisor;
	return checkedMultiply(duration.numerator(), rate) / denominator;
}

SampleBoundaryScheduler::SampleBoundaryScheduler(const std::uint32_t sampleRate)
	: elapsedDuration(0, 1), frameBoundaryValue(0), sampleRateValue(sampleRate)
{
	if (sampleRate == 0) {
		throw std::invalid_argument("sample rate must be nonzero");
	}
}

std::uint64_t
SampleBoundaryScheduler::advance(const Duration& duration)
{
	elapsedDuration = elapsedDuration + duration;
	frameBoundaryValue = sampleCount(elapsedDuration, sampleRateValue);
	return frameBoundaryValue;
}

const Duration&
SampleBoundaryScheduler::elapsed() const noexcept
{
	return elapsedDuration;
}

std::uint64_t
SampleBoundaryScheduler::frameBoundary() const noexcept
{
	return frameBoundaryValue;
}

} // namespace sstv::core
