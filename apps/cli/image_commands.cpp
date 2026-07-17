// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/image_commands.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "image_commands.hpp"

#include <sstv/analog/offline_tx.hpp>
#include <sstv/core/mode.hpp>
#include <sstv/image/image.hpp>
#include <sstv/offline/wav_writer.hpp>

#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <variant>
#include <vector>

namespace {

constexpr std::uint32_t defaultSampleRate = 48'000;

struct ImageCommandOptions {
	std::string mode;
	std::filesystem::path input;
	std::filesystem::path output;
	sstv::image::FitMode fit = sstv::image::FitMode::contain;
	std::optional<sstv::image::CropRect> crop;
	std::optional<sstv::analog::FskIdentifier> fskIdentifier;
	sstv::core::Rgb8Pixel background{0, 0, 0};
	std::uint32_t sampleRate = defaultSampleRate;
	bool force = false;
	bool hasMode = false;
	bool hasInput = false;
	bool hasOutput = false;
	bool hasFit = false;
	bool hasCrop = false;
	bool hasBackground = false;
	bool hasSampleRate = false;
	bool hasFskIdentifier = false;
};

[[nodiscard]] std::uint64_t
parseUnsigned(const std::string_view value, const std::string_view name)
{
	std::uint64_t number = 0;
	const char* const first = value.data();
	const char* const last = value.data() + value.size();
	const auto [end, error] = std::from_chars(first, last, number);
	if (value.empty() || error != std::errc{} || end != last) {
		throw std::invalid_argument("invalid " + std::string(name) + ": "
		    + std::string(value));
	}
	return number;
}

[[nodiscard]] std::uint32_t
parseSampleRate(const std::string_view value)
{
	const std::uint64_t parsed = parseUnsigned(value, "sample rate");
	if (parsed > std::numeric_limits<std::uint32_t>::max()
	    || !sstv::offline::isSupportedSampleRate(static_cast<std::uint32_t>(parsed))) {
		throw std::invalid_argument("invalid or unsupported sample rate: "
		    + std::string(value));
	}
	return static_cast<std::uint32_t>(parsed);
}

[[nodiscard]] sstv::image::CropRect
parseCrop(const std::string_view value)
{
	std::array<std::uint64_t, 4> values{};
	std::size_t start = 0;
	for (std::size_t index = 0; index < values.size(); ++index) {
		const std::size_t end = value.find(',', start);
		if ((index < values.size() - 1U && end == std::string_view::npos)
		    || (index == values.size() - 1U && end != std::string_view::npos)) {
			throw std::invalid_argument("crop must be X,Y,WIDTH,HEIGHT");
		}
		const std::size_t length = end == std::string_view::npos
		    ? value.size() - start : end - start;
		values[index] = parseUnsigned(value.substr(start, length), "crop");
		start = end == std::string_view::npos ? value.size() : end + 1U;
	}
	if (values[2] == 0U || values[3] == 0U) {
		throw std::invalid_argument("crop width and height must be nonzero");
	}
	return {values[0], values[1], values[2], values[3]};
}

[[nodiscard]] std::uint8_t
hexByte(const std::string_view value)
{
	unsigned int byte = 0;
	const auto [end, error] = std::from_chars(
	    value.data(), value.data() + value.size(), byte, 16);
	if (error != std::errc{} || end != value.data() + value.size() || byte > 255U) {
		throw std::invalid_argument("background must be six hexadecimal digits");
	}
	return static_cast<std::uint8_t>(byte);
}

[[nodiscard]] sstv::core::Rgb8Pixel
parseBackground(const std::string_view value)
{
	if (value.size() != 6U) {
		throw std::invalid_argument("background must be RRGGBB");
	}
	return {
		hexByte(value.substr(0, 2)),
		hexByte(value.substr(2, 2)),
		hexByte(value.substr(4, 2)),
	};
}

void
setValueOption(ImageCommandOptions& options, const std::string_view argument,
	const std::string_view value, const bool isEncode)
{
	if (argument == "--mode") {
		if (options.hasMode) {
			throw std::invalid_argument("duplicate option: --mode");
		}
		options.mode = value;
		options.hasMode = true;
	} else if (argument == "--input") {
		if (options.hasInput) {
			throw std::invalid_argument("duplicate option: --input");
		}
		options.input = value;
		options.hasInput = true;
	} else if (argument == "--output") {
		if (options.hasOutput) {
			throw std::invalid_argument("duplicate option: --output");
		}
		options.output = value;
		options.hasOutput = true;
	} else if (argument == "--fit") {
		if (options.hasFit) {
			throw std::invalid_argument("duplicate option: --fit");
		}
		if (value == "contain") {
			options.fit = sstv::image::FitMode::contain;
		} else if (value == "cover") {
			options.fit = sstv::image::FitMode::cover;
		} else {
			throw std::invalid_argument("fit must be contain or cover");
		}
		options.hasFit = true;
	} else if (argument == "--crop") {
		if (options.hasCrop) {
			throw std::invalid_argument("duplicate option: --crop");
		}
		options.crop = parseCrop(value);
		options.hasCrop = true;
	} else if (argument == "--background") {
		if (options.hasBackground) {
			throw std::invalid_argument("duplicate option: --background");
		}
		options.background = parseBackground(value);
		options.hasBackground = true;
	} else if (argument == "--sample-rate" && isEncode) {
		if (options.hasSampleRate) {
			throw std::invalid_argument("duplicate option: --sample-rate");
		}
		options.sampleRate = parseSampleRate(value);
		options.hasSampleRate = true;
	} else if (argument == "--fsk-id" && isEncode) {
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
	} else {
		throw std::invalid_argument("unexpected option: " + std::string(argument));
	}
}

[[nodiscard]] ImageCommandOptions
parseOptions(const int argc, char* argv[], const bool isEncode)
{
	ImageCommandOptions options;
	for (int index = 2; index < argc; ++index) {
		const std::string_view argument{argv[index]};
		if (argument == "--force") {
			if (options.force) {
				throw std::invalid_argument("duplicate option: --force");
			}
			options.force = true;
			continue;
		}
		if (!argument.starts_with("--")) {
			throw std::invalid_argument("unexpected argument: " + std::string(argument));
		}
		if (++index >= argc) {
			throw std::invalid_argument("missing value for " + std::string(argument));
		}
		const std::string_view value{argv[index]};
		if (value.starts_with("--")) {
			throw std::invalid_argument("missing value for " + std::string(argument));
		}
		setValueOption(options, argument, value, isEncode);
	}
	if (!options.hasMode || !options.hasInput || !options.hasOutput) {
		throw std::invalid_argument("--mode, --input, and --output are required");
	}
	return options;
}

void
rejectSameInputOutput(const ImageCommandOptions& options)
{
	std::error_code error;
	if (std::filesystem::exists(options.output, error)
	    && std::filesystem::equivalent(options.input, options.output, error)) {
		throw std::invalid_argument("input and output must be different files");
	}
	if (error) {
		throw std::system_error(error, "compare input and output");
	}
	const std::filesystem::path input = std::filesystem::weakly_canonical(options.input, error);
	if (error) {
		throw std::system_error(error, "resolve input");
	}
	const std::filesystem::path outputParent = options.output.parent_path().empty()
	    ? std::filesystem::path{"."} : options.output.parent_path();
	const std::filesystem::path output
	    = std::filesystem::weakly_canonical(outputParent, error)
	    / options.output.filename();
	if (error) {
		throw std::system_error(error, "resolve output");
	}
	if (input == output) {
		throw std::invalid_argument("input and output must be different files");
	}
}

[[nodiscard]] std::string_view
formatName(const sstv::image::RasterFormat format) noexcept
{
	return format == sstv::image::RasterFormat::jpeg ? "JPEG" : "PNG";
}

[[nodiscard]] std::string_view
fitName(const sstv::image::FitMode fit) noexcept
{
	return fit == sstv::image::FitMode::contain ? "contain" : "cover";
}

void
printPreparedInfo(const sstv::image::PreparedImage& prepared,
	const std::filesystem::path& output)
{
	std::cout << "Source format: " << formatName(prepared.source.format) << '\n'
	    << "Source dimensions: " << prepared.source.width << 'x'
	    << prepared.source.height << '\n'
	    << "Applied orientation: "
	    << static_cast<unsigned int>(prepared.source.orientation) << '\n'
	    << "Oriented dimensions: " << prepared.source.orientedWidth << 'x'
	    << prepared.source.orientedHeight << '\n';
	if (prepared.appliedCrop.has_value()) {
		std::cout << "Crop: " << prepared.appliedCrop->x << ','
		    << prepared.appliedCrop->y << ',' << prepared.appliedCrop->width
		    << ',' << prepared.appliedCrop->height << '\n';
	} else {
		std::cout << "Crop: none\n";
	}
	std::cout << "Fit: " << fitName(prepared.fit) << '\n'
	    << "Prepared dimensions: " << prepared.frame.view().width() << 'x'
	    << prepared.frame.view().height() << '\n'
	    << "Output: " << output.string() << '\n';
}

[[nodiscard]] int
executeImageCommand(const int argc, char* argv[], const bool isEncode)
{
	try {
		const ImageCommandOptions options = parseOptions(argc, argv, isEncode);
		rejectSameInputOutput(options);
		const auto* const mode = sstv::core::find_mode(options.mode);
		if (mode == nullptr
		    || !mode->capabilities.contains(sstv::core::ModeCapability::offlineImageTx)) {
			throw std::invalid_argument("mode does not support offline image TX");
		}
		const sstv::image::ImageRecipe recipe{
			mode->width,
			mode->height,
			options.fit,
			options.crop,
			options.background,
			true,
		};
		sstv::image::ImageResult<sstv::image::PreparedImage> result
		    = sstv::image::prepareRasterImage(options.input, recipe,
		        sstv::image::defaultImageLoadLimits());
		if (const auto* error = std::get_if<sstv::image::ImageError>(&result)) {
			std::cerr << "Error: " << error->operation << ": " << error->message << '\n';
			return 1;
		}
		sstv::image::PreparedImage prepared
		    = std::get<sstv::image::PreparedImage>(std::move(result));
		if (!isEncode) {
			auto writeResult = sstv::image::writeRgb8PngAtomic(
			    options.output, prepared.frame.view(), options.force);
			if (const auto* error
			    = std::get_if<sstv::image::ImageError>(&writeResult)) {
				std::cerr << "Error: " << error->operation << ": "
				    << error->message << '\n';
				return 1;
			}
			printPreparedInfo(prepared, options.output);
			return 0;
		}
		sstv::analog::OfflineTxResult encodeResult
		    = sstv::analog::encodeOfflineTransmission(options.mode,
		        sstv::core::ModeCapability::offlineImageTx,
		        prepared.frame.view(), sstv::analog::OfflineTransmissionOptions{
		            0.8F, options.fskIdentifier});
		if (const auto* error
		    = std::get_if<sstv::analog::OfflineTxError>(&encodeResult)) {
			throw std::invalid_argument(error->message);
		}
		sstv::analog::OfflineTransmission transmission
		    = std::get<sstv::analog::OfflineTransmission>(std::move(encodeResult));
		const sstv::offline::WavMetadata metadata
		    = sstv::offline::writePcm16WavAtomic(
		        options.output, transmission.events, options.sampleRate, options.force);
		printPreparedInfo(prepared, options.output);
		std::cout << "FSK ID: "
		    << (options.fskIdentifier.has_value()
		            ? "appended (" + std::string(options.fskIdentifier->value()) + ")"
		            : "none")
		    << '\n';
		const sstv::core::Duration duration = transmission.duration;
		const long double seconds = static_cast<long double>(duration.numerator())
		    / static_cast<long double>(duration.denominator());
		std::cout << "Sample rate: " << metadata.sampleRate << " Hz\n"
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

bool
isImageCommand(const std::string_view argument) noexcept
{
	return argument == "prepare-image" || argument == "encode-image";
}

void
printImageCommandHelp()
{
	std::cout
	    << "  scanline-sstv-cli prepare-image --mode MODE --input INPUT\n"
	       "      --output PREPARED.png [--fit contain|cover]\n"
	       "      [--crop X,Y,WIDTH,HEIGHT] [--background RRGGBB] [--force]\n"
	       "  scanline-sstv-cli encode-image --mode MODE --input INPUT\n"
	       "      --output OUTPUT.wav [--fit contain|cover]\n"
	       "      [--crop X,Y,WIDTH,HEIGHT] [--background RRGGBB]\n"
	       "      [--sample-rate RATE] [--fsk-id TEXT] [--force]\n";
}

int
runImageCommand(const int argc, char* argv[])
{
	return executeImageCommand(argc, argv, std::string_view(argv[1]) == "encode-image");
}
