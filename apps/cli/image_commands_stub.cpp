// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/image_commands_stub.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "image_commands.hpp"

#include <iostream>

bool
isImageCommand(const std::string_view argument) noexcept
{
	return argument == "prepare-image" || argument == "encode-image";
}

void
printImageCommandHelp()
{
	std::cout << "  prepare-image / encode-image: unavailable in this minimal build\n";
}

int
runImageCommand(const int, char*[])
{
	std::cerr << "Error: image support was not built; enable SSTV_BUILD_IMAGE\n";
	return 1;
}
