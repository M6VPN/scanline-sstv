// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/core/rgb8_frame.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace sstv::core {

struct Rgb8Pixel {
	std::uint8_t red;
	std::uint8_t green;
	std::uint8_t blue;
};

/** Validated non-owning immutable RGB8 image view. */
class Rgb8View {
public:
	Rgb8View(std::uint16_t, std::uint16_t, std::span<const Rgb8Pixel>);

	[[nodiscard]] std::uint16_t height() const noexcept;
	[[nodiscard]] const Rgb8Pixel& pixel(std::uint16_t, std::uint16_t) const;
	[[nodiscard]] std::span<const Rgb8Pixel> pixels() const noexcept;
	[[nodiscard]] std::uint16_t width() const noexcept;

private:
	std::uint16_t heightValue;
	std::span<const Rgb8Pixel> pixelValues;
	std::uint16_t widthValue;
};

/** Owning immutable RGB8 frame. Pixel storage is exposed only through const views. */
class Rgb8Frame {
public:
	Rgb8Frame(std::uint16_t, std::uint16_t, std::vector<Rgb8Pixel>);

	[[nodiscard]] Rgb8View view() const;

private:
	std::uint16_t heightValue;
	std::vector<Rgb8Pixel> pixelValues;
	std::uint16_t widthValue;
};

/** Create the deterministic M1a bars, gradients, corners, and line-marker pattern. */
[[nodiscard]] Rgb8Frame makeDiagnosticPattern(std::uint16_t, std::uint16_t);

} // namespace sstv::core
