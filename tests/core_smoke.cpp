// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/core_smoke.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/core/mode.hpp>
#include <sstv/core/version.hpp>

#include <cassert>

int main() {
    static_assert(sstv::core::version_major == 0);
    assert(!sstv::core::version_string.empty());

	const auto modes = sstv::core::built_in_modes();
	assert(modes.size() == 1);
	const auto* martin = sstv::core::find_mode("martin-m1");
	assert(martin != nullptr);
	assert(martin->has_offline_test_pattern_tx);
	assert(!martin->has_live_tx);
	assert(!martin->has_receive);
    return 0;
}
