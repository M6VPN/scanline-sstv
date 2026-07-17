// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m1e_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/martin_m1.hpp>
#include <sstv/analog/offline_tx.hpp>
#include <sstv/analog/paired_luma_chroma.hpp>
#include <sstv/analog/pd_120.hpp>
#include <sstv/analog/robot_36.hpp>
#include <sstv/analog/scottie_s1.hpp>
#include <sstv/analog/vis.hpp>
#include <sstv/core/mode.hpp>
#include <sstv/core/rgb8_frame.hpp>
#include <sstv/core/timing.hpp>
#include <sstv/dsp/tone_renderer.hpp>
#include <sstv/offline/wav_writer.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace {

void
require(const bool condition, const std::string& message)
{
	if (!condition) {
		throw std::runtime_error(message);
	}
}

template<typename Exception, typename Function>
void
requireThrows(Function function, const std::string& message)
{
	try {
		function();
	} catch (const Exception&) {
		return;
	}
	throw std::runtime_error(message);
}

[[nodiscard]] sstv::core::Rgb8Frame
makeFrame(const std::uint16_t width, const std::uint16_t height,
	const std::span<const sstv::core::Rgb8Pixel> pixels)
{
	return sstv::core::Rgb8Frame(width, height,
	    std::vector<sstv::core::Rgb8Pixel>(pixels.begin(), pixels.end()));
}

[[nodiscard]] bool
eventsEqual(const std::span<const sstv::core::ToneEvent> left,
	const std::span<const sstv::core::ToneEvent> right)
{
	if (left.size() != right.size()) {
		return false;
	}
	for (std::size_t index = 0; index < left.size(); ++index) {
		if (!(left[index].duration() == right[index].duration())
		    || left[index].frequencyHz() != right[index].frequencyHz()
		    || left[index].amplitude() != right[index].amplitude()) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] std::vector<float>
renderEvents(const std::span<const sstv::core::ToneEvent> events,
	const std::size_t blockSize)
{
	sstv::dsp::ToneRenderer renderer(events, 48'000);
	std::vector<float> result;
	result.reserve(static_cast<std::size_t>(renderer.frameCount()));
	std::vector<float> block(blockSize);
	while (!renderer.finished()) {
		const std::size_t count = renderer.render(block);
		result.insert(result.end(), block.begin(),
		    block.begin() + static_cast<std::ptrdiff_t>(count));
	}
	return result;
}

void
testColourConversion()
{
	using sstv::core::Rgb8Pixel;
	constexpr std::array<Rgb8Pixel, 8> pixels{{
		{255, 0, 0}, {0, 255, 0}, {255, 0, 0}, {0, 0, 255},
		{0, 0, 255}, {255, 255, 255}, {0, 255, 0}, {255, 255, 255},
	}};
	const auto first = sstv::analog::convertPd120Colour(makeFrame(4, 2, pixels).view());
	const auto second = sstv::analog::convertPd120Colour(makeFrame(4, 2, pixels).view());
	const auto colour = first.view();
	constexpr std::array<std::uint8_t, 8> expectedLuma{
		81, 144, 81, 40, 40, 234, 144, 234,
	};
	constexpr std::array<std::uint8_t, 4> expectedRed{174, 81, 136, 118};
	constexpr std::array<std::uint8_t, 4> expectedBlue{164, 90, 71, 183};
	require(colour.lumas().size() == 8U && colour.redDifferences().size() == 4U
	    && colour.blueDifferences().size() == 4U && colour.chromaWidth() == 4U
	    && colour.chromaHeight() == 1U,
	    "PD120 4:4:0 plane geometry is wrong");
	require(std::equal(colour.lumas().begin(), colour.lumas().end(), expectedLuma.begin())
	    && std::equal(colour.redDifferences().begin(), colour.redDifferences().end(),
	        expectedRed.begin())
	    && std::equal(colour.blueDifferences().begin(), colour.blueDifferences().end(),
	        expectedBlue.begin()),
	    "PD120 conversion, vertical averaging, or component order differs from the vector");
	require(std::equal(colour.lumas().begin(), colour.lumas().end(),
	        second.view().lumas().begin())
	    && std::equal(colour.redDifferences().begin(), colour.redDifferences().end(),
	        second.view().redDifferences().begin())
	    && std::equal(colour.blueDifferences().begin(), colour.blueDifferences().end(),
	        second.view().blueDifferences().begin()),
	    "PD120 conversion is not deterministic");
	require(colour.redDifference(0, 0) == 174U
	    && colour.redDifference(1, 0) == 81U,
	    "PD120 horizontally subsampled the full-width chroma plane");
	constexpr std::array<Rgb8Pixel, 18> primaryPairs{{
		{0, 0, 0}, {255, 255, 255}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255},
		{127, 127, 127}, {128, 128, 128}, {1, 2, 3}, {254, 253, 252},
		{0, 0, 0}, {255, 255, 255}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255},
		{127, 127, 127}, {128, 128, 128}, {1, 2, 3}, {254, 253, 252},
	}};
	const auto primaryFrame = sstv::analog::convertPd120Colour(
	    makeFrame(9, 2, primaryPairs).view());
	const auto primary = primaryFrame.view();
	constexpr std::array<std::uint8_t, 9> expectedPrimaryLuma{
		16, 234, 81, 144, 40, 125, 125, 17, 233,
	};
	constexpr std::array<std::uint8_t, 9> expectedPrimaryRed{
		128, 128, 239, 34, 109, 128, 128, 127, 128,
	};
	constexpr std::array<std::uint8_t, 9> expectedPrimaryBlue{
		128, 128, 90, 53, 239, 128, 128, 128, 127,
	};
	for (std::uint16_t x = 0; x < 9U; ++x) {
		require(primary.luma(x, 0) == expectedPrimaryLuma[x]
		    && primary.redDifference(x, 0) == expectedPrimaryRed[x]
		    && primary.blueDifference(x, 0) == expectedPrimaryBlue[x],
		    "PD120 primary, neutral, clipping, or rounding conversion is wrong");
	}
}

void
testColourStorageValidation()
{
	using sstv::analog::LumaColourDifference440Frame;
	requireThrows<std::invalid_argument>([] {
		LumaColourDifference440Frame invalid(0, 2, {}, {}, {});
	}, "4:4:0 storage accepted zero width");
	requireThrows<std::invalid_argument>([] {
		LumaColourDifference440Frame invalid(3, 3,
		    std::vector<std::uint8_t>(9), std::vector<std::uint8_t>(3),
		    std::vector<std::uint8_t>(3));
	}, "4:4:0 storage accepted odd height");
	requireThrows<std::invalid_argument>([] {
		LumaColourDifference440Frame invalid(3, 2,
		    std::vector<std::uint8_t>(5), std::vector<std::uint8_t>(3),
		    std::vector<std::uint8_t>(3));
	}, "4:4:0 storage accepted a short luma plane");
	requireThrows<std::invalid_argument>([] {
		const std::vector<sstv::core::Rgb8Pixel> pixels(9);
		(void)sstv::analog::convertPd120Colour(sstv::core::Rgb8View(3, 3, pixels));
	}, "PD120 conversion accepted odd height");
	const LumaColourDifference440Frame valid(3, 2,
	    std::vector<std::uint8_t>(6), std::vector<std::uint8_t>(3),
	    std::vector<std::uint8_t>(3));
	requireThrows<std::out_of_range>([&valid] {
		(void)valid.view().redDifference(3, 0);
	}, "4:4:0 storage accepted an out-of-range chroma coordinate");
}

void
testDescriptorAndVis()
{
	using sstv::analog::Pd120Component;
	using sstv::analog::Pd120FixedToneSegment;
	using sstv::analog::Pd120ScanSegment;
	using sstv::core::Duration;
	const auto& descriptor = sstv::analog::pd120Descriptor();
	require(descriptor.id == "pd-120" && descriptor.displayName == "PD120"
	    && descriptor.visCode == 95 && descriptor.width == 640
	    && descriptor.height == 496 && descriptor.preImageSchedule.empty()
	    && descriptor.pairSchedule.size() == 6U,
	    "PD120 descriptor identity, dimensions, or pair schedule is wrong");
	const auto& sync = std::get<Pd120FixedToneSegment>(descriptor.pairSchedule[0]);
	const auto& porch = std::get<Pd120FixedToneSegment>(descriptor.pairSchedule[1]);
	require(sync.duration == Duration::fromMicroseconds(20'000)
	    && sync.frequencyHz == 1'200.0
	    && porch.duration == Duration::fromMicroseconds(2'080)
	    && porch.frequencyHz == 1'500.0,
	    "PD120 sync or porch differs from the evidence record");
	constexpr std::array<Pd120Component, 4> components{{
		Pd120Component::firstLuma,
		Pd120Component::redDifference,
		Pd120Component::blueDifference,
		Pd120Component::secondLuma,
	}};
	for (std::size_t index = 0; index < components.size(); ++index) {
		const auto& scan = std::get<Pd120ScanSegment>(descriptor.pairSchedule[index + 2U]);
		require(scan.component == components[index]
		    && scan.duration == Duration::fromMicroseconds(121'600)
		    && scan.sampleWidth == 640U,
		    "PD120 scan order, duration, or horizontal sampling is wrong");
	}
	constexpr std::array<bool, 8> expectedBits{
		true, true, true, true, true, false, true, false,
	};
	require(sstv::analog::makeVisBits(95) == expectedBits,
	    "PD120 VIS bits or even parity are wrong");
	constexpr std::array<double, 13> expectedFrequencies{
		1'900, 1'200, 1'900, 1'200, 1'100, 1'100, 1'100,
		1'100, 1'100, 1'300, 1'100, 1'300, 1'200,
	};
	const auto header = sstv::analog::makeVisHeader(95, 0.8F);
	for (std::size_t index = 0; index < header.size(); ++index) {
		require(header[index].frequencyHz() == expectedFrequencies[index],
		    "PD120 VIS frequency sequence differs from the vector");
	}
}

void
testEncodingAndTiming()
{
	using sstv::core::Duration;
	const auto frame = sstv::core::makeDiagnosticPattern(640, 496);
	const auto events = sstv::analog::encodePd120(frame.view(), 0.8F);
	require(events.size() == 635'389U,
	    "PD120 event count differs from the independent vector");
	const Duration expectedDuration(1'587'663, 12'500);
	require(sstv::analog::pd120TransmissionDuration() == expectedDuration,
	    "PD120 complete duration differs from the independent vector");
	Duration summed(0, 1);
	for (const auto& event : events) {
		summed = summed + event.duration();
	}
	require(summed == expectedDuration, "PD120 event durations do not sum exactly");
	constexpr std::array<std::pair<std::size_t, std::uint64_t>, 18> boundaries{{
		{12, 43'680}, {13, 44'640}, {14, 44'739}, {15, 44'748},
		{654, 50'576}, {655, 50'585}, {1'294, 56'413}, {1'295, 56'422},
		{1'934, 62'250}, {1'935, 62'259}, {2'574, 68'087}, {2'575, 69'047},
		{2'576, 69'146}, {317'701, 3'071'112}, {317'702, 3'071'212},
		{632'827, 6'073'178}, {632'828, 6'073'278}, {635'388, 6'096'625},
	}};
	sstv::core::SampleBoundaryScheduler scheduler(48'000);
	std::size_t selected = 0;
	for (std::size_t index = 0; index < events.size(); ++index) {
		const std::uint64_t boundary = scheduler.advance(events[index].duration());
		if (selected < boundaries.size() && index == boundaries[selected].first) {
			require(boundary == boundaries[selected].second,
			    "PD120 event boundary differs from the independent vector");
			++selected;
		}
	}
	require(selected == boundaries.size(), "PD120 boundary vector was incomplete");
	require(events[13].frequencyHz() == 1'200.0
	    && events[14].frequencyHz() == 1'500.0
	    && events[2'575].frequencyHz() == 1'200.0
	    && events[317'701].frequencyHz() == 1'200.0
	    && events[632'827].frequencyHz() == 1'200.0,
	    "PD120 first, second, middle, or final pair sync placement is wrong");
	require(std::abs(events[15].frequencyHz()
	        - sstv::analog::pd120ComponentFrequency(81)) < 1.0e-10
	    && std::abs(events[655].frequencyHz()
	        - sstv::analog::pd120ComponentFrequency(239)) < 1.0e-10
	    && std::abs(events[1'295].frequencyHz()
	        - sstv::analog::pd120ComponentFrequency(90)) < 1.0e-10
	    && std::abs(events[1'935].frequencyHz()
	        - sstv::analog::pd120ComponentFrequency(81)) < 1.0e-10
	    && std::abs(events[635'388].frequencyHz()
	        - sstv::analog::pd120ComponentFrequency(234)) < 1.0e-10,
	    "PD120 selected luma or colour-difference tones are wrong");
	constexpr std::array<std::pair<std::uint32_t, std::uint64_t>, 5> frameCounts{{
		{8'000, 1'016'104}, {11'025, 1'400'318}, {44'100, 5'601'275},
		{48'000, 6'096'625}, {96'000, 12'193'251},
	}};
	for (const auto& [rate, expected] : frameCounts) {
		require(sstv::core::sampleCount(expectedDuration, rate) == expected,
		    "PD120 frame count differs from the independent vector");
	}
	const Duration pairDuration = Duration::fromMicroseconds(508'480);
	sstv::core::SampleBoundaryScheduler repeated(11'025);
	for (std::uint64_t pair = 1; pair <= 100'000; ++pair) {
		require(repeated.advance(pairDuration)
		    == sstv::core::sampleCount(pairDuration * pair, 11'025),
		    "PD120 repeated pair scheduling accumulated drift");
	}
	require(sstv::analog::pd120ComponentFrequency(0) == 1'500.0
	    && sstv::analog::pd120ComponentFrequency(255) == 2'300.0
	    && std::abs(sstv::analog::pd120ComponentFrequency(128)
	        - 1'901.5686274509803) < 1.0e-10,
	    "PD120 component-frequency mapping is wrong");
	requireThrows<std::invalid_argument>([] {
		const auto wrong = sstv::core::makeDiagnosticPattern(640, 480);
		(void)sstv::analog::encodePd120(wrong.view(), 0.8F);
	}, "PD120 encoder accepted wrong dimensions");
}

void
testRegistryAndDispatch()
{
	using sstv::analog::OfflineTransmission;
	using sstv::core::ModeCapability;
	const auto* const pd120 = sstv::core::find_mode("pd-120");
	require(pd120 != nullptr && pd120->width == 640 && pd120->height == 496
	    && pd120->vis_code == 95
	    && pd120->colour_encoding == sstv::core::ColourEncoding::lumaColourDifference
	    && pd120->offline_tx_strategy == sstv::core::OfflineTxStrategy::pd120,
	    "PD120 registry metadata is wrong");
	require(pd120->capabilities.contains(ModeCapability::offlineTestPatternTx)
	    && pd120->capabilities.contains(ModeCapability::offlineImageTx)
	    && !pd120->capabilities.contains(ModeCapability::liveTx)
	    && !pd120->capabilities.contains(ModeCapability::receive),
	    "PD120 capabilities are not truthful");
	const auto frame = sstv::core::makeDiagnosticPattern(640, 496);
	auto result = sstv::analog::encodeOfflineTransmission("pd-120",
	    ModeCapability::offlineTestPatternTx, frame.view(), 0.8F);
	require(std::holds_alternative<OfflineTransmission>(result),
	    "PD120 central dispatch rejected its advertised capability");
	const auto& transmission = std::get<OfflineTransmission>(result);
	require(eventsEqual(transmission.events,
	        sstv::analog::encodePd120(frame.view(), 0.8F))
	    && transmission.duration == sstv::analog::pd120TransmissionDuration(),
	    "PD120 central dispatch differs from the direct encoder");
	auto missing = sstv::analog::encodeOfflineTransmission("pd-120",
	    ModeCapability::liveTx, frame.view(), 0.8F);
	require(std::get<sstv::analog::OfflineTxError>(missing).code
	    == sstv::analog::OfflineTxErrorCode::missingCapability,
	    "PD120 dispatch accepted live TX");
	const auto wrong = sstv::core::makeDiagnosticPattern(640, 480);
	auto wrongResult = sstv::analog::encodeOfflineTransmission("pd-120",
	    ModeCapability::offlineImageTx, wrong.view(), 0.8F);
	require(std::get<sstv::analog::OfflineTxError>(wrongResult).code
	    == sstv::analog::OfflineTxErrorCode::invalidFrameDimensions,
	    "PD120 dispatch accepted wrong dimensions");
	for (const auto& mode : sstv::core::built_in_modes()) {
		if (!mode.capabilities.contains(ModeCapability::offlineTestPatternTx)) {
			continue;
		}
		const auto diagnostic = sstv::core::makeDiagnosticPattern(mode.width, mode.height);
		auto advertised = sstv::analog::encodeOfflineTransmission(mode.id,
		    ModeCapability::offlineTestPatternTx, diagnostic.view(), 0.8F);
		require(std::holds_alternative<OfflineTransmission>(advertised),
		    "an advertised offline TX strategy is not dispatchable");
	}
}

void
testRenderer()
{
	const auto frame = sstv::core::makeDiagnosticPattern(640, 496);
	const auto events = sstv::analog::encodePd120(frame.view(), 0.8F);
	const std::span<const sstv::core::ToneEvent> pairs(events.data() + 13, 5'124);
	const std::vector<float> reference = renderEvents(pairs, 65'536);
	for (const std::size_t blockSize : {1U, 257U, 4'096U, 65'536U}) {
		const std::vector<float> actual = renderEvents(pairs, blockSize);
		require(actual.size() == reference.size(),
		    "PD120 renderer block size changed frame count");
		for (std::size_t index = 0; index < actual.size(); ++index) {
			require(std::abs(actual[index] - reference[index]) <= 1.0e-7F,
			    "PD120 renderer output changed with block size");
		}
	}
	require(std::abs(reference[1'060]) > 0.01F,
	    "PD120 oscillator phase reset at a tone boundary");
	for (const float sample : reference) {
		require(std::isfinite(sample) && std::abs(sample) <= 0.8F + 1.0e-7F,
		    "PD120 renderer produced a non-finite or unbounded sample");
		(void)sstv::offline::floatToPcm16(sample);
	}
}

void
testWavAndCleanup()
{
	const auto frame = sstv::core::makeDiagnosticPattern(640, 496);
	const auto events = sstv::analog::encodePd120(frame.view(), 0.8F);
	const std::filesystem::path directory = std::filesystem::temp_directory_path()
	    / "scanline-sstv-m1e-tests";
	std::error_code ignored;
	std::filesystem::remove_all(directory, ignored);
	std::filesystem::create_directories(directory);
	const std::filesystem::path path = directory / "pd-120.wav";
	const auto metadata = sstv::offline::writePcm16WavAtomic(
	    path, events, 8'000, false);
	require(metadata.sampleRate == 8'000 && metadata.frameCount == 1'016'104
	    && std::filesystem::file_size(path) == 2'032'252,
	    "PD120 WAV metadata or RIFF size is wrong");
	std::array<char, 44> header{};
	std::ifstream input(path, std::ios::binary);
	input.read(header.data(), static_cast<std::streamsize>(header.size()));
	require(input.gcount() == static_cast<std::streamsize>(header.size())
	    && std::string_view(header.data(), 4) == "RIFF"
	    && std::string_view(header.data() + 8, 4) == "WAVE"
	    && std::string_view(header.data() + 36, 4) == "data",
	    "PD120 WAV header is invalid");
	requireThrows<std::runtime_error>([&path, &events] {
		(void)sstv::offline::writePcm16WavAtomic(path, events, 8'000, false);
	}, "PD120 WAV overwrote an existing destination");
	(void)sstv::offline::writePcm16WavAtomic(path, events, 8'000, true);
	const std::filesystem::path blocked = directory / "blocked";
	std::filesystem::create_directory(blocked);
	requireThrows<std::system_error>([&blocked, &events] {
		(void)sstv::offline::writePcm16WavAtomic(blocked, events, 8'000, true);
	}, "PD120 WAV failure path unexpectedly published output");
	for (const auto& entry : std::filesystem::directory_iterator(directory)) {
		require(!entry.path().filename().string().starts_with("blocked.tmp."),
		    "PD120 WAV failure abandoned a temporary file");
	}
	std::filesystem::remove_all(directory);
}

void
testFrozenModes()
{
	const auto rgbFrame = sstv::core::makeDiagnosticPattern(320, 256);
	const auto robotFrame = sstv::core::makeDiagnosticPattern(320, 240);
	const auto martin = sstv::analog::encodeMartinM1(rgbFrame.view(), 0.8F);
	const auto scottie = sstv::analog::encodeScottieS1(rgbFrame.view(), 0.8F);
	const auto robot = sstv::analog::encodeRobot36(robotFrame.view(), 0.8F);
	require(martin.size() == 247'053U
	    && sstv::analog::martinM1TransmissionDuration()
	        == sstv::core::Duration(7'200'011, 62'500)
	    && sstv::core::sampleCount(sstv::analog::martinM1TransmissionDuration(),
	        48'000) == 5'529'608,
	    "Martin M1 frozen event count, duration, or frame count changed");
	require(scottie.size() == 246'797U
	    && sstv::analog::scottieS1TransmissionDuration()
	        == sstv::core::Duration(1'381'679, 12'500)
	    && sstv::core::sampleCount(sstv::analog::scottieS1TransmissionDuration(),
	        48'000) == 5'305'647,
	    "Scottie S1 frozen event count, duration, or frame count changed");
	require(robot.size() == 116'173U
	    && sstv::analog::robot36TransmissionDuration() == sstv::core::Duration(3'691, 100)
	    && sstv::core::sampleCount(sstv::analog::robot36TransmissionDuration(), 48'000)
	        == 1'771'680,
	    "Robot 36 frozen event count, duration, or frame count changed");
	constexpr std::array<sstv::core::Rgb8Pixel, 4> robotPixels{{
		{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255},
	}};
	const auto robotColourFrame = sstv::analog::convertRobot36Colour(
	    makeFrame(2, 2, robotPixels).view());
	const auto robotColour = robotColourFrame.view();
	require(robotColour.redDifference(0, 0) == 127
	    && robotColour.blueDifference(0, 0) == 127,
	    "Robot 36 frozen 4:2:0 conversion changed");
}

} // namespace

int
main()
{
	testColourConversion();
	testColourStorageValidation();
	testDescriptorAndVis();
	testEncodingAndTiming();
	testRegistryAndDispatch();
	testRenderer();
	testWavAndCleanup();
	testFrozenModes();
	return 0;
}
