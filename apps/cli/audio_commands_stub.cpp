// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/audio_commands_stub.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_commands.hpp"

#include <iostream>

bool
isAudioCommand(const std::string_view argument) noexcept
{
	return argument == "list-audio";
}

void
printAudioCommandHelp()
{
	std::cout << "  list-audio: unavailable in this audio-disabled build\n";
}

int
runAudioCommand(const int, char*[])
{
	std::cerr << "Error: audio discovery was not built; enable SSTV_BUILD_AUDIO\n";
	return 1;
}
