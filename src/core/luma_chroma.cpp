// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/luma_chroma.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/luma_chroma.hpp>

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <utility>

namespace sstv::analog {
namespace {

[[nodiscard]] std::size_t
checkedPlaneSize(const std::uint16_t width, const std::uint16_t height,
	const bool isChroma)
{
	if (width == 0U || height == 0U || width % 2U != 0U || height % 2U != 0U) {
		throw std::invalid_argument(
		    "4:2:0 frame dimensions must be nonzero and even");
	}
	const std::size_t planeWidth = isChroma ? width / 2U : width;
	const std::size_t planeHeight = isChroma ? height / 2U : height;
	if (planeWidth > std::numeric_limits<std::size_t>::max() / planeHeight) {
		throw std::overflow_error("4:2:0 plane size overflow");
	}
	return planeWidth * planeHeight;
}

[[nodiscard]] std::uint8_t
planeValue(const std::span<const std::uint8_t> plane, const std::uint16_t width,
	const std::uint16_t height, const std::uint16_t x, const std::uint16_t y)
{
	if (x >= width || y >= height) {
		throw std::out_of_range("4:2:0 plane coordinate is outside the frame");
	}
	return plane[static_cast<std::size_t>(y) * width + x];
}

void
validatePlanes(const std::uint16_t width, const std::uint16_t height,
	const std::span<const std::uint8_t> lumas,
	const std::span<const std::uint8_t> redDifferences,
	const std::span<const std::uint8_t> blueDifferences)
{
	if (lumas.size() != checkedPlaneSize(width, height, false)
	    || redDifferences.size() != checkedPlaneSize(width, height, true)
	    || blueDifferences.size() != checkedPlaneSize(width, height, true)) {
		throw std::invalid_argument("4:2:0 plane size does not match dimensions");
	}
}

} // namespace

LumaColourDifference420View::LumaColourDifference420View(
	const std::uint16_t width, const std::uint16_t height,
	const std::span<const std::uint8_t> lumas,
	const std::span<const std::uint8_t> redDifferences,
	const std::span<const std::uint8_t> blueDifferences)
	: blueDifferenceValues(blueDifferences), heightValue(height), lumaValues(lumas),
	  redDifferenceValues(redDifferences), widthValue(width)
{
	validatePlanes(width, height, lumas, redDifferences, blueDifferences);
}

std::uint8_t
LumaColourDifference420View::blueDifference(const std::uint16_t x,
	const std::uint16_t y) const
{
	return planeValue(blueDifferenceValues, chromaWidth(), chromaHeight(), x, y);
}

std::span<const std::uint8_t>
LumaColourDifference420View::blueDifferences() const noexcept
{
	return blueDifferenceValues;
}

std::uint16_t
LumaColourDifference420View::chromaHeight() const noexcept
{
	return static_cast<std::uint16_t>(heightValue / 2U);
}

std::uint16_t
LumaColourDifference420View::chromaWidth() const noexcept
{
	return static_cast<std::uint16_t>(widthValue / 2U);
}

std::uint16_t
LumaColourDifference420View::height() const noexcept
{
	return heightValue;
}

std::uint8_t
LumaColourDifference420View::luma(const std::uint16_t x, const std::uint16_t y) const
{
	return planeValue(lumaValues, widthValue, heightValue, x, y);
}

std::span<const std::uint8_t>
LumaColourDifference420View::lumas() const noexcept
{
	return lumaValues;
}

std::uint8_t
LumaColourDifference420View::redDifference(const std::uint16_t x,
	const std::uint16_t y) const
{
	return planeValue(redDifferenceValues, chromaWidth(), chromaHeight(), x, y);
}

std::span<const std::uint8_t>
LumaColourDifference420View::redDifferences() const noexcept
{
	return redDifferenceValues;
}

std::uint16_t
LumaColourDifference420View::width() const noexcept
{
	return widthValue;
}

LumaColourDifference420Frame::LumaColourDifference420Frame(
	const std::uint16_t width, const std::uint16_t height,
	std::vector<std::uint8_t> lumas, std::vector<std::uint8_t> redDifferences,
	std::vector<std::uint8_t> blueDifferences)
	: blueDifferenceValues(std::move(blueDifferences)), heightValue(height),
	  lumaValues(std::move(lumas)), redDifferenceValues(std::move(redDifferences)),
	  widthValue(width)
{
	validatePlanes(width, height, lumaValues, redDifferenceValues,
	    blueDifferenceValues);
}

LumaColourDifference420View
LumaColourDifference420Frame::view() const
{
	return LumaColourDifference420View(widthValue, heightValue, lumaValues,
	    redDifferenceValues, blueDifferenceValues);
}

} // namespace sstv::analog
