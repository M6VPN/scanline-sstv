// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/image_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/martin_m1.hpp>
#include <sstv/analog/pd_120.hpp>
#include <sstv/analog/robot_36.hpp>
#include <sstv/analog/scottie_s1.hpp>
#include <sstv/core/rgb8_frame.hpp>
#include <sstv/image/image.hpp>

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {

const std::filesystem::path fixtures{SSTV_IMAGE_FIXTURE_DIR};

class TemporaryDirectory {
public:
	TemporaryDirectory()
	{
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		value = std::filesystem::temp_directory_path()
		    / ("scanline-sstv-image-" + std::to_string(::getpid()) + "-"
		        + std::to_string(stamp));
		std::filesystem::create_directories(value);
	}
	~TemporaryDirectory()
	{
		std::error_code error;
		std::filesystem::remove_all(value, error);
	}
	[[nodiscard]] const std::filesystem::path& path() const noexcept
	{
		return value;
	}
private:
	std::filesystem::path value;
};

void
require(const bool condition, const std::string& message)
{
	if (!condition) {
		throw std::runtime_error(message);
	}
}

void
requirePixel(const sstv::core::Rgb8Pixel actual,
	const sstv::core::Rgb8Pixel expected, const std::string& message,
	const int tolerance = 0)
{
	const auto near = [tolerance](const std::uint8_t left, const std::uint8_t right) {
		return std::abs(static_cast<int>(left) - static_cast<int>(right)) <= tolerance;
	};
	require(near(actual.red, expected.red) && near(actual.green, expected.green)
	    && near(actual.blue, expected.blue), message);
}

[[nodiscard]] sstv::image::ImageRecipe
makeRecipe(const std::uint16_t width, const std::uint16_t height,
	const sstv::image::FitMode fit = sstv::image::FitMode::contain,
	const sstv::core::Rgb8Pixel background = {0, 0, 0},
	const std::optional<sstv::image::CropRect> crop = std::nullopt,
	const bool orient = true)
{
	return {width, height, fit, crop, background, orient};
}

[[nodiscard]] sstv::image::PreparedImage
prepare(const std::filesystem::path& path, const sstv::image::ImageRecipe& recipe,
	const sstv::image::ImageLoadLimits limits = sstv::image::defaultImageLoadLimits())
{
	auto result = sstv::image::prepareRasterImage(path, recipe, limits);
	if (const auto* error = std::get_if<sstv::image::ImageError>(&result)) {
		throw std::runtime_error(error->operation + ": " + error->message);
	}
	return std::get<sstv::image::PreparedImage>(std::move(result));
}

void
requireError(const std::filesystem::path& path,
	const sstv::image::ImageRecipe& recipe,
	const sstv::image::ImageErrorCode expected,
	const sstv::image::ImageLoadLimits limits = sstv::image::defaultImageLoadLimits())
{
	auto result = sstv::image::prepareRasterImage(path, recipe, limits);
	const auto* error = std::get_if<sstv::image::ImageError>(&result);
	require(error != nullptr, "invalid image was accepted: " + path.string());
	require(error->code == expected, "wrong image error for " + path.string()
	    + ": " + error->message);
}

[[nodiscard]] sstv::core::Rgb8Frame
solidFrame(const std::uint16_t width, const std::uint16_t height,
	const sstv::core::Rgb8Pixel pixel)
{
	return sstv::core::Rgb8Frame(width, height,
	    std::vector<sstv::core::Rgb8Pixel>(
	        static_cast<std::size_t>(width) * height, pixel));
}

void
writeFrame(const std::filesystem::path& path, const sstv::core::Rgb8View view,
	const bool force = false)
{
	auto result = sstv::image::writeRgb8PngAtomic(path, view, force);
	if (const auto* error = std::get_if<sstv::image::ImageError>(&result)) {
		throw std::runtime_error(error->operation + ": " + error->message);
	}
}

[[nodiscard]] bool
equalFrames(const sstv::core::Rgb8View left, const sstv::core::Rgb8View right)
{
	return left.width() == right.width() && left.height() == right.height()
	    && std::equal(left.pixels().begin(), left.pixels().end(), right.pixels().begin(),
	        [](const auto& a, const auto& b) {
			return a.red == b.red && a.green == b.green && a.blue == b.blue;
		});
}

void
testExactIntegration(const std::filesystem::path& temporary)
{
	const sstv::core::Rgb8Frame direct = sstv::core::makeDiagnosticPattern(320, 256);
	const std::filesystem::path path = temporary / "diagnostic.png";
	writeFrame(path, direct.view());
	const auto prepared = prepare(path, makeRecipe(320, 256));
	require(equalFrames(direct.view(), prepared.frame.view()),
	    "exact-size PNG round trip changed pixels");
	const auto directEvents = sstv::analog::encodeMartinM1(direct.view(), 0.8F);
	const auto loadedEvents = sstv::analog::encodeMartinM1(prepared.frame.view(), 0.8F);
	require(directEvents.size() == loadedEvents.size(), "event count changed");
	for (std::size_t index = 0; index < directEvents.size(); ++index) {
		require(directEvents[index].duration() == loadedEvents[index].duration()
		    && directEvents[index].frequencyHz() == loadedEvents[index].frequencyHz()
		    && directEvents[index].amplitude() == loadedEvents[index].amplitude(),
		    "image path changed the frozen Martin M1 event stream");
	}
	const auto directScottie = sstv::analog::encodeScottieS1(direct.view(), 0.8F);
	const auto loadedScottie
	    = sstv::analog::encodeScottieS1(prepared.frame.view(), 0.8F);
	require(directScottie.size() == loadedScottie.size(),
	    "Scottie S1 event count changed through exact-size PNG preparation");
	for (std::size_t index = 0; index < directScottie.size(); ++index) {
		require(directScottie[index].duration() == loadedScottie[index].duration()
		    && directScottie[index].frequencyHz()
		        == loadedScottie[index].frequencyHz()
		    && directScottie[index].amplitude() == loadedScottie[index].amplitude(),
		    "image path changed the Scottie S1 event stream");
	}
	const sstv::core::Rgb8Frame directRobot
	    = sstv::core::makeDiagnosticPattern(320, 240);
	const std::filesystem::path robotPath = temporary / "robot-diagnostic.png";
	writeFrame(robotPath, directRobot.view());
	const auto preparedRobot = prepare(robotPath, makeRecipe(320, 240));
	require(equalFrames(directRobot.view(), preparedRobot.frame.view()),
	    "exact-size Robot 36 PNG round trip changed pixels");
	const auto directRobotEvents
	    = sstv::analog::encodeRobot36(directRobot.view(), 0.8F);
	const auto loadedRobotEvents
	    = sstv::analog::encodeRobot36(preparedRobot.frame.view(), 0.8F);
	require(directRobotEvents.size() == loadedRobotEvents.size(),
	    "Robot 36 event count changed through exact-size PNG preparation");
	for (std::size_t index = 0; index < directRobotEvents.size(); ++index) {
		require(directRobotEvents[index].duration()
		        == loadedRobotEvents[index].duration()
		    && directRobotEvents[index].frequencyHz()
		        == loadedRobotEvents[index].frequencyHz()
		    && directRobotEvents[index].amplitude()
		        == loadedRobotEvents[index].amplitude(),
		    "image path changed the Robot 36 event stream");
	}
	const sstv::core::Rgb8Frame directPd120
	    = sstv::core::makeDiagnosticPattern(640, 496);
	const std::filesystem::path pd120Path = temporary / "pd-120-diagnostic.png";
	writeFrame(pd120Path, directPd120.view());
	const auto preparedPd120 = prepare(pd120Path, makeRecipe(640, 496));
	require(equalFrames(directPd120.view(), preparedPd120.frame.view()),
	    "exact-size PD120 PNG round trip changed pixels");
	const auto directPd120Events
	    = sstv::analog::encodePd120(directPd120.view(), 0.8F);
	const auto loadedPd120Events
	    = sstv::analog::encodePd120(preparedPd120.frame.view(), 0.8F);
	require(directPd120Events.size() == loadedPd120Events.size(),
	    "PD120 event count changed through exact-size PNG preparation");
	for (std::size_t index = 0; index < directPd120Events.size(); ++index) {
		require(directPd120Events[index].duration()
		        == loadedPd120Events[index].duration()
		    && directPd120Events[index].frequencyHz()
		        == loadedPd120Events[index].frequencyHz()
		    && directPd120Events[index].amplitude()
		        == loadedPd120Events[index].amplitude(),
		    "image path changed the PD120 event stream");
	}
}

void
testDimensionsAndFit(const std::filesystem::path& temporary)
{
	const std::array<std::pair<std::string, sstv::core::Rgb8Frame>, 3> sources{{
		{"portrait.png", solidFrame(2, 5, {10, 20, 30})},
		{"square.png", solidFrame(4, 4, {40, 50, 60})},
		{"tiny.png", solidFrame(1, 1, {70, 80, 90})},
	}};
	for (const auto& [name, frame] : sources) {
		const auto path = temporary / name;
		writeFrame(path, frame.view());
		const auto output = prepare(path, makeRecipe(320, 256));
		require(output.frame.view().width() == 320 && output.frame.view().height() == 256,
		    "arbitrary source did not produce mode dimensions");
	}
	const sstv::core::Rgb8Pixel background{1, 2, 3};
	const auto contained = prepare(fixtures / "marker.png",
	    makeRecipe(10, 10, sstv::image::FitMode::contain, background));
	requirePixel(contained.frame.view().pixel(0, 0), background,
	    "contain letterbox background is wrong");
	requirePixel(contained.frame.view().pixel(0, 2), {255, 0, 0},
	    "contain placement is wrong", 2);
	const auto covered = prepare(fixtures / "marker.png",
	    makeRecipe(3, 3, sstv::image::FitMode::cover));
	requirePixel(covered.frame.view().pixel(0, 0), {32, 32, 32},
	    "cover crop origin is wrong");
	requirePixel(covered.frame.view().pixel(2, 2), {224, 224, 224},
	    "cover crop boundary is wrong");
}

void
testCropsAndOrientation()
{
	const auto cropped = prepare(fixtures / "marker.png", makeRecipe(3, 3,
	    sstv::image::FitMode::contain, {0, 0, 0}, {sstv::image::CropRect{1, 0, 3, 3}}));
	requirePixel(cropped.frame.view().pixel(0, 0), {32, 32, 32}, "crop origin is wrong");
	requireError(fixtures / "marker.png", makeRecipe(3, 3,
	    sstv::image::FitMode::contain, {0, 0, 0}, {sstv::image::CropRect{4, 0, 2, 1}}),
	    sstv::image::ImageErrorCode::invalidCrop);
	requireError(fixtures / "marker.png", makeRecipe(3, 3,
	    sstv::image::FitMode::contain, {0, 0, 0},
	    {sstv::image::CropRect{std::numeric_limits<std::uint64_t>::max(), 0, 2, 1}}),
	    sstv::image::ImageErrorCode::invalidCrop);
	const std::array<std::array<sstv::core::Rgb8Pixel, 4>, 8> corners{{
		{{{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255}}},
		{{{0, 255, 0}, {255, 0, 0}, {255, 255, 255}, {0, 0, 255}}},
		{{{255, 255, 255}, {0, 0, 255}, {0, 255, 0}, {255, 0, 0}}},
		{{{0, 0, 255}, {255, 255, 255}, {255, 0, 0}, {0, 255, 0}}},
		{{{255, 0, 0}, {0, 0, 255}, {0, 255, 0}, {255, 255, 255}}},
		{{{0, 0, 255}, {255, 0, 0}, {255, 255, 255}, {0, 255, 0}}},
		{{{255, 255, 255}, {0, 255, 0}, {0, 0, 255}, {255, 0, 0}}},
		{{{0, 255, 0}, {255, 255, 255}, {255, 0, 0}, {0, 0, 255}}},
	}};
	for (std::uint8_t orientation = 1; orientation <= 8; ++orientation) {
		const std::uint16_t width = orientation >= 5 ? 3 : 5;
		const std::uint16_t height = orientation >= 5 ? 5 : 3;
		const auto output = prepare(fixtures / "orientation"
		    / ("orientation-" + std::to_string(orientation) + ".png"),
		    makeRecipe(width, height));
		const auto view = output.frame.view();
		requirePixel(view.pixel(0, 0), corners[orientation - 1U][0], "oriented TL wrong");
		const std::uint16_t right = static_cast<std::uint16_t>(width - 1U);
		const std::uint16_t bottom = static_cast<std::uint16_t>(height - 1U);
		requirePixel(view.pixel(right, 0), corners[orientation - 1U][1], "oriented TR wrong");
		requirePixel(view.pixel(0, bottom), corners[orientation - 1U][2], "oriented BL wrong");
		requirePixel(view.pixel(right, bottom), corners[orientation - 1U][3], "oriented BR wrong");
	}
	const auto orientedCrop = prepare(fixtures / "orientation/orientation-6.png",
	    makeRecipe(1, 1, sstv::image::FitMode::contain, {0, 0, 0},
	        {sstv::image::CropRect{0, 0, 1, 1}}));
	requirePixel(orientedCrop.frame.view().pixel(0, 0), {0, 0, 255},
	    "crop was not applied after orientation");
	const auto raw = prepare(fixtures / "orientation/orientation-6.png",
	    makeRecipe(5, 3, sstv::image::FitMode::contain, {0, 0, 0}, std::nullopt, false));
	requirePixel(raw.frame.view().pixel(0, 0), {255, 0, 0},
	    "disabled EXIF orientation transformed pixels");
}

void
testSamplesAlphaAndProfiles()
{
	const auto gray = prepare(fixtures / "gray.png", makeRecipe(3, 3));
	requirePixel(gray.frame.view().pixel(2, 0), {128, 128, 128},
	    "grayscale expansion is wrong");
	const auto gray16 = prepare(fixtures / "gray16.png", makeRecipe(2, 2));
	requirePixel(gray16.frame.view().pixel(1, 0), {128, 128, 128},
	    "16-bit conversion is wrong");
	const sstv::core::Rgb8Pixel background{10, 20, 30};
	const auto alpha = prepare(fixtures / "alpha.png",
	    makeRecipe(3, 2, sstv::image::FitMode::contain, background));
	requirePixel(alpha.frame.view().pixel(0, 0), background,
	    "transparent pixel did not use background");
	requirePixel(alpha.frame.view().pixel(2, 1), {255, 0, 0}, "opaque alpha changed");
	const auto scaled = prepare(fixtures / "alpha.png",
	    makeRecipe(30, 20, sstv::image::FitMode::cover, background));
	requirePixel(scaled.frame.view().pixel(0, 0), background,
	    "alpha resize produced an edge halo", 2);
	const auto profiled = prepare(fixtures / "profiled.png", makeRecipe(5, 3));
	require(profiled.source.hasEmbeddedProfile, "embedded profile was not detected");
	requirePixel(profiled.frame.view().pixel(0, 0), {255, 0, 0},
	    "embedded sRGB conversion changed a primary", 8);
	requireError(fixtures / "invalid-profile.png", makeRecipe(5, 3),
	    sstv::image::ImageErrorCode::invalidProfile);
	requireError(fixtures / "cmyk.jpg", makeRecipe(5, 3),
	    sstv::image::ImageErrorCode::unsupportedColourspace);
}

void
testLoadersAndRejections(const std::filesystem::path& temporary)
{
	const auto spoofed = prepare(fixtures / "spoofed.jpg", makeRecipe(5, 3));
	require(spoofed.source.format == sstv::image::RasterFormat::png,
	    "extension overrode actual loader");
	const auto jpeg = prepare(fixtures / "marker.jpg", makeRecipe(5, 3));
	require(jpeg.source.format == sstv::image::RasterFormat::jpeg, "JPEG loader not used");
	requirePixel(jpeg.frame.view().pixel(0, 0), {255, 0, 0},
	    "JPEG exceeded documented tolerance", 100);
	requireError(fixtures / "truncated.png", makeRecipe(5, 3),
	    sstv::image::ImageErrorCode::invalidMetadata);
	requireError(fixtures / "oversized-header.png", makeRecipe(5, 3),
	    sstv::image::ImageErrorCode::dimensionLimit);
	requireError(fixtures / "orientation/orientation-9.png", makeRecipe(5, 3),
	    sstv::image::ImageErrorCode::invalidOrientation);
	requireError(fixtures / "marker.webp", makeRecipe(5, 3),
	    sstv::image::ImageErrorCode::unsupportedLoader);
	requireError(fixtures / "marker.tiff", makeRecipe(5, 3),
	    sstv::image::ImageErrorCode::unsupportedLoader);
	requireError(fixtures / "animated.png", makeRecipe(1, 1),
	    sstv::image::ImageErrorCode::pageLimit);
	const auto svg = temporary / "input.svg";
	requireError("/dev/null", makeRecipe(5, 3), sstv::image::ImageErrorCode::invalidPath);
	std::ofstream(svg) << "<svg xmlns=\"http://www.w3.org/2000/svg\"/>";
	requireError(svg, makeRecipe(5, 3), sstv::image::ImageErrorCode::unsupportedLoader);
	const auto pdf = temporary / "input.pdf";
	std::ofstream(pdf) << "%PDF-1.4\n%%EOF\n";
	requireError(pdf, makeRecipe(5, 3), sstv::image::ImageErrorCode::unsupportedLoader);
	requireError("https://example.invalid/image.png", makeRecipe(5, 3),
	    sstv::image::ImageErrorCode::invalidPath);
	requireError(temporary, makeRecipe(5, 3), sstv::image::ImageErrorCode::invalidPath);
	const auto fifo = temporary / "input.fifo";
	require(::mkfifo(fifo.c_str(), S_IRUSR | S_IWUSR) == 0, "FIFO creation failed");
	requireError(fifo, makeRecipe(5, 3), sstv::image::ImageErrorCode::invalidPath);
	const auto socketPath = temporary / "input.socket";
	const int socketDescriptor = ::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	require(socketDescriptor >= 0, "socket creation failed");
	struct sockaddr_un address{};
	address.sun_family = AF_UNIX;
	const std::string socketName = socketPath.string();
	require(socketName.size() < sizeof(address.sun_path), "socket path is too long");
	std::copy(socketName.begin(), socketName.end(), address.sun_path);
	require(::bind(socketDescriptor, reinterpret_cast<const sockaddr*>(&address),
	    sizeof(address)) == 0, "socket bind failed");
	requireError(socketPath, makeRecipe(5, 3), sstv::image::ImageErrorCode::invalidPath);
	::close(socketDescriptor);
}

void
testLimitsAndPublication(const std::filesystem::path& temporary)
{
	auto limits = sstv::image::defaultImageLoadLimits();
	limits.maximumFileBytes = 1;
	requireError(fixtures / "marker.png", makeRecipe(5, 3),
	    sstv::image::ImageErrorCode::fileTooLarge, limits);
	limits = sstv::image::defaultImageLoadLimits();
	limits.maximumDecodedPixels = 14;
	requireError(fixtures / "marker.png", makeRecipe(5, 3),
	    sstv::image::ImageErrorCode::pixelLimit, limits);
	limits = sstv::image::defaultImageLoadLimits();
	limits.maximumOutputWidth = 4;
	requireError(fixtures / "marker.png", makeRecipe(5, 3),
	    sstv::image::ImageErrorCode::dimensionLimit, limits);
	const auto first = prepare(fixtures / "marker.png", makeRecipe(13, 11));
	const auto second = prepare(fixtures / "marker.png", makeRecipe(13, 11));
	require(equalFrames(first.frame.view(), second.frame.view()),
	    "preparation is not deterministic");
	const auto output = temporary / "prepared.png";
	writeFrame(output, first.frame.view());
	auto refused = sstv::image::writeRgb8PngAtomic(output, second.frame.view(), false);
	const auto* error = std::get_if<sstv::image::ImageError>(&refused);
	require(error != nullptr && error->code == sstv::image::ImageErrorCode::destinationExists,
	    "overwrite was not refused");
	writeFrame(output, second.frame.view(), true);
	std::ifstream stream(output, std::ios::binary);
	const std::string bytes((std::istreambuf_iterator<char>(stream)), {});
	require(bytes.find("must be stripped") == std::string::npos,
	    "prepared PNG retained source metadata");
	auto failed = sstv::image::writeRgb8PngAtomic(
	    temporary / "missing/output.png", second.frame.view(), false);
	require(std::holds_alternative<sstv::image::ImageError>(failed),
	    "invalid output destination was accepted");
	for (const auto& entry : std::filesystem::directory_iterator(temporary)) {
		require(entry.path().filename().string().find(".tmp.") == std::string::npos,
		    "failed publication abandoned a temporary file");
	}
}

} // namespace

int
main()
{
	const TemporaryDirectory temporary;
	testExactIntegration(temporary.path());
	testDimensionsAndFit(temporary.path());
	testCropsAndOrientation();
	testSamplesAlphaAndProfiles();
	testLoadersAndRejections(temporary.path());
	testLimitsAndPublication(temporary.path());
	return 0;
}
