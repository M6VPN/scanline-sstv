// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/core/mode.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

namespace sstv::core {

enum class ModeFamily : std::uint8_t {
    analog,
    hamdrm,
    kgstv,
};

enum class ColourEncoding : std::uint8_t {
    monochrome,
    rgb,
    yuv,
    digital_payload,
};

struct ModeDescriptor {
	std::string_view id;
	std::string_view display_name;
	ModeFamily family;
	ColourEncoding colour_encoding;
	std::uint16_t width;
	std::uint16_t height;
	std::optional<std::uint8_t> vis_code;
	bool has_offline_test_pattern_tx;
	bool has_live_tx;
	bool has_receive;
};

[[nodiscard]] std::span<const ModeDescriptor> built_in_modes() noexcept;
[[nodiscard]] const ModeDescriptor* find_mode(std::string_view id) noexcept;

} // namespace sstv::core
