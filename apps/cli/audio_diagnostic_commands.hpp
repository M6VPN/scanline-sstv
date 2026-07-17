// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/audio_diagnostic_commands.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

[[nodiscard]] bool isAudioDiagnosticCommand(std::string_view) noexcept;
void printAudioDiagnosticCommandHelp();
[[nodiscard]] int runAudioDiagnosticCommand(int, char*[]);
