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
	lumaColourDifference,
	digital_payload,
};

enum class ModeCapability : std::uint32_t {
	offlineTestPatternTx = 1U << 0U,
	offlineImageTx = 1U << 1U,
	liveTx = 1U << 2U,
	receive = 1U << 3U,
	offlineFskIdTx = 1U << 4U,
};

enum class OfflineTxStrategy : std::uint8_t {
	none,
	martinM1,
	scottieS1,
	robot36,
	pd120,
};

class ModeCapabilities {
public:
	constexpr ModeCapabilities() noexcept = default;
	constexpr ModeCapabilities(const ModeCapability capability) noexcept
		: bits(static_cast<std::uint32_t>(capability))
	{
	}
	constexpr ModeCapabilities(const ModeCapability left,
		const ModeCapability right) noexcept
		: bits(static_cast<std::uint32_t>(left)
		    | static_cast<std::uint32_t>(right))
	{
	}
	[[nodiscard]] constexpr bool contains(const ModeCapability capability) const noexcept
	{
		return (bits & static_cast<std::uint32_t>(capability)) != 0U;
	}
	[[nodiscard]] constexpr ModeCapabilities with(
		const ModeCapability capability) const noexcept
	{
		ModeCapabilities result;
		result.bits = bits | static_cast<std::uint32_t>(capability);
		return result;
	}
private:
	std::uint32_t bits = 0;
};

[[nodiscard]] constexpr ModeCapabilities operator|(
	const ModeCapability left, const ModeCapability right) noexcept
{
	return ModeCapabilities(left, right);
}

[[nodiscard]] constexpr ModeCapabilities operator|(
	const ModeCapabilities left, const ModeCapability right) noexcept
{
	return left.with(right);
}

struct ModeDescriptor {
	std::string_view id;
	std::string_view display_name;
	ModeFamily family;
	ColourEncoding colour_encoding;
	std::uint16_t width;
	std::uint16_t height;
	std::optional<std::uint8_t> vis_code;
	ModeCapabilities capabilities;
	OfflineTxStrategy offline_tx_strategy;
};

[[nodiscard]] std::span<const ModeDescriptor> built_in_modes() noexcept;
[[nodiscard]] const ModeDescriptor* find_mode(std::string_view id) noexcept;

} // namespace sstv::core
