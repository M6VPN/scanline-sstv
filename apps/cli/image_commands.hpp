// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/image_commands.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

[[nodiscard]] bool isImageCommand(std::string_view) noexcept;
void printImageCommandHelp();
[[nodiscard]] int runImageCommand(int, char*[]);
