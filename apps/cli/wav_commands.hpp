// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/wav_commands.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

[[nodiscard]] bool isWavCommand(std::string_view) noexcept;
void printWavCommandHelp();
[[nodiscard]] int runWavCommand(int, char*[]);
