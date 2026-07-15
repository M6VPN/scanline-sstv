// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/core/mode.hpp>
#include <sstv/core/version.hpp>

#include <iostream>
#include <string_view>

namespace {

void print_help() {
    std::cout << "Usage: sstv-cli [--help] [--version] [--list-modes]\n"
                 "\n"
                 "Foundation diagnostic utility. Encode/decode commands arrive in M1/M3.\n";
}

void print_modes() {
    const auto modes = sstv::core::built_in_modes();
    if (modes.empty()) {
        std::cout << "No on-air modes are registered in the M0 foundation build.\n";
        return;
    }

    for (const auto& mode : modes) {
        std::cout << mode.id << '\t' << mode.display_name << '\t' << mode.width << 'x'
                  << mode.height << '\n';
    }
}

} // namespace

int main(const int argc, char* argv[]) {
    if (argc == 1) {
        print_help();
        return 0;
    }

    const std::string_view argument{argv[1]};
    if (argument == "--help" || argument == "-h") {
        print_help();
        return 0;
    }
    if (argument == "--version") {
        std::cout << "scanline-sstv-cli " << sstv::core::version_string << '\n';
        return 0;
    }
    if (argument == "--list-modes") {
        print_modes();
        return 0;
    }

    std::cerr << "Unknown argument: " << argument << "\n\n";
    print_help();
    return 2;
}

