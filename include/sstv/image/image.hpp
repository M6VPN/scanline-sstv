// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/image/image.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/core/rgb8_frame.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <variant>

namespace sstv::image {

enum class FitMode : std::uint8_t {
	contain,
	cover,
};

enum class RasterFormat : std::uint8_t {
	jpeg,
	png,
};

enum class ExifOrientation : std::uint8_t {
	identity = 1,
	flipHorizontal = 2,
	rotate180 = 3,
	flipVertical = 4,
	transpose = 5,
	rotate90Clockwise = 6,
	transverse = 7,
	rotate270Clockwise = 8,
};

enum class ImageErrorCode : std::uint8_t {
	invalidPath,
	unsupportedLoader,
	fileTooLarge,
	invalidMetadata,
	dimensionLimit,
	pixelLimit,
	pageLimit,
	invalidOrientation,
	invalidCrop,
	unsupportedColourspace,
	invalidProfile,
	decodeFailure,
	invalidRecipe,
	destinationExists,
	outputFailure,
};

struct ImageError {
	ImageErrorCode code;
	std::string operation;
	std::string message;
};

template<typename Value>
using ImageResult = std::variant<Value, ImageError>;

struct CropRect {
	std::uint64_t x;
	std::uint64_t y;
	std::uint64_t width;
	std::uint64_t height;
};

struct ImageLoadLimits {
	std::uint64_t maximumFileBytes;
	std::uint32_t maximumWidth;
	std::uint32_t maximumHeight;
	std::uint64_t maximumDecodedPixels;
	std::uint32_t maximumPages;
	std::uint32_t maximumOutputWidth;
	std::uint32_t maximumOutputHeight;
};

struct ImageRecipe {
	std::uint16_t targetWidth;
	std::uint16_t targetHeight;
	FitMode fit;
	std::optional<CropRect> crop;
	core::Rgb8Pixel background;
	bool applyExifOrientation;
};

struct SourceImageInfo {
	RasterFormat format;
	std::uint32_t width;
	std::uint32_t height;
	std::uint32_t orientedWidth;
	std::uint32_t orientedHeight;
	ExifOrientation orientation;
	std::uint8_t bitsPerSample;
	bool hasAlpha;
	bool hasEmbeddedProfile;
};

struct PreparedImage {
	core::Rgb8Frame frame;
	SourceImageInfo source;
	std::optional<CropRect> appliedCrop;
	FitMode fit;
};

struct PngMetadata {
	std::uint32_t width;
	std::uint32_t height;
};

[[nodiscard]] ImageLoadLimits defaultImageLoadLimits() noexcept;
[[nodiscard]] ImageResult<PreparedImage> prepareRasterImage(
	const std::filesystem::path&, const ImageRecipe&, const ImageLoadLimits&);
[[nodiscard]] ImageResult<PngMetadata> writeRgb8PngAtomic(
	const std::filesystem::path&, core::Rgb8View, bool);

} // namespace sstv::image
