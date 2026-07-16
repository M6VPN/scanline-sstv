// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/main.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/martin_m1.hpp>
#include <sstv/core/mode.hpp>
#include <sstv/core/rgb8_frame.hpp>
#include <sstv/core/version.hpp>
#include <sstv/offline/wav_writer.hpp>

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace {

constexpr std::uint32_t defaultSampleRate = 48'000;

struct EncodeOptions {
	std::string mode;
	std::filesystem::path output;
	std::uint32_t sampleRate = defaultSampleRate;
	bool force = false;
	bool hasMode = false;
	bool hasOutput = false;
	bool hasSampleRate = false;
};

void
printHelp()
{
	std::cout
	    << "Usage:\n"
	       "  scanline-sstv-cli [--help] [--version] [--list-modes]\n"
	       "  scanline-sstv-cli encode-test-pattern --mode martin-m1\n"
	       "      --output OUTPUT.wav [--sample-rate RATE] [--force]\n"
	       "\n"
	       "encode-test-pattern creates offline diagnostic audio only. It does not\n"
	       "play audio, access a sound card, control a radio, or key PTT.\n";
}

void
printModes()
{
	const auto modes = sstv::core::built_in_modes();
	if (modes.empty()) {
		std::cout << "No evidence-approved modes are registered.\n";
		return;
	}
	for (const auto& mode : modes) {
		std::cout << mode.id << '\t' << mode.display_name << '\t'
		    << mode.width << 'x' << mode.height << '\t'
		    << (mode.has_offline_test_pattern_tx ? "offline-test-pattern-tx" : "metadata-only")
		    << '\n';
	}
}

[[nodiscard]] std::uint32_t
parseSampleRate(const std::string_view value)
{
	std::uint32_t rate = 0;
	const char* const first = value.data();
	const char* const last = value.data() + value.size();
	const auto [end, error] = std::from_chars(first, last, rate);
	if (value.empty() || error != std::errc{} || end != last
	    || !sstv::offline::isSupportedSampleRate(rate)) {
		throw std::invalid_argument("invalid or unsupported sample rate: "
		    + std::string(value));
	}
	return rate;
}

[[nodiscard]] EncodeOptions
parseEncodeOptions(const int argc, char* argv[])
{
	EncodeOptions options;
	for (int index = 2; index < argc; ++index) {
		const std::string_view argument{argv[index]};
		if (argument == "--force") {
			if (options.force) {
				throw std::invalid_argument("duplicate option: --force");
			}
			options.force = true;
			continue;
		}
		if (argument != "--mode" && argument != "--output"
		    && argument != "--sample-rate") {
			throw std::invalid_argument("unexpected argument: " + std::string(argument));
		}
		if (++index >= argc) {
			throw std::invalid_argument("missing value for " + std::string(argument));
		}
		const std::string_view value{argv[index]};
		if (value.starts_with("--")) {
			throw std::invalid_argument("missing value for " + std::string(argument));
		}
		if (argument == "--mode") {
			if (options.hasMode) {
				throw std::invalid_argument("duplicate option: --mode");
			}
			options.mode = value;
			options.hasMode = true;
		} else if (argument == "--output") {
			if (options.hasOutput) {
				throw std::invalid_argument("duplicate option: --output");
			}
			options.output = value;
			options.hasOutput = true;
		} else {
			if (options.hasSampleRate) {
				throw std::invalid_argument("duplicate option: --sample-rate");
			}
			options.sampleRate = parseSampleRate(value);
			options.hasSampleRate = true;
		}
	}
	if (!options.hasMode || !options.hasOutput) {
		throw std::invalid_argument("--mode and --output are required");
	}
	if (options.mode != "martin-m1") {
		throw std::invalid_argument("unknown mode: " + options.mode);
	}
	if (options.output.empty()) {
		throw std::invalid_argument("output path must not be empty");
	}
	return options;
}

int
encodeTestPattern(const int argc, char* argv[])
{
	try {
		const EncodeOptions options = parseEncodeOptions(argc, argv);
		const auto& descriptor = sstv::analog::martinM1Descriptor();
		const sstv::core::Rgb8Frame frame
		    = sstv::core::makeDiagnosticPattern(descriptor.width, descriptor.height);
		const std::vector<sstv::core::ToneEvent> events
		    = sstv::analog::encodeMartinM1(frame.view(), 0.8F);
		const sstv::offline::WavMetadata metadata = sstv::offline::writePcm16WavAtomic(
		    options.output, events, options.sampleRate, options.force);
		const sstv::core::Duration duration = sstv::analog::martinM1TransmissionDuration();
		const long double seconds = static_cast<long double>(duration.numerator())
		    / static_cast<long double>(duration.denominator());
		std::cout << "Mode: " << descriptor.id << '\n'
		    << "Dimensions: " << descriptor.width << 'x' << descriptor.height << '\n'
		    << "Sample rate: " << metadata.sampleRate << " Hz\n"
		    << "Frame count: " << metadata.frameCount << '\n'
		    << "Duration: " << std::fixed << std::setprecision(6)
		    << seconds << " seconds\n";
		return 0;
	} catch (const std::invalid_argument& error) {
		std::cerr << "Error: " << error.what() << '\n';
		return 2;
	} catch (const std::exception& error) {
		std::cerr << "Error: " << error.what() << '\n';
		return 1;
	}
}

} // namespace

int
main(const int argc, char* argv[])
{
	if (argc == 1) {
		printHelp();
		return 0;
	}
	const std::string_view argument{argv[1]};
	if (argument == "encode-test-pattern") {
		return encodeTestPattern(argc, argv);
	}
	if (argc != 2) {
		std::cerr << "Error: unexpected extra arguments\n";
		return 2;
	}
	if (argument == "--help" || argument == "-h") {
		printHelp();
		return 0;
	}
	if (argument == "--version") {
		std::cout << "scanline-sstv-cli " << sstv::core::version_string << '\n';
		return 0;
	}
	if (argument == "--list-modes") {
		printModes();
		return 0;
	}
	std::cerr << "Error: unknown argument: " << argument << '\n';
	return 2;
}
