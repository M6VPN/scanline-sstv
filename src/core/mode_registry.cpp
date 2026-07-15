// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/core/mode.hpp>

#include <array>

namespace sstv::core {
namespace {

// M0 deliberately contains no on-air descriptors. M1 adds each descriptor together with
// protocol provenance and golden encode/decode vectors.
constexpr std::array<ModeDescriptor, 0> modes{};

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

