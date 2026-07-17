// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/analog/paired_luma_chroma.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace sstv::analog {

/** Immutable view of full-resolution luma and vertically subsampled colour differences. */
class LumaColourDifference440View {
public:
	LumaColourDifference440View(std::uint16_t, std::uint16_t,
		std::span<const std::uint8_t>, std::span<const std::uint8_t>,
		std::span<const std::uint8_t>);

	[[nodiscard]] std::uint8_t blueDifference(std::uint16_t, std::uint16_t) const;
	[[nodiscard]] std::span<const std::uint8_t> blueDifferences() const noexcept;
	[[nodiscard]] std::uint16_t chromaHeight() const noexcept;
	[[nodiscard]] std::uint16_t chromaWidth() const noexcept;
	[[nodiscard]] std::uint16_t height() const noexcept;
	[[nodiscard]] std::uint8_t luma(std::uint16_t, std::uint16_t) const;
	[[nodiscard]] std::span<const std::uint8_t> lumas() const noexcept;
	[[nodiscard]] std::uint8_t redDifference(std::uint16_t, std::uint16_t) const;
	[[nodiscard]] std::span<const std::uint8_t> redDifferences() const noexcept;
	[[nodiscard]] std::uint16_t width() const noexcept;

private:
	std::span<const std::uint8_t> blueDifferenceValues;
	std::uint16_t heightValue;
	std::span<const std::uint8_t> lumaValues;
	std::span<const std::uint8_t> redDifferenceValues;
	std::uint16_t widthValue;
};

/** Owning immutable full-width, vertically subsampled luma and colour-difference frame. */
class LumaColourDifference440Frame {
public:
	LumaColourDifference440Frame(std::uint16_t, std::uint16_t,
		std::vector<std::uint8_t>, std::vector<std::uint8_t>,
		std::vector<std::uint8_t>);

	[[nodiscard]] LumaColourDifference440View view() const;

private:
	std::vector<std::uint8_t> blueDifferenceValues;
	std::uint16_t heightValue;
	std::vector<std::uint8_t> lumaValues;
	std::vector<std::uint8_t> redDifferenceValues;
	std::uint16_t widthValue;
};

} // namespace sstv::analog
