// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/rgb8_frame.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/core/rgb8_frame.hpp>

#include <array>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace sstv::core {
namespace {

constexpr std::array<Rgb8Pixel, 8> colourBars{{
	{255, 255, 255},
	{255, 255, 0},
	{0, 255, 255},
	{0, 255, 0},
	{255, 0, 255},
	{255, 0, 0},
	{0, 0, 255},
	{0, 0, 0},
}};

[[nodiscard]] std::uint8_t
scaleCoordinate(const std::uint16_t value, const std::uint16_t maximum)
{
	if (maximum == 0) {
		return 0;
	}
	const std::uint32_t scaled = static_cast<std::uint32_t>(value) * 255U
	    + static_cast<std::uint32_t>(maximum) / 2U;
	return static_cast<std::uint8_t>(scaled / maximum);
}

[[nodiscard]] Rgb8Pixel
basePatternPixel(const std::uint16_t x, const std::uint16_t y,
    const std::uint16_t width, const std::uint16_t height)
{
	const std::uint16_t quarter = static_cast<std::uint16_t>(height / 4U);
	const std::uint16_t maximumX = static_cast<std::uint16_t>(width - 1U);
	if (y < quarter) {
		const std::size_t bar = static_cast<std::size_t>(x) * colourBars.size() / width;
		return colourBars[bar];
	}
	if (y < quarter * 2U) {
		const std::uint8_t level = scaleCoordinate(x, maximumX);
		return {level, level, level};
	}
	if (y < quarter * 3U) {
		const std::uint8_t horizontal = scaleCoordinate(x, maximumX);
		const std::uint16_t maximumY = static_cast<std::uint16_t>(quarter - 1U);
		const std::uint8_t vertical = scaleCoordinate(
		    static_cast<std::uint16_t>(y - quarter * 2U), maximumY);
		return {horizontal, vertical, static_cast<std::uint8_t>(255U - horizontal)};
	}
	const bool light = ((x / 16U) + (y / 16U)) % 2U == 0U;
	return light ? Rgb8Pixel{224, 224, 224} : Rgb8Pixel{32, 32, 32};
}

[[nodiscard]] Rgb8Pixel
diagnosticPixel(const std::uint16_t x, const std::uint16_t y,
    const std::uint16_t width, const std::uint16_t height)
{
	constexpr std::uint16_t cornerSize = 12;
	if (x < cornerSize && y < cornerSize) {
		return {255, 0, 0};
	}
	if (x >= width - cornerSize && y < cornerSize) {
		return {0, 255, 0};
	}
	if (x < cornerSize && y >= height - cornerSize) {
		return {0, 0, 255};
	}
	if (x >= width - cornerSize && y >= height - cornerSize) {
		return {255, 255, 255};
	}
	if (y % 32U == 0U && (x < 8U || x >= width - 8U)) {
		const bool white = (y / 32U) % 2U == 0U;
		return white ? Rgb8Pixel{255, 255, 255} : Rgb8Pixel{0, 0, 0};
	}
	return basePatternPixel(x, y, width, height);
}

[[nodiscard]] std::size_t
framePixelCount(const std::uint16_t width, const std::uint16_t height)
{
	if (width == 0U || height == 0U) {
		throw std::invalid_argument("RGB8 frame dimensions must be nonzero");
	}
	return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
}

} // namespace

Rgb8View::Rgb8View(const std::uint16_t width, const std::uint16_t height,
    const std::span<const Rgb8Pixel> pixels)
	: heightValue(height), pixelValues(pixels), widthValue(width)
{
	if (pixels.size() != framePixelCount(width, height)) {
		throw std::invalid_argument("RGB8 pixel count does not match dimensions");
	}
}

std::uint16_t
Rgb8View::height() const noexcept
{
	return heightValue;
}

const Rgb8Pixel&
Rgb8View::pixel(const std::uint16_t x, const std::uint16_t y) const
{
	if (x >= widthValue || y >= heightValue) {
		throw std::out_of_range("RGB8 pixel coordinate is outside the frame");
	}
	return pixelValues[static_cast<std::size_t>(y) * widthValue + x];
}

std::span<const Rgb8Pixel>
Rgb8View::pixels() const noexcept
{
	return pixelValues;
}

std::uint16_t
Rgb8View::width() const noexcept
{
	return widthValue;
}

Rgb8Frame::Rgb8Frame(const std::uint16_t width, const std::uint16_t height,
    std::vector<Rgb8Pixel> pixels)
	: heightValue(height), pixelValues(std::move(pixels)), widthValue(width)
{
	if (pixelValues.size() != framePixelCount(width, height)) {
		throw std::invalid_argument("RGB8 pixel count does not match dimensions");
	}
}

Rgb8View
Rgb8Frame::view() const
{
	return Rgb8View(widthValue, heightValue, pixelValues);
}

Rgb8Frame
makeDiagnosticPattern(const std::uint16_t width, const std::uint16_t height)
{
	if (width < 24U || height < 24U) {
		throw std::invalid_argument("diagnostic pattern dimensions must be at least 24 by 24");
	}
	std::vector<Rgb8Pixel> pixels(framePixelCount(width, height));
	for (std::uint16_t y = 0; y < height; ++y) {
		for (std::uint16_t x = 0; x < width; ++x) {
			pixels[static_cast<std::size_t>(y) * width + x]
			    = diagnosticPixel(x, y, width, height);
		}
	}
	return Rgb8Frame(width, height, std::move(pixels));
}

} // namespace sstv::core
