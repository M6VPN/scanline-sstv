// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/core/mode.hpp>
#include <sstv/core/version.hpp>

#include <cassert>

int main() {
    static_assert(sstv::core::version_major == 0);
    assert(!sstv::core::version_string.empty());

    // On-air descriptors are gated on M1 protocol provenance and golden vectors.
    assert(sstv::core::built_in_modes().empty());
    assert(sstv::core::find_mode("martin-m1") == nullptr);
    return 0;
}

