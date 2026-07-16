// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/core_smoke.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/core/mode.hpp>
#include <sstv/core/version.hpp>

#include <cassert>

int
main()
{
	static_assert(sstv::core::version_major == 0);
	assert(!sstv::core::version_string.empty());
	const auto modes = sstv::core::built_in_modes();
	assert(modes.size() == 2);
	const auto* martin = sstv::core::find_mode("martin-m1");
	assert(martin != nullptr);
	assert(martin->capabilities.contains(sstv::core::ModeCapability::offlineTestPatternTx));
	assert(martin->capabilities.contains(sstv::core::ModeCapability::offlineImageTx));
	assert(!martin->capabilities.contains(sstv::core::ModeCapability::liveTx));
	assert(!martin->capabilities.contains(sstv::core::ModeCapability::receive));
	const auto* scottie = sstv::core::find_mode("scottie-s1");
	assert(scottie != nullptr);
	assert(scottie->vis_code == 60);
	assert(scottie->capabilities.contains(
	    sstv::core::ModeCapability::offlineTestPatternTx));
	assert(scottie->capabilities.contains(sstv::core::ModeCapability::offlineImageTx));
	assert(!scottie->capabilities.contains(sstv::core::ModeCapability::liveTx));
	assert(!scottie->capabilities.contains(sstv::core::ModeCapability::receive));
	constexpr auto combined = sstv::core::ModeCapability::offlineTestPatternTx
	    | sstv::core::ModeCapability::offlineImageTx
	    | sstv::core::ModeCapability::liveTx;
	static_assert(combined.contains(sstv::core::ModeCapability::liveTx));
	return 0;
}
