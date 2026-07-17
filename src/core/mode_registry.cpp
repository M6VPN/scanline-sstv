// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/mode_registry.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/core/mode.hpp>

#include <array>

namespace sstv::core {
namespace {

/* Each offline mode is registered only after its evidence and vector gate passes. */
constexpr std::array<ModeDescriptor, 4> modes{{
	{
		"martin-m1",
		"Martin M1",
		ModeFamily::analog,
		ColourEncoding::rgb,
		320,
		256,
		44,
		ModeCapability::offlineTestPatternTx | ModeCapability::offlineImageTx
		    | ModeCapability::offlineFskIdTx,
		OfflineTxStrategy::martinM1,
	},
	{
		"scottie-s1",
		"Scottie S1",
		ModeFamily::analog,
		ColourEncoding::rgb,
		320,
		256,
		60,
		ModeCapability::offlineTestPatternTx | ModeCapability::offlineImageTx
		    | ModeCapability::offlineFskIdTx,
		OfflineTxStrategy::scottieS1,
	},
	{
		"robot-36",
		"Robot 36",
		ModeFamily::analog,
		ColourEncoding::lumaColourDifference,
		320,
		240,
		8,
		ModeCapability::offlineTestPatternTx | ModeCapability::offlineImageTx
		    | ModeCapability::offlineFskIdTx,
		OfflineTxStrategy::robot36,
	},
	{
		"pd-120",
		"PD120",
		ModeFamily::analog,
		ColourEncoding::lumaColourDifference,
		640,
		496,
		95,
		ModeCapability::offlineTestPatternTx | ModeCapability::offlineImageTx
		    | ModeCapability::offlineFskIdTx,
		OfflineTxStrategy::pd120,
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
