// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/paired_luma_chroma.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/paired_luma_chroma.hpp>

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
	if (width == 0U || height == 0U || height % 2U != 0U) {
		throw std::invalid_argument(
		    "4:4:0 frame dimensions must be nonzero with even height");
	}
	const std::size_t planeHeight = isChroma ? height / 2U : height;
	if (width > std::numeric_limits<std::size_t>::max() / planeHeight) {
		throw std::overflow_error("4:4:0 plane size overflow");
	}
	return static_cast<std::size_t>(width) * planeHeight;
}

[[nodiscard]] std::uint8_t
planeValue(const std::span<const std::uint8_t> plane, const std::uint16_t width,
	const std::uint16_t height, const std::uint16_t x, const std::uint16_t y)
{
	if (x >= width || y >= height) {
		throw std::out_of_range("4:4:0 plane coordinate is outside the frame");
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
		throw std::invalid_argument("4:4:0 plane size does not match dimensions");
	}
}

} // namespace

LumaColourDifference440View::LumaColourDifference440View(
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
LumaColourDifference440View::blueDifference(const std::uint16_t x,
	const std::uint16_t y) const
{
	return planeValue(blueDifferenceValues, chromaWidth(), chromaHeight(), x, y);
}

std::span<const std::uint8_t>
LumaColourDifference440View::blueDifferences() const noexcept
{
	return blueDifferenceValues;
}

std::uint16_t
LumaColourDifference440View::chromaHeight() const noexcept
{
	return static_cast<std::uint16_t>(heightValue / 2U);
}

std::uint16_t
LumaColourDifference440View::chromaWidth() const noexcept
{
	return widthValue;
}

std::uint16_t
LumaColourDifference440View::height() const noexcept
{
	return heightValue;
}

std::uint8_t
LumaColourDifference440View::luma(const std::uint16_t x, const std::uint16_t y) const
{
	return planeValue(lumaValues, widthValue, heightValue, x, y);
}

std::span<const std::uint8_t>
LumaColourDifference440View::lumas() const noexcept
{
	return lumaValues;
}

std::uint8_t
LumaColourDifference440View::redDifference(const std::uint16_t x,
	const std::uint16_t y) const
{
	return planeValue(redDifferenceValues, chromaWidth(), chromaHeight(), x, y);
}

std::span<const std::uint8_t>
LumaColourDifference440View::redDifferences() const noexcept
{
	return redDifferenceValues;
}

std::uint16_t
LumaColourDifference440View::width() const noexcept
{
	return widthValue;
}

LumaColourDifference440Frame::LumaColourDifference440Frame(
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

LumaColourDifference440View
LumaColourDifference440Frame::view() const
{
	return LumaColourDifference440View(widthValue, heightValue, lumaValues,
	    redDifferenceValues, blueDifferenceValues);
}

} // namespace sstv::analog
