// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/mode_registry.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/core/mode.hpp>

#include <array>

namespace sstv::core {
namespace {

/* Registered after the M1a evidence and independent golden-vector gate passed. */
constexpr std::array<ModeDescriptor, 1> modes{{
	{
		"martin-m1",
		"Martin M1",
		ModeFamily::analog,
		ColourEncoding::rgb,
		320,
		256,
		44,
		ModeCapability::offlineTestPatternTx | ModeCapability::offlineImageTx,
	},
}};

} // namespace

std::span<const ModeDescriptor> built_in_modes() noexcept {
	return modes;
}

const ModeDescriptor* find_mode(const std::string_view id) noexcept {
	for (const auto& mode : modes) {
		if (mode.id == id) {
			return &mode;
		}
	}
	return nullptr;
}

} // namespace sstv::core
