// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/hil_commands.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

[[nodiscard]] bool isHilCommand(std::string_view) noexcept;
void printHilCommandHelp();
[[nodiscard]] int runHilCommand(int, char*[]);
