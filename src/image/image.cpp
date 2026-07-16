// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/image/image.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/image/image.hpp>

#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>
#include <vips/vips8>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <charconv>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace sstv::image {
namespace {

using vips::VImage;
using vips::VOption;
using vips::VSource;
using vips::VTarget;

class ImageFailure final : public std::exception {
public:
	explicit ImageFailure(ImageError value)
		: errorValue(std::move(value))
	{
	}

	[[nodiscard]] const char* what() const noexcept override
	{
		return errorValue.message.c_str();
	}

	[[nodiscard]] const ImageError& error() const noexcept
	{
		return errorValue;
	}

private:
	ImageError errorValue;
};

class FileDescriptor {
public:
	explicit FileDescriptor(const int value = -1) noexcept
		: descriptor(value)
	{
	}

	FileDescriptor(const FileDescriptor&) = delete;
	FileDescriptor& operator=(const FileDescriptor&) = delete;

	FileDescriptor(FileDescriptor&& other) noexcept
		: descriptor(std::exchange(other.descriptor, -1))
	{
	}

	FileDescriptor& operator=(FileDescriptor&& other) noexcept
	{
		if (this != &other) {
			close();
			descriptor = std::exchange(other.descriptor, -1);
		}
		return *this;
	}

	~FileDescriptor()
	{
		close();
	}

	void close() noexcept
	{
		if (descriptor >= 0) {
			::close(descriptor);
			descriptor = -1;
		}
	}

	[[nodiscard]] int get() const noexcept
	{
		return descriptor;
	}

private:
	int descriptor;
};

struct FileIdentity {
	dev_t device;
	ino_t inode;
	off_t size;
};

struct OpenInput {
	FileDescriptor descriptor;
	FileIdentity identity;
};

struct HeaderInfo {
	std::filesystem::path canonicalPath;
	FileIdentity identity;
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

struct ResizeGeometry {
	std::uint32_t width;
	std::uint32_t height;
	std::uint32_t left;
	std::uint32_t top;
};

[[noreturn]] void
fail(const ImageErrorCode code, std::string operation, std::string message)
{
	throw ImageFailure({code, std::move(operation), std::move(message)});
}

void
ensureVips()
{
	static std::once_flag flag;
	static int result = -1;
	std::call_once(flag, [] {
		result = vips_init("scanline-sstv");
	});
	if (result != 0) {
		const std::string message = vips_error_buffer();
		vips_error_clear();
		fail(ImageErrorCode::decodeFailure, "initialise", message);
	}
}

[[nodiscard]] bool
looksLikeUrl(const std::string& value)
{
	return value.find("://") != std::string::npos
	    || value.starts_with("data:")
	    || value.starts_with("file:");
}

[[nodiscard]] OpenInput
openRegularFile(const std::filesystem::path& path, const std::uint64_t maximumBytes)
{
	const int descriptor = ::open(
	    path.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
	if (descriptor < 0) {
		fail(ImageErrorCode::invalidPath, "open input",
		    std::system_error(errno, std::generic_category()).what());
	}
	FileDescriptor owned(descriptor);
	struct stat status{};
	if (::fstat(descriptor, &status) != 0) {
		fail(ImageErrorCode::invalidPath, "inspect input",
		    std::system_error(errno, std::generic_category()).what());
	}
	if (!S_ISREG(status.st_mode)) {
		fail(ImageErrorCode::invalidPath, "inspect input",
		    "input must resolve to a regular file");
	}
	if (status.st_size < 0) {
		fail(ImageErrorCode::invalidMetadata, "inspect input",
		    "input reports a negative file size");
	}
	if (static_cast<std::uint64_t>(status.st_size) > maximumBytes) {
		fail(ImageErrorCode::fileTooLarge, "inspect input",
		    "input exceeds the configured file-size limit");
	}
	return {std::move(owned), {status.st_dev, status.st_ino, status.st_size}};
}

[[nodiscard]] std::filesystem::path
resolveInput(const std::filesystem::path& path)
{
	if (path.empty() || looksLikeUrl(path.string())) {
		fail(ImageErrorCode::invalidPath, "resolve input",
		    "input must be a local filesystem path");
	}
	std::error_code error;
	const std::filesystem::path canonical = std::filesystem::canonical(path, error);
	if (error) {
		fail(ImageErrorCode::invalidPath, "resolve input", error.message());
	}
	return canonical;
}

void
validateLimits(const ImageLoadLimits& limits)
{
	if (limits.maximumFileBytes == 0U || limits.maximumWidth == 0U
	    || limits.maximumHeight == 0U || limits.maximumDecodedPixels == 0U
	    || limits.maximumPages == 0U || limits.maximumOutputWidth == 0U
	    || limits.maximumOutputHeight == 0U) {
		fail(ImageErrorCode::invalidRecipe, "validate limits",
		    "all image limits must be nonzero");
	}
}

void
validateRecipe(const ImageRecipe& recipe, const ImageLoadLimits& limits)
{
	if (recipe.targetWidth == 0U || recipe.targetHeight == 0U) {
		fail(ImageErrorCode::invalidRecipe, "validate recipe",
		    "target dimensions must be nonzero");
	}
	if (recipe.targetWidth > limits.maximumOutputWidth
	    || recipe.targetHeight > limits.maximumOutputHeight) {
		fail(ImageErrorCode::dimensionLimit, "validate recipe",
		    "target dimensions exceed the configured output limit");
	}
	if (recipe.crop.has_value()
	    && (recipe.crop->width == 0U || recipe.crop->height == 0U)) {
		fail(ImageErrorCode::invalidCrop, "validate crop",
		    "crop width and height must be nonzero");
	}
}

[[nodiscard]] RasterFormat
detectFormat(const VSource& source)
{
	const char* const loader = vips_foreign_find_load_source(source.get_source());
	if (loader == nullptr) {
		fail(ImageErrorCode::unsupportedLoader, "detect loader",
		    "no libvips loader accepts the input");
	}
	const std::string name(loader);
	if (name == "jpegload_source" || name == "VipsForeignLoadJpegSource") {
		return RasterFormat::jpeg;
	}
	if (name == "pngload_source" || name == "VipsForeignLoadPngSource") {
		return RasterFormat::png;
	}
	fail(ImageErrorCode::unsupportedLoader, "detect loader",
	    "the detected libvips loader is not in the JPEG/PNG allowlist: " + name);
}

[[nodiscard]] VImage
loadFromSource(const VSource& source, const RasterFormat format, const int shrink)
{
	VOption* options = VImage::option()
	    ->set("access", VIPS_ACCESS_SEQUENTIAL)
	    ->set("fail_on", VIPS_FAIL_ON_ERROR);
	if (format == RasterFormat::jpeg) {
		options->set("shrink", shrink);
		return VImage::jpegload_source(source, options);
	}
	return VImage::pngload_source(source, options);
}

[[nodiscard]] std::uint32_t
readBigEndian32(const std::array<std::uint8_t, 8>& bytes) noexcept
{
	return static_cast<std::uint32_t>(bytes[0]) << 24U
	    | static_cast<std::uint32_t>(bytes[1]) << 16U
	    | static_cast<std::uint32_t>(bytes[2]) << 8U
	    | static_cast<std::uint32_t>(bytes[3]);
}

void
rejectAnimatedPng(const OpenInput& input)
{
	constexpr std::array<std::uint8_t, 8> signature{{
		0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
	}};
	std::array<std::uint8_t, 8> bytes{};
	if (::pread(input.descriptor.get(), bytes.data(), bytes.size(), 0)
	    != static_cast<ssize_t>(bytes.size()) || bytes != signature) {
		fail(ImageErrorCode::invalidMetadata, "inspect PNG chunks",
		    "PNG signature is truncated or invalid");
	}
	std::uint64_t offset = signature.size();
	const std::uint64_t fileSize = static_cast<std::uint64_t>(input.identity.size);
	while (offset <= fileSize && fileSize - offset >= 12U) {
		if (::pread(input.descriptor.get(), bytes.data(), bytes.size(),
		        static_cast<off_t>(offset)) != static_cast<ssize_t>(bytes.size())) {
			fail(ImageErrorCode::invalidMetadata, "inspect PNG chunks",
			    "PNG chunk header is truncated");
		}
		const std::uint64_t length = readBigEndian32(bytes);
		if (length > fileSize - offset - 12U) {
			fail(ImageErrorCode::invalidMetadata, "inspect PNG chunks",
			    "PNG chunk length exceeds the input file");
		}
		const std::array<std::uint8_t, 4> type{{bytes[4], bytes[5], bytes[6], bytes[7]}};
		if (type == std::array<std::uint8_t, 4>{{'a', 'c', 'T', 'L'}}) {
			fail(ImageErrorCode::pageLimit, "inspect PNG chunks",
			    "animated PNG input is not supported");
		}
		if (type == std::array<std::uint8_t, 4>{{'I', 'D', 'A', 'T'}}
		    || type == std::array<std::uint8_t, 4>{{'I', 'E', 'N', 'D'}}) {
			return;
		}
		offset += length + 12U;
	}
	fail(ImageErrorCode::invalidMetadata, "inspect PNG chunks",
	    "PNG does not contain image data");
}

[[nodiscard]] std::uint64_t
checkedPixels(const std::uint32_t width, const std::uint32_t height)
{
	return static_cast<std::uint64_t>(width) * static_cast<std::uint64_t>(height);
}

[[nodiscard]] ExifOrientation
readOrientation(const VImage& image)
{
	int value = 1;
	if (image.get_typeof(VIPS_META_ORIENTATION) != 0U) {
		value = image.get_int(VIPS_META_ORIENTATION);
	}
	if (image.get_typeof("exif-ifd0-Orientation") != 0U) {
		const std::string raw = image.get_string("exif-ifd0-Orientation");
		int rawValue = 0;
		const auto [end, error] = std::from_chars(
		    raw.data(), raw.data() + raw.size(), rawValue);
		if (error != std::errc{} || end == raw.data()) {
			fail(ImageErrorCode::invalidMetadata, "read orientation",
			    "EXIF orientation metadata is malformed");
		}
		if (rawValue < 1 || rawValue > 8) {
			fail(ImageErrorCode::invalidOrientation, "read orientation",
			    "EXIF orientation must be between 1 and 8");
		}
		value = rawValue;
	}
	if (value < 1 || value > 8) {
		fail(ImageErrorCode::invalidOrientation, "read orientation",
		    "EXIF orientation must be between 1 and 8");
	}
	return static_cast<ExifOrientation>(value);
}

[[nodiscard]] bool
swapsAxes(const ExifOrientation orientation) noexcept
{
	return orientation == ExifOrientation::transpose
	    || orientation == ExifOrientation::rotate90Clockwise
	    || orientation == ExifOrientation::transverse
	    || orientation == ExifOrientation::rotate270Clockwise;
}

[[nodiscard]] std::uint8_t
bitsPerSample(const VipsBandFormat format)
{
	if (format == VIPS_FORMAT_UCHAR) {
		return 8;
	}
	if (format == VIPS_FORMAT_USHORT) {
		return 16;
	}
	fail(ImageErrorCode::invalidMetadata, "read header",
	    "only 8-bit and 16-bit integer raster samples are supported");
}

void
validatePages(const VImage& image, const ImageLoadLimits& limits)
{
	int pages = 1;
	if (image.get_typeof("n-pages") != 0U) {
		pages = image.get_int("n-pages");
	}
	if (pages < 1) {
		fail(ImageErrorCode::invalidMetadata, "read header",
		    "page count must be positive");
	}
	if (static_cast<std::uint32_t>(pages) > limits.maximumPages) {
		fail(ImageErrorCode::pageLimit, "read header",
		    "animated or multipage input is not supported");
	}
	if (image.get_typeof("page-height") != 0U) {
		const int pageHeight = image.get_int("page-height");
		if (pageHeight <= 0
		    || static_cast<std::int64_t>(image.height())
		        != static_cast<std::int64_t>(pageHeight) * pages) {
			fail(ImageErrorCode::invalidMetadata, "read header",
			    "page metadata contradicts the image height");
		}
	}
}

[[nodiscard]] HeaderInfo
readHeader(const std::filesystem::path& path, const ImageLoadLimits& limits)
{
	OpenInput input = openRegularFile(path, limits.maximumFileBytes);
	FileDescriptor sourceDescriptor(::dup(input.descriptor.get()));
	if (sourceDescriptor.get() < 0) {
		fail(ImageErrorCode::invalidPath, "duplicate input",
		    std::system_error(errno, std::generic_category()).what());
	}
	VSource source = VSource::new_from_descriptor(sourceDescriptor.get());
	const RasterFormat format = detectFormat(source);
	if (format == RasterFormat::png) {
		rejectAnimatedPng(input);
	}
	VImage image = loadFromSource(source, format, 1);
	const int widthValue = image.width();
	const int heightValue = image.height();
	if (widthValue <= 0 || heightValue <= 0) {
		fail(ImageErrorCode::invalidMetadata, "read header",
		    "source dimensions must be positive");
	}
	const std::uint32_t width = static_cast<std::uint32_t>(widthValue);
	const std::uint32_t height = static_cast<std::uint32_t>(heightValue);
	if (width > limits.maximumWidth || height > limits.maximumHeight) {
		fail(ImageErrorCode::dimensionLimit, "read header",
		    "source dimensions exceed configured limits");
	}
	if (checkedPixels(width, height) > limits.maximumDecodedPixels) {
		fail(ImageErrorCode::pixelLimit, "read header",
		    "source pixel count exceeds the configured limit");
	}
	validatePages(image, limits);
	const ExifOrientation orientation = readOrientation(image);
	const bool swap = swapsAxes(orientation);
	return {
		path,
		input.identity,
		format,
		width,
		height,
		swap ? height : width,
		swap ? width : height,
		orientation,
		bitsPerSample(image.format()),
		image.has_alpha(),
		image.get_typeof(VIPS_META_ICC_NAME) != 0U,
	};
}

void
validateIdentity(const FileIdentity& expected, const FileIdentity& actual)
{
	if (expected.device != actual.device || expected.inode != actual.inode
	    || expected.size != actual.size) {
		fail(ImageErrorCode::invalidPath, "reopen input",
		    "input changed after header validation");
	}
}

void
validateCrop(const CropRect& crop, const std::uint32_t width,
	const std::uint32_t height)
{
	if (crop.width == 0U || crop.height == 0U || crop.x > width || crop.y > height
	    || crop.width > static_cast<std::uint64_t>(width) - crop.x
	    || crop.height > static_cast<std::uint64_t>(height) - crop.y) {
		fail(ImageErrorCode::invalidCrop, "validate crop",
		    "crop is outside the oriented source image");
	}
}

[[nodiscard]] std::uint64_t
roundRatio(const std::uint64_t value, const std::uint64_t multiplier,
	const std::uint64_t divisor)
{
	if (value != 0U && multiplier > std::numeric_limits<std::uint64_t>::max() / value) {
		fail(ImageErrorCode::invalidRecipe, "calculate geometry",
		    "resize arithmetic overflow");
	}
	const std::uint64_t product = value * multiplier;
	if (product > std::numeric_limits<std::uint64_t>::max() - divisor / 2U) {
		fail(ImageErrorCode::invalidRecipe, "calculate geometry",
		    "resize arithmetic overflow");
	}
	return (product + divisor / 2U) / divisor;
}

[[nodiscard]] std::uint64_t
ceilRatio(const std::uint64_t value, const std::uint64_t multiplier,
	const std::uint64_t divisor)
{
	if (value != 0U && multiplier > std::numeric_limits<std::uint64_t>::max() / value) {
		fail(ImageErrorCode::invalidRecipe, "calculate geometry",
		    "resize arithmetic overflow");
	}
	const std::uint64_t product = value * multiplier;
	return product / divisor + (product % divisor == 0U ? 0U : 1U);
}

[[nodiscard]] ResizeGeometry
geometry(const std::uint32_t sourceWidth, const std::uint32_t sourceHeight,
	const ImageRecipe& recipe)
{
	const std::uint64_t targetWidth = recipe.targetWidth;
	const std::uint64_t targetHeight = recipe.targetHeight;
	std::uint64_t width = targetWidth;
	std::uint64_t height = targetHeight;
	const std::uint64_t sourceAspectLeft
	    = static_cast<std::uint64_t>(sourceWidth) * targetHeight;
	const std::uint64_t sourceAspectRight
	    = static_cast<std::uint64_t>(sourceHeight) * targetWidth;
	if (recipe.fit == FitMode::contain) {
		if (sourceAspectLeft >= sourceAspectRight) {
			height = roundRatio(sourceHeight, targetWidth, sourceWidth);
		} else {
			width = roundRatio(sourceWidth, targetHeight, sourceHeight);
		}
		width = std::max<std::uint64_t>(1U, width);
		height = std::max<std::uint64_t>(1U, height);
	} else if (sourceAspectLeft >= sourceAspectRight) {
		width = ceilRatio(sourceWidth, targetHeight, sourceHeight);
	} else {
		height = ceilRatio(sourceHeight, targetWidth, sourceWidth);
	}
	return {
		static_cast<std::uint32_t>(width),
		static_cast<std::uint32_t>(height),
		static_cast<std::uint32_t>((width - targetWidth) / 2U),
		static_cast<std::uint32_t>((height - targetHeight) / 2U),
	};
}

[[nodiscard]] int
jpegShrink(const HeaderInfo& header, const ImageRecipe& recipe)
{
	if (header.format != RasterFormat::jpeg || recipe.crop.has_value()) {
		return 1;
	}
	const ResizeGeometry desired = geometry(header.orientedWidth,
	    header.orientedHeight, recipe);
	for (const int factor : {8, 4, 2}) {
		const std::uint32_t width = (header.orientedWidth
		    + static_cast<std::uint32_t>(factor) - 1U)
		    / static_cast<std::uint32_t>(factor);
		const std::uint32_t height = (header.orientedHeight
		    + static_cast<std::uint32_t>(factor) - 1U)
		    / static_cast<std::uint32_t>(factor);
		if (width >= desired.width && height >= desired.height) {
			return factor;
		}
	}
	return 1;
}

[[nodiscard]] VImage
toEightBit(const VImage& image)
{
	if (image.format() == VIPS_FORMAT_UCHAR) {
		return image;
	}
	if (image.format() == VIPS_FORMAT_USHORT) {
		return image.cast(VIPS_FORMAT_UCHAR, VImage::option()->set("shift", true));
	}
	fail(ImageErrorCode::invalidMetadata, "convert samples",
	    "unsupported raster sample format");
}

[[nodiscard]] VImage
normaliseColour(VImage image, const bool hasProfile)
{
	const bool hasAlpha = image.has_alpha();
	VImage alpha;
	VImage colour = image;
	if (hasAlpha) {
		alpha = image.extract_band(image.bands() - 1);
		colour = image.extract_band(0,
		    VImage::option()->set("n", image.bands() - 1));
	}
	if (hasProfile) {
		size_t profileSize = 0;
		const void* const profile = colour.get_blob(VIPS_META_ICC_NAME, &profileSize);
		if (profile == nullptr || profileSize == 0U
		    || !vips_icc_is_compatible_profile(
		        colour.get_image(), profile, profileSize)) {
			fail(ImageErrorCode::invalidProfile, "validate colour profile",
			    "embedded ICC profile is invalid or incompatible");
		}
		try {
			colour = colour.icc_transform("srgb", VImage::option()
			    ->set("embedded", true)
			    ->set("intent", VIPS_INTENT_RELATIVE)
			    ->set("black_point_compensation", true)
			    ->set("depth", 8));
		} catch (const vips::VError& error) {
			vips_error_clear();
			fail(ImageErrorCode::invalidProfile, "convert colour profile", error.what());
		}
	} else {
		if (colour.interpretation() == VIPS_INTERPRETATION_CMYK
		    || colour.bands() == 4) {
			fail(ImageErrorCode::unsupportedColourspace, "convert colour",
			    "unprofiled CMYK input is not supported");
		}
		colour = toEightBit(colour);
		if (colour.bands() == 1) {
			colour = VImage::bandjoin({colour, colour, colour});
		} else if (colour.bands() != 3) {
			fail(ImageErrorCode::unsupportedColourspace, "convert colour",
			    "unprofiled input must be grayscale, RGB, grayscale-alpha, or RGBA");
		}
	}
	colour = toEightBit(colour);
	if (colour.bands() != 3) {
		fail(ImageErrorCode::unsupportedColourspace, "convert colour",
		    "colour conversion did not produce three channels");
	}
	colour = colour.copy(VImage::option()->set(
	    "interpretation", VIPS_INTERPRETATION_sRGB));
	if (hasAlpha) {
		alpha = toEightBit(alpha);
		return colour.bandjoin(alpha);
	}
	return colour.bandjoin(255.0);
}

[[nodiscard]] VImage
applyFit(VImage image, const ImageRecipe& recipe)
{
	const ResizeGeometry resize = geometry(
	    static_cast<std::uint32_t>(image.width()),
	    static_cast<std::uint32_t>(image.height()), recipe);
	if (resize.width != static_cast<std::uint32_t>(image.width())
	    || resize.height != static_cast<std::uint32_t>(image.height())) {
		const double horizontal = static_cast<double>(resize.width) / image.width();
		const double vertical = static_cast<double>(resize.height) / image.height();
		image = image.resize(horizontal, VImage::option()
		    ->set("vscale", vertical)
		    ->set("kernel", VIPS_KERNEL_LANCZOS3));
	}
	if (recipe.fit == FitMode::contain) {
		const int left = (static_cast<int>(recipe.targetWidth) - image.width()) / 2;
		const int top = (static_cast<int>(recipe.targetHeight) - image.height()) / 2;
		return image.embed(left, top, recipe.targetWidth, recipe.targetHeight,
		    VImage::option()
		        ->set("extend", VIPS_EXTEND_BACKGROUND)
		        ->set("background", std::vector<double>{0.0, 0.0, 0.0, 0.0}));
	}
	return image.extract_area(static_cast<int>(resize.left),
	    static_cast<int>(resize.top), recipe.targetWidth, recipe.targetHeight);
}

[[nodiscard]] core::Rgb8Frame
materialise(VImage image, const ImageRecipe& recipe)
{
	image = image.unpremultiply(VImage::option()->set("max_alpha", 255.0));
	image = image.flatten(VImage::option()->set("background", std::vector<double>{
	    static_cast<double>(recipe.background.red),
	    static_cast<double>(recipe.background.green),
	    static_cast<double>(recipe.background.blue),
	}));
	image = image.cast(VIPS_FORMAT_UCHAR).copy(
	    VImage::option()->set("interpretation", VIPS_INTERPRETATION_sRGB));
	if (image.width() != recipe.targetWidth
	    || image.height() != recipe.targetHeight || image.bands() != 3
	    || image.format() != VIPS_FORMAT_UCHAR) {
		fail(ImageErrorCode::decodeFailure, "materialise image",
		    "image graph did not produce tightly packed target RGB8");
	}
	size_t size = 0;
	void* const memory = image.write_to_memory(&size);
	if (memory == nullptr) {
		fail(ImageErrorCode::decodeFailure, "materialise image",
		    "libvips did not return final image memory");
	}
	const std::size_t expected = static_cast<std::size_t>(recipe.targetWidth)
	    * recipe.targetHeight * 3U;
	if (size != expected) {
		g_free(memory);
		fail(ImageErrorCode::decodeFailure, "materialise image",
		    "final RGB8 memory size is inconsistent");
	}
	const auto* bytes = static_cast<const std::uint8_t*>(memory);
	std::vector<core::Rgb8Pixel> pixels;
	pixels.reserve(static_cast<std::size_t>(recipe.targetWidth) * recipe.targetHeight);
	for (std::size_t offset = 0; offset < expected; offset += 3U) {
		pixels.push_back({bytes[offset], bytes[offset + 1U], bytes[offset + 2U]});
	}
	g_free(memory);
	return core::Rgb8Frame(recipe.targetWidth, recipe.targetHeight, std::move(pixels));
}

[[nodiscard]] PreparedImage
prepareImpl(const std::filesystem::path& path, const ImageRecipe& recipe,
	const ImageLoadLimits& limits)
{
	ensureVips();
	validateLimits(limits);
	validateRecipe(recipe, limits);
	const std::filesystem::path canonical = resolveInput(path);
	const HeaderInfo header = readHeader(canonical, limits);
	if (recipe.crop.has_value()) {
		validateCrop(*recipe.crop,
		    recipe.applyExifOrientation ? header.orientedWidth : header.width,
		    recipe.applyExifOrientation ? header.orientedHeight : header.height);
	}
	OpenInput input = openRegularFile(canonical, limits.maximumFileBytes);
	validateIdentity(header.identity, input.identity);
	FileDescriptor sourceDescriptor(::dup(input.descriptor.get()));
	if (sourceDescriptor.get() < 0) {
		fail(ImageErrorCode::invalidPath, "duplicate input",
		    std::system_error(errno, std::generic_category()).what());
	}
	VSource source = VSource::new_from_descriptor(sourceDescriptor.get());
	VImage image = loadFromSource(source, header.format, jpegShrink(header, recipe));
	if (recipe.applyExifOrientation) {
		image = image.autorot();
	}
	if (recipe.crop.has_value()) {
		image = image.extract_area(static_cast<int>(recipe.crop->x),
		    static_cast<int>(recipe.crop->y), static_cast<int>(recipe.crop->width),
		    static_cast<int>(recipe.crop->height));
	}
	image = normaliseColour(image, header.hasEmbeddedProfile);
	image = image.premultiply(VImage::option()->set("max_alpha", 255.0));
	image = applyFit(image, recipe);
	core::Rgb8Frame frame = materialise(image, recipe);
	const SourceImageInfo sourceInfo{
		header.format,
		header.width,
		header.height,
		recipe.applyExifOrientation ? header.orientedWidth : header.width,
		recipe.applyExifOrientation ? header.orientedHeight : header.height,
		header.orientation,
		header.bitsPerSample,
		header.hasAlpha,
		header.hasEmbeddedProfile,
	};
	return {std::move(frame), sourceInfo, recipe.crop, recipe.fit};
}

class TemporaryPng {
public:
	TemporaryPng(const std::filesystem::path& destinationValue, const bool forceValue)
		: destination(destinationValue), descriptor(-1), force(forceValue)
	{
		std::error_code error;
		if (!force && std::filesystem::exists(destination, error)) {
			fail(ImageErrorCode::destinationExists, "inspect PNG destination",
			    "output already exists; use --force to replace it");
		}
		if (error) {
			fail(ImageErrorCode::outputFailure, "inspect PNG destination", error.message());
		}
		for (unsigned int attempt = 0; attempt < 100U; ++attempt) {
			path = destination.string() + ".tmp."
			    + std::to_string(static_cast<long long>(::getpid())) + "."
			    + std::to_string(attempt);
			descriptor = ::open(path.c_str(), O_RDWR | O_CREAT | O_EXCL,
			    S_IRUSR | S_IWUSR);
			if (descriptor >= 0) {
				return;
			}
			if (errno != EEXIST) {
				fail(ImageErrorCode::outputFailure, "create temporary PNG",
				    std::system_error(errno, std::generic_category()).what());
			}
		}
		fail(ImageErrorCode::outputFailure, "create temporary PNG",
		    "could not create a unique temporary file");
	}

	TemporaryPng(const TemporaryPng&) = delete;
	TemporaryPng& operator=(const TemporaryPng&) = delete;

	~TemporaryPng()
	{
		if (descriptor >= 0) {
			::close(descriptor);
		}
		if (!path.empty()) {
			::unlink(path.c_str());
		}
	}

	[[nodiscard]] int fileDescriptor() const noexcept
	{
		return descriptor;
	}

	void publish()
	{
		if (::fsync(descriptor) != 0) {
			fail(ImageErrorCode::outputFailure, "sync temporary PNG",
			    std::system_error(errno, std::generic_category()).what());
		}
		if (::close(descriptor) != 0) {
			descriptor = -1;
			fail(ImageErrorCode::outputFailure, "close temporary PNG",
			    std::system_error(errno, std::generic_category()).what());
		}
		descriptor = -1;
		const int result = force ? ::rename(path.c_str(), destination.c_str())
		    : ::link(path.c_str(), destination.c_str());
		if (result != 0) {
			fail(ImageErrorCode::outputFailure, "publish PNG",
			    std::system_error(errno, std::generic_category()).what());
		}
		if (!force && ::unlink(path.c_str()) != 0) {
			const int error = errno;
			::unlink(destination.c_str());
			fail(ImageErrorCode::outputFailure, "remove temporary PNG link",
			    std::system_error(error, std::generic_category()).what());
		}
		path.clear();
	}

private:
	std::filesystem::path destination;
	int descriptor;
	bool force;
	std::filesystem::path path;
};

[[nodiscard]] PngMetadata
writePngImpl(const std::filesystem::path& destination,
	const core::Rgb8View frame, const bool force)
{
	ensureVips();
	std::vector<std::uint8_t> bytes;
	bytes.reserve(frame.pixels().size() * 3U);
	for (const core::Rgb8Pixel& pixel : frame.pixels()) {
		bytes.push_back(pixel.red);
		bytes.push_back(pixel.green);
		bytes.push_back(pixel.blue);
	}
	VImage image = VImage::new_from_memory_copy(bytes.data(), bytes.size(),
	    frame.width(), frame.height(), 3, VIPS_FORMAT_UCHAR);
	image = image.copy(VImage::option()->set(
	    "interpretation", VIPS_INTERPRETATION_sRGB));
	TemporaryPng temporary(destination, force);
	const int targetDescriptor = ::dup(temporary.fileDescriptor());
	if (targetDescriptor < 0) {
		fail(ImageErrorCode::outputFailure, "duplicate PNG descriptor",
		    std::system_error(errno, std::generic_category()).what());
	}
	FileDescriptor ownedTarget(targetDescriptor);
	{
		VTarget target = VTarget::new_to_descriptor(ownedTarget.get());
		image.pngsave_target(target, VImage::option()
		    ->set("strip", true)
		    ->set("compression", 9)
		    ->set("interlace", false)
		    ->set("filter", VIPS_FOREIGN_PNG_FILTER_ALL));
	}
	ownedTarget.close();
	temporary.publish();
	return {frame.width(), frame.height()};
}

} // namespace

ImageLoadLimits
defaultImageLoadLimits() noexcept
{
	return {
		64U * 1'024U * 1'024U,
		16'384,
		16'384,
		64'000'000,
		1,
		4'096,
		4'096,
	};
}

ImageResult<PreparedImage>
prepareRasterImage(const std::filesystem::path& path, const ImageRecipe& recipe,
	const ImageLoadLimits& limits)
{
	try {
		return prepareImpl(path, recipe, limits);
	} catch (const ImageFailure& error) {
		return error.error();
	} catch (const vips::VError& error) {
		const std::string message = error.what();
		vips_error_clear();
		return ImageError{ImageErrorCode::decodeFailure, "decode image", message};
	} catch (const std::exception& error) {
		return ImageError{ImageErrorCode::decodeFailure, "prepare image", error.what()};
	}
}

ImageResult<PngMetadata>
writeRgb8PngAtomic(const std::filesystem::path& destination,
	const core::Rgb8View frame, const bool force)
{
	try {
		return writePngImpl(destination, frame, force);
	} catch (const ImageFailure& error) {
		return error.error();
	} catch (const vips::VError& error) {
		const std::string message = error.what();
		vips_error_clear();
		return ImageError{ImageErrorCode::outputFailure, "write PNG", message};
	} catch (const std::exception& error) {
		return ImageError{ImageErrorCode::outputFailure, "write PNG", error.what()};
	}
}

} // namespace sstv::image
