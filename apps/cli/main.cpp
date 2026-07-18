// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/main.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/offline_tx.hpp>
#include <sstv/core/mode.hpp>
#include <sstv/core/rgb8_frame.hpp>
#include <sstv/core/version.hpp>
#include <sstv/offline/wav_writer.hpp>

#include "image_commands.hpp"
#include "audio_commands.hpp"
#include "wav_commands.hpp"
#if defined(SSTV_ENABLE_LIVE_TX)
#include "live_tx_commands.hpp"
#endif

#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>

namespace {

constexpr std::uint32_t defaultSampleRate = 48'000;

struct EncodeOptions {
	std::string mode;
	std::filesystem::path output;
	std::optional<sstv::analog::FskIdentifier> fskIdentifier;
	std::uint32_t sampleRate = defaultSampleRate;
	bool force = false;
	bool hasMode = false;
	bool hasOutput = false;
	bool hasSampleRate = false;
	bool hasFskIdentifier = false;
};

void
printHelp()
{
	std::cout
	    << "Usage:\n"
	       "  scanline-sstv-cli [--help] [--version] [--list-modes]\n"
	       "  scanline-sstv-cli encode-test-pattern --mode MODE\n"
	       "      --output OUTPUT.wav [--sample-rate RATE] [--fsk-id TEXT]\n"
	       "      [--force]\n"
	       "\n"
	       "Examples:\n"
	       "  scanline-sstv-cli encode-test-pattern --mode martin-m1\n"
	       "      --output martin-m1.wav\n"
	       "  scanline-sstv-cli encode-test-pattern --mode scottie-s1\n"
	       "      --output scottie-s1.wav\n"
	       "  scanline-sstv-cli encode-test-pattern --mode robot-36\n"
	       "      --output robot-36.wav\n"
	       "  scanline-sstv-cli encode-test-pattern --mode pd-120\n"
	       "      --output pd-120.wav\n"
	       "  scanline-sstv-cli encode-test-pattern --mode martin-m1\n"
	       "      --output martin-m1-id.wav --fsk-id M6VPN\n"
	       "  scanline-sstv-cli encode-image --mode robot-36 --input source.png\n"
	       "      --output robot-36.wav\n"
	       "  scanline-sstv-cli encode-image --mode pd-120 --input source.jpg\n"
	       "      --output pd-120.wav\n"
	       "\n"
	       "Offline generation only. These commands do not\n"
	       "play audio, access a sound card, control a radio, or key PTT.\n";
	printImageCommandHelp();
	printWavCommandHelp();
	printAudioCommandHelp();
#if defined(SSTV_ENABLE_LIVE_TX)
	printLiveTransmitCommandHelp();
#endif
}

[[nodiscard]] std::string_view
colourEncodingName(const sstv::core::ColourEncoding encoding)
{
	switch (encoding) {
	case sstv::core::ColourEncoding::monochrome:
		return "monochrome";
	case sstv::core::ColourEncoding::rgb:
		return "rgb";
	case sstv::core::ColourEncoding::lumaColourDifference:
		return "luma-red-blue-difference";
	case sstv::core::ColourEncoding::digital_payload:
		return "digital-payload";
	}
	throw std::logic_error("invalid colour encoding");
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
		    << (mode.capabilities.contains(sstv::core::ModeCapability::offlineTestPatternTx)
		        ? "offline-test-pattern-tx" : "metadata-only");
		if (mode.capabilities.contains(sstv::core::ModeCapability::offlineImageTx)) {
			std::cout << ",offline-image-tx";
		}
		if (mode.capabilities.contains(sstv::core::ModeCapability::offlineFskIdTx)) {
			std::cout << ",optional-fsk-id";
		}
		std::cout << '\t' << colourEncodingName(mode.colour_encoding) << '\n';
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
		    && argument != "--sample-rate" && argument != "--fsk-id") {
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
		} else if (argument == "--sample-rate") {
			if (options.hasSampleRate) {
				throw std::invalid_argument("duplicate option: --sample-rate");
			}
			options.sampleRate = parseSampleRate(value);
			options.hasSampleRate = true;
		} else {
			if (options.hasFskIdentifier) {
				throw std::invalid_argument("duplicate option: --fsk-id");
			}
			const sstv::analog::FskIdentifierResult result
			    = sstv::analog::validateFskIdentifier(value);
			if (const auto* error
			    = std::get_if<sstv::analog::FskIdError>(&result)) {
				throw std::invalid_argument(error->message);
			}
			options.fskIdentifier
			    = std::get<sstv::analog::FskIdentifier>(result);
			options.hasFskIdentifier = true;
		}
	}
	if (!options.hasMode || !options.hasOutput) {
		throw std::invalid_argument("--mode and --output are required");
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
		const sstv::core::ModeDescriptor* const mode = sstv::core::find_mode(options.mode);
		if (mode == nullptr) {
			throw std::invalid_argument("unknown mode: " + options.mode);
		}
		const sstv::core::Rgb8Frame frame
		    = sstv::core::makeDiagnosticPattern(mode->width, mode->height);
		sstv::analog::OfflineTxResult result = sstv::analog::encodeOfflineTransmission(
		    options.mode, sstv::core::ModeCapability::offlineTestPatternTx,
		    frame.view(), sstv::analog::OfflineTransmissionOptions{
		        0.8F, options.fskIdentifier});
		if (const auto* error = std::get_if<sstv::analog::OfflineTxError>(&result)) {
			throw std::invalid_argument(error->message);
		}
		sstv::analog::OfflineTransmission transmission
		    = std::get<sstv::analog::OfflineTransmission>(std::move(result));
		const sstv::offline::WavMetadata metadata = sstv::offline::writePcm16WavAtomic(
		    options.output, transmission.events, options.sampleRate, options.force);
		const sstv::core::Duration duration = transmission.duration;
		const long double seconds = static_cast<long double>(duration.numerator())
		    / static_cast<long double>(duration.denominator());
		std::cout << "Mode: " << mode->id << '\n'
		    << "Dimensions: " << mode->width << 'x' << mode->height << '\n'
		    << "FSK ID: "
		    << (options.fskIdentifier.has_value()
		            ? "appended (" + std::string(options.fskIdentifier->value()) + ")"
		            : "none")
		    << '\n'
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
	if (isImageCommand(argument)) {
		return runImageCommand(argc, argv);
	}
	if (isWavCommand(argument)) {
		return runWavCommand(argc, argv);
	}
	if (isAudioCommand(argument)) {
		return runAudioCommand(argc, argv);
	}
#if defined(SSTV_ENABLE_LIVE_TX)
	if (argument == "transmit-image") {
		return runLiveTransmitCommand(argc, argv);
	}
#endif
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
