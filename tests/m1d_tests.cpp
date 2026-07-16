// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m1d_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/luma_chroma.hpp>
#include <sstv/analog/martin_m1.hpp>
#include <sstv/analog/offline_tx.hpp>
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
testColourValues()
{
	using sstv::core::Rgb8Pixel;
	constexpr std::array<Rgb8Pixel, 24> pixels{{
		{0, 0, 0}, {0, 0, 0}, {255, 255, 255}, {255, 255, 255},
		{255, 0, 0}, {255, 0, 0}, {0, 255, 0}, {0, 255, 0},
		{0, 0, 255}, {0, 0, 255}, {128, 128, 128}, {128, 128, 128},
		{0, 0, 0}, {0, 0, 0}, {255, 255, 255}, {255, 255, 255},
		{255, 0, 0}, {255, 0, 0}, {0, 255, 0}, {0, 255, 0},
		{0, 0, 255}, {0, 0, 255}, {128, 128, 128}, {128, 128, 128},
	}};
	const auto frame = makeFrame(12, 2, pixels);
	const auto converted = sstv::analog::convertRobot36Colour(frame.view());
	const auto colour = converted.view();
	constexpr std::array<std::uint8_t, 6> expectedLuma{16, 234, 81, 144, 40, 125};
	constexpr std::array<std::uint8_t, 6> expectedRed{128, 128, 239, 34, 109, 128};
	constexpr std::array<std::uint8_t, 6> expectedBlue{128, 128, 90, 53, 239, 128};
	for (std::uint16_t x = 0; x < 6U; ++x) {
		require(colour.luma(static_cast<std::uint16_t>(x * 2U), 0)
		    == expectedLuma[x],
		    "Robot 36 luma conversion differs from the vector");
	}
	for (std::uint16_t x = 0; x < 6U; ++x) {
		require(colour.redDifference(x, 0) == expectedRed[x]
		    && colour.blueDifference(x, 0) == expectedBlue[x],
		    "Robot 36 primary colour difference conversion is wrong");
	}
	constexpr std::array<Rgb8Pixel, 4> nearLimits{{
		{1, 2, 3}, {1, 2, 3}, {254, 253, 252}, {254, 253, 252},
	}};
	const auto limitsFrame = sstv::analog::convertRobot36Colour(
	    makeFrame(2, 2, nearLimits).view());
	const auto limits = limitsFrame.view();
	require(limits.luma(0, 0) == 17 && limits.redDifference(0, 0) == 127
	    && limits.blueDifference(0, 0) == 127,
	    "Robot 36 near-boundary conversion is wrong");
}

void
testSubsampling()
{
	using sstv::core::Rgb8Pixel;
	constexpr std::array<Rgb8Pixel, 4> block{{
		{255, 0, 0}, {0, 255, 0},
		{0, 0, 255}, {255, 255, 255},
	}};
	const auto first = sstv::analog::convertRobot36Colour(makeFrame(2, 2, block).view());
	const auto second = sstv::analog::convertRobot36Colour(makeFrame(2, 2, block).view());
	require(first.view().redDifference(0, 0) == 127
	    && first.view().blueDifference(0, 0) == 127,
	    "Robot 36 2 by 2 chroma average or halfway truncation is wrong");
	require(first.view().lumas().size() == 4U
	    && first.view().redDifferences().size() == 1U
	    && first.view().blueDifferences().size() == 1U,
	    "Robot 36 4:2:0 plane dimensions are wrong");
	require(first.view().lumas().size() == second.view().lumas().size()
	    && std::equal(first.view().lumas().begin(), first.view().lumas().end(),
	        second.view().lumas().begin())
	    && std::equal(first.view().redDifferences().begin(),
	        first.view().redDifferences().end(), second.view().redDifferences().begin())
	    && std::equal(first.view().blueDifferences().begin(),
	        first.view().blueDifferences().end(), second.view().blueDifferences().begin()),
	    "Robot 36 conversion is not deterministic");
	constexpr std::array<Rgb8Pixel, 16> checker{{
		{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255},
		{255, 0, 0}, {0, 255, 0}, {0, 0, 255}, {255, 255, 255},
		{0, 255, 0}, {255, 0, 0}, {255, 255, 255}, {0, 0, 255},
		{0, 255, 0}, {255, 0, 0}, {255, 255, 255}, {0, 0, 255},
	}};
	const auto checkedFrame = sstv::analog::convertRobot36Colour(
	    makeFrame(4, 4, checker).view());
	const auto checked = checkedFrame.view();
	require(checked.redDifference(0, 0) == 136
	    && checked.blueDifference(0, 0) == 71
	    && checked.redDifference(1, 1) == 118
	    && checked.blueDifference(1, 1) == 183,
	    "Robot 36 horizontal, vertical, or final-pair chroma handling is wrong");
}

void
testColourStorageValidation()
{
	using sstv::analog::LumaColourDifference420Frame;
	requireThrows<std::invalid_argument>([] {
		LumaColourDifference420Frame invalid(0, 2, {}, {}, {});
	}, "4:2:0 storage accepted zero width");
	requireThrows<std::invalid_argument>([] {
		LumaColourDifference420Frame invalid(3, 2,
		    std::vector<std::uint8_t>(6), std::vector<std::uint8_t>(1),
		    std::vector<std::uint8_t>(1));
	}, "4:2:0 storage accepted odd width");
	requireThrows<std::invalid_argument>([] {
		LumaColourDifference420Frame invalid(2, 2,
		    std::vector<std::uint8_t>(3), std::vector<std::uint8_t>(1),
		    std::vector<std::uint8_t>(1));
	}, "4:2:0 storage accepted a short luma plane");
	requireThrows<std::invalid_argument>([] {
		const std::vector<sstv::core::Rgb8Pixel> pixels(6);
		(void)sstv::analog::convertRobot36Colour(
		    sstv::core::Rgb8View(3, 2, pixels));
	}, "Robot 36 colour conversion accepted odd dimensions");
	const LumaColourDifference420Frame valid(2, 2,
	    std::vector<std::uint8_t>(4), std::vector<std::uint8_t>(1),
	    std::vector<std::uint8_t>(1));
	requireThrows<std::out_of_range>([&valid] {
		(void)valid.view().redDifference(1, 0);
	}, "4:2:0 storage accepted an out-of-range chroma coordinate");
}

void
testDescriptorAndVis()
{
	using sstv::analog::Robot36Component;
	using sstv::analog::Robot36FixedToneSegment;
	using sstv::analog::Robot36ScanSegment;
	using sstv::core::Duration;
	const auto& descriptor = sstv::analog::robot36Descriptor();
	require(descriptor.id == "robot-36" && descriptor.displayName == "Robot 36"
	    && descriptor.visCode == 8 && descriptor.width == 320
	    && descriptor.height == 240,
	    "Robot 36 descriptor identity or dimensions are wrong");
	require(descriptor.preImageSchedule.empty()
	    && descriptor.evenLineSchedule.size() == 6U
	    && descriptor.oddLineSchedule.size() == 6U,
	    "Robot 36 line schedules are incomplete");
	const auto& evenId = std::get<Robot36FixedToneSegment>(
	    descriptor.evenLineSchedule[3]);
	const auto& oddId = std::get<Robot36FixedToneSegment>(
	    descriptor.oddLineSchedule[3]);
	const auto& evenScan = std::get<Robot36ScanSegment>(
	    descriptor.evenLineSchedule[5]);
	const auto& oddScan = std::get<Robot36ScanSegment>(
	    descriptor.oddLineSchedule[5]);
	require(evenId.duration == Duration::fromMicroseconds(4'500)
	    && evenId.frequencyHz == 1'500.0
	    && oddId.duration == Duration::fromMicroseconds(4'500)
	    && oddId.frequencyHz == 2'300.0
	    && evenScan.component == Robot36Component::redDifference
	    && oddScan.component == Robot36Component::blueDifference,
	    "Robot 36 component parity or identification is wrong");
	const auto& sync = std::get<Robot36FixedToneSegment>(
	    descriptor.evenLineSchedule[0]);
	const auto& syncPorch = std::get<Robot36FixedToneSegment>(
	    descriptor.evenLineSchedule[1]);
	const auto& luma = std::get<Robot36ScanSegment>(
	    descriptor.evenLineSchedule[2]);
	const auto& componentPorch = std::get<Robot36FixedToneSegment>(
	    descriptor.evenLineSchedule[4]);
	require(sync.duration == Duration::fromMicroseconds(9'000)
	    && sync.frequencyHz == 1'200.0
	    && syncPorch.duration == Duration::fromMicroseconds(3'000)
	    && syncPorch.frequencyHz == 1'500.0
	    && luma.component == Robot36Component::lumaY
	    && luma.duration == Duration::fromMicroseconds(88'000)
	    && componentPorch.duration == Duration::fromMicroseconds(1'500)
	    && componentPorch.frequencyHz == 1'900.0
	    && evenScan.duration == Duration::fromMicroseconds(44'000)
	    && oddScan.duration == Duration::fromMicroseconds(44'000),
	    "Robot 36 line timing differs from the evidence record");
	constexpr std::array<double, 13> frequencies{
		1'900, 1'200, 1'900, 1'200, 1'300, 1'300, 1'300,
		1'100, 1'300, 1'300, 1'300, 1'100, 1'200,
	};
	const auto bits = sstv::analog::makeVisBits(8);
	constexpr std::array<bool, 8> expectedBits{
		false, false, false, true, false, false, false, true,
	};
	require(bits == expectedBits, "Robot 36 VIS bits or parity are wrong");
	const auto header = sstv::analog::makeVisHeader(8, 0.8F);
	for (std::size_t index = 0; index < header.size(); ++index) {
		require(header[index].frequencyHz() == frequencies[index],
		    "Robot 36 VIS sequence differs from the vector");
	}
}

void
testEncodingAndTiming()
{
	using sstv::core::Duration;
	const auto frame = sstv::core::makeDiagnosticPattern(320, 240);
	const auto events = sstv::analog::encodeRobot36(frame.view(), 0.8F);
	require(events.size() == 116'173U,
	    "Robot 36 event count differs from the independent vector");
	const Duration expectedDuration(3'691, 100);
	require(sstv::analog::robot36TransmissionDuration() == expectedDuration,
	    "Robot 36 duration differs from the independent vector");
	Duration summed(0, 1);
	for (const auto& event : events) {
		summed = summed + event.duration();
	}
	require(summed == expectedDuration, "Robot 36 event durations do not sum exactly");
	constexpr std::array<std::pair<std::size_t, std::uint64_t>, 15> boundaries{{
		{12, 43'680}, {13, 44'112}, {14, 44'256}, {333, 48'466},
		{334, 48'480}, {335, 48'696}, {496, 50'880}, {497, 51'312},
		{498, 51'456}, {817, 55'666}, {818, 55'680}, {819, 55'896},
		{115'205, 1'757'712}, {115'689, 1'764'912}, {116'172, 1'771'680},
	}};
	sstv::core::SampleBoundaryScheduler scheduler(48'000);
	std::size_t selected = 0;
	for (std::size_t index = 0; index < events.size(); ++index) {
		const std::uint64_t boundary = scheduler.advance(events[index].duration());
		if (selected < boundaries.size() && index == boundaries[selected].first) {
			require(boundary == boundaries[selected].second,
			    "Robot 36 event boundary differs from the independent vector");
			++selected;
		}
	}
	require(selected == boundaries.size(), "Robot 36 boundary vector was incomplete");
	require(events[13].frequencyHz() == 1'200.0
	    && events[14].frequencyHz() == 1'500.0
	    && events[335].frequencyHz() == 1'500.0
	    && events[336].frequencyHz() == 1'900.0
	    && events[497].frequencyHz() == 1'200.0
	    && events[819].frequencyHz() == 2'300.0
	    && events[820].frequencyHz() == 1'900.0
	    && events[115'205].frequencyHz() == 1'200.0
	    && events[115'689].frequencyHz() == 1'200.0,
	    "Robot 36 first, second, penultimate, or final line order is wrong");
	require(std::abs(events[15].frequencyHz()
	        - sstv::analog::robot36ComponentFrequency(81)) < 1.0e-10
	    && std::abs(events[337].frequencyHz()
	        - sstv::analog::robot36ComponentFrequency(239)) < 1.0e-10
	    && std::abs(events[499].frequencyHz()
	        - sstv::analog::robot36ComponentFrequency(81)) < 1.0e-10
	    && std::abs(events[821].frequencyHz()
	        - sstv::analog::robot36ComponentFrequency(90)) < 1.0e-10
	    && std::abs(events[116'172].frequencyHz()
	        - sstv::analog::robot36ComponentFrequency(128)) < 1.0e-10,
	    "Robot 36 selected luma or colour-difference scan tones are wrong");
	constexpr std::array<std::pair<std::uint32_t, std::uint64_t>, 5> frameCounts{{
		{8'000, 295'280}, {11'025, 406'932}, {44'100, 1'627'731},
		{48'000, 1'771'680}, {96'000, 3'543'360},
	}};
	for (const auto& [rate, expected] : frameCounts) {
		require(sstv::core::sampleCount(expectedDuration, rate) == expected,
		    "Robot 36 frame count differs from the independent vector");
	}
	const Duration lineDuration(3, 20);
	sstv::core::SampleBoundaryScheduler repeated(11'025);
	for (std::uint64_t line = 1; line <= 100'000; ++line) {
		require(repeated.advance(lineDuration)
		    == sstv::core::sampleCount(lineDuration * line, 11'025),
		    "Robot 36 repeated line scheduling accumulated drift");
	}
	require(sstv::analog::robot36ComponentFrequency(0) == 1'500.0
	    && sstv::analog::robot36ComponentFrequency(255) == 2'300.0
	    && std::abs(sstv::analog::robot36ComponentFrequency(128)
	        - 1'901.5686274509803) < 1.0e-10,
	    "Robot 36 component-frequency mapping is wrong");
	requireThrows<std::invalid_argument>([] {
		const auto wrong = sstv::core::makeDiagnosticPattern(320, 256);
		(void)sstv::analog::encodeRobot36(wrong.view(), 0.8F);
	}, "Robot 36 encoder accepted wrong dimensions");
}

void
testRegistryAndDispatch()
{
	using sstv::analog::OfflineTransmission;
	using sstv::core::ModeCapability;
	const auto* const robot = sstv::core::find_mode("robot-36");
	require(robot != nullptr && robot->width == 320 && robot->height == 240
	    && robot->vis_code == 8
	    && robot->colour_encoding == sstv::core::ColourEncoding::lumaColourDifference
	    && robot->offline_tx_strategy == sstv::core::OfflineTxStrategy::robot36,
	    "Robot 36 registry metadata is wrong");
	require(robot->capabilities.contains(ModeCapability::offlineTestPatternTx)
	    && robot->capabilities.contains(ModeCapability::offlineImageTx)
	    && !robot->capabilities.contains(ModeCapability::liveTx)
	    && !robot->capabilities.contains(ModeCapability::receive),
	    "Robot 36 capabilities are not truthful");
	const auto frame = sstv::core::makeDiagnosticPattern(320, 240);
	auto result = sstv::analog::encodeOfflineTransmission("robot-36",
	    ModeCapability::offlineTestPatternTx, frame.view(), 0.8F);
	require(std::holds_alternative<OfflineTransmission>(result),
	    "Robot 36 central dispatch rejected its advertised capability");
	const auto& transmission = std::get<OfflineTransmission>(result);
	require(eventsEqual(transmission.events,
	    sstv::analog::encodeRobot36(frame.view(), 0.8F))
	    && transmission.duration == sstv::analog::robot36TransmissionDuration(),
	    "Robot 36 central dispatch differs from the direct encoder");
	auto missing = sstv::analog::encodeOfflineTransmission("robot-36",
	    ModeCapability::liveTx, frame.view(), 0.8F);
	require(std::get<sstv::analog::OfflineTxError>(missing).code
	    == sstv::analog::OfflineTxErrorCode::missingCapability,
	    "Robot 36 dispatch accepted live TX");
	const auto wrong = sstv::core::makeDiagnosticPattern(320, 256);
	auto wrongResult = sstv::analog::encodeOfflineTransmission("robot-36",
	    ModeCapability::offlineImageTx, wrong.view(), 0.8F);
	require(std::get<sstv::analog::OfflineTxError>(wrongResult).code
	    == sstv::analog::OfflineTxErrorCode::invalidFrameDimensions,
	    "Robot 36 dispatch accepted wrong dimensions");
}

void
testRenderer()
{
	const auto frame = sstv::core::makeDiagnosticPattern(320, 240);
	const auto events = sstv::analog::encodeRobot36(frame.view(), 0.8F);
	const std::span<const sstv::core::ToneEvent> lines(events.data() + 13, 968);
	const std::vector<float> reference = renderEvents(lines, 65'536);
	for (const std::size_t blockSize : {1U, 257U, 4'096U, 65'536U}) {
		const std::vector<float> actual = renderEvents(lines, blockSize);
		require(actual.size() == reference.size(),
		    "Robot 36 renderer block size changed frame count");
		for (std::size_t index = 0; index < actual.size(); ++index) {
			require(std::abs(actual[index] - reference[index]) <= 1.0e-7F,
			    "Robot 36 renderer output changed with block size");
		}
	}
	require(std::abs(reference[432]) > 0.01F,
	    "Robot 36 oscillator phase reset at a tone boundary");
	for (const float sample : reference) {
		require(std::isfinite(sample) && std::abs(sample) <= 0.8F + 1.0e-7F,
		    "Robot 36 renderer produced a non-finite or unbounded sample");
		(void)sstv::offline::floatToPcm16(sample);
	}
}

void
testWavAndCleanup()
{
	const auto frame = sstv::core::makeDiagnosticPattern(320, 240);
	const auto events = sstv::analog::encodeRobot36(frame.view(), 0.8F);
	const std::filesystem::path directory = std::filesystem::temp_directory_path()
	    / "scanline-sstv-m1d-tests";
	std::error_code ignored;
	std::filesystem::remove_all(directory, ignored);
	std::filesystem::create_directories(directory);
	const std::filesystem::path path = directory / "robot-36.wav";
	const auto metadata = sstv::offline::writePcm16WavAtomic(
	    path, events, 8'000, false);
	require(metadata.sampleRate == 8'000 && metadata.frameCount == 295'280
	    && std::filesystem::file_size(path) == 590'604,
	    "Robot 36 WAV metadata or RIFF size is wrong");
	std::array<char, 44> header{};
	std::ifstream input(path, std::ios::binary);
	input.read(header.data(), static_cast<std::streamsize>(header.size()));
	require(input.gcount() == static_cast<std::streamsize>(header.size())
	    && std::string_view(header.data(), 4) == "RIFF"
	    && std::string_view(header.data() + 8, 4) == "WAVE"
	    && std::string_view(header.data() + 36, 4) == "data",
	    "Robot 36 WAV header is invalid");
	requireThrows<std::runtime_error>([&path, &events] {
		(void)sstv::offline::writePcm16WavAtomic(path, events, 8'000, false);
	}, "Robot 36 WAV overwrote an existing destination");
	(void)sstv::offline::writePcm16WavAtomic(path, events, 8'000, true);
	const std::filesystem::path blocked = directory / "blocked";
	std::filesystem::create_directory(blocked);
	requireThrows<std::system_error>([&blocked, &events] {
		(void)sstv::offline::writePcm16WavAtomic(blocked, events, 8'000, true);
	}, "Robot 36 WAV failure path unexpectedly published output");
	for (const auto& entry : std::filesystem::directory_iterator(directory)) {
		require(!entry.path().filename().string().starts_with("blocked.tmp."),
		    "Robot 36 WAV failure abandoned a temporary file");
	}
	std::filesystem::remove_all(directory);
}

void
testFrozenRgbModes()
{
	const auto frame = sstv::core::makeDiagnosticPattern(320, 256);
	const auto martin = sstv::analog::encodeMartinM1(frame.view(), 0.8F);
	const auto scottie = sstv::analog::encodeScottieS1(frame.view(), 0.8F);
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
}

} // namespace

int
main()
{
	testColourValues();
	testSubsampling();
	testColourStorageValidation();
	testDescriptorAndVis();
	testEncodingAndTiming();
	testRegistryAndDispatch();
	testRenderer();
	testWavAndCleanup();
	testFrozenRgbModes();
	return 0;
}
