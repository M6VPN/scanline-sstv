// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/wav_commands.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "wav_commands.hpp"

#include <sstv/offline/wav_inspector.hpp>

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace {

[[nodiscard]] std::filesystem::path
parseInput(const int argc, char* argv[])
{
	std::filesystem::path input;
	bool hasInput = false;
	for (int index = 2; index < argc; ++index) {
		const std::string_view argument{argv[index]};
		if (argument != "--input") {
			throw std::invalid_argument("unexpected argument: "
			    + std::string(argument));
		}
		if (hasInput) {
			throw std::invalid_argument("duplicate option: --input");
		}
		if (++index >= argc || std::string_view(argv[index]).starts_with("--")) {
			throw std::invalid_argument("missing value for --input");
		}
		input = argv[index];
		hasInput = true;
	}
	if (!hasInput || input.empty()) {
		throw std::invalid_argument("--input is required");
	}
	return input;
}

void
printInspection(const sstv::offline::WavInspection& inspection)
{
	const long double seconds
	    = static_cast<long double>(inspection.duration.numerator())
	    / static_cast<long double>(inspection.duration.denominator());
	std::cout.imbue(std::locale::classic());
	std::cout << "Container: RIFF/WAVE\n"
	    << "Format: linear PCM (1)\n"
	    << "Channels: " << inspection.channels << '\n'
	    << "Sample rate: " << inspection.sampleRate << " Hz\n"
	    << "Bits per sample: " << inspection.bitsPerSample << '\n'
	    << "Data bytes: " << inspection.dataBytes << '\n'
	    << "Frame count: " << inspection.frameCount << '\n'
	    << "Duration: " << std::fixed << std::setprecision(6) << seconds
	    << " seconds\n"
	    << "Minimum sample: " << inspection.minimumSample << '\n'
	    << "Maximum sample: " << inspection.maximumSample << '\n'
	    << "Peak absolute level: " << inspection.peakAbsolute << '\n'
	    << "DC mean: " << inspection.dcMean << '\n'
	    << "RMS level: " << inspection.rmsLevel << '\n'
	    << "Clipped positive samples: " << inspection.clippedPositiveSamples << '\n'
	    << "Clipped negative samples: " << inspection.clippedNegativeSamples << '\n';
}

} // namespace

bool
isWavCommand(const std::string_view argument) noexcept
{
	return argument == "inspect-wav";
}

void
printWavCommandHelp()
{
	std::cout << "  scanline-sstv-cli inspect-wav --input FILE\n";
}

int
runWavCommand(const int argc, char* argv[])
{
	try {
		const std::filesystem::path input = parseInput(argc, argv);
		const sstv::offline::WavInspectionResult result
		    = sstv::offline::inspectPcm16Wav(
		        input, sstv::offline::defaultWavInspectionLimits());
		if (const auto* error
		    = std::get_if<sstv::offline::WavInspectError>(&result)) {
			std::cerr << "Error: " << error->operation << ": "
			    << error->message << '\n';
			return 1;
		}
		printInspection(std::get<sstv::offline::WavInspection>(result));
		return 0;
	} catch (const std::invalid_argument& error) {
		std::cerr << "Error: " << error.what() << '\n';
		return 2;
	} catch (const std::exception& error) {
		std::cerr << "Error: " << error.what() << '\n';
		return 1;
	}
}
