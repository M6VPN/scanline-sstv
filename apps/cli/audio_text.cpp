// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/audio_text.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_text.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

[[nodiscard]] bool
isContinuationByte(const unsigned char value)
{
	return (value & 0xC0U) == 0x80U;
}

[[nodiscard]] std::size_t
validUtf8SequenceLength(const std::string_view value, const std::size_t index)
{
	const auto first = static_cast<unsigned char>(value[index]);
	if (first < 0x80U) {
		return first >= 0x20U && first != 0x7FU ? 1 : 0;
	}
	std::size_t length = 0;
	std::uint32_t minimum = 0;
	std::uint32_t codePoint = 0;
	if (first >= 0xC2U && first <= 0xDFU) {
		length = 2;
		minimum = 0x80U;
		codePoint = first & 0x1FU;
	} else if (first >= 0xE0U && first <= 0xEFU) {
		length = 3;
		minimum = 0x800U;
		codePoint = first & 0x0FU;
	} else if (first >= 0xF0U && first <= 0xF4U) {
		length = 4;
		minimum = 0x10000U;
		codePoint = first & 0x07U;
	} else {
		return 0;
	}
	if (index + length > value.size()) {
		return 0;
	}
	for (std::size_t offset = 1; offset < length; ++offset) {
		const auto next = static_cast<unsigned char>(value[index + offset]);
		if (!isContinuationByte(next)) {
			return 0;
		}
		codePoint = (codePoint << 6U) | (next & 0x3FU);
	}
	if (codePoint < minimum || codePoint > 0x10FFFFU
	    || (codePoint >= 0xD800U && codePoint <= 0xDFFFU)) {
		return 0;
	}
	return length;
}

} // namespace

std::string
escapeAudioTerminalText(const std::string_view value)
{
	constexpr std::array hex{'0', '1', '2', '3', '4', '5', '6', '7',
	    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
	std::string escaped;
	for (std::size_t index = 0; index < value.size();) {
		const std::size_t length = validUtf8SequenceLength(value, index);
		if (length != 0) {
			escaped.append(value.substr(index, length));
			index += length;
			continue;
		}
		const auto byte = static_cast<unsigned char>(value[index]);
		escaped.append("\\x");
		escaped.push_back(hex[byte >> 4U]);
		escaped.push_back(hex[byte & 0x0FU]);
		++index;
	}
	return escaped;
}
