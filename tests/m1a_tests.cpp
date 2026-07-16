// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m1a_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/martin_m1.hpp>
#include <sstv/analog/vis.hpp>
#include <sstv/core/rgb8_frame.hpp>
#include <sstv/core/timing.hpp>
#include <sstv/dsp/tone_renderer.hpp>
#include <sstv/offline/wav_writer.hpp>

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
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

void
mixHash(std::uint64_t& hash, std::uint64_t value)
{
	for (unsigned int index = 0; index < 8U; ++index) {
		hash ^= value & 0xffU;
		hash *= 1'099'511'628'211ULL;
		value >>= 8U;
	}
}

[[nodiscard]] std::uint32_t
readU32(const std::array<std::uint8_t, 44>& bytes, const std::size_t offset)
{
	return static_cast<std::uint32_t>(bytes[offset])
	    | static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U
	    | static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U
	    | static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U;
}

[[nodiscard]] std::uint16_t
readU16(const std::array<std::uint8_t, 44>& bytes, const std::size_t offset)
{
	return static_cast<std::uint16_t>(bytes[offset])
	    | static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U;
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
		result.insert(result.end(), block.begin(), block.begin()
		    + static_cast<std::ptrdiff_t>(count));
	}
	return result;
}

void
testTiming()
{
	using sstv::core::Duration;
	require(Duration(2, 4) == Duration(1, 2), "duration normalization failed");
	require(Duration(0, 99) == Duration(0, 1), "zero normalization failed");
	requireThrows<std::invalid_argument>([] { (void)Duration(1, 0); },
	    "zero denominator accepted");
	requireThrows<std::invalid_argument>([] { (void)(Duration(1, 2) / 0); },
	    "zero divisor accepted");
	requireThrows<std::invalid_argument>([] {
		(void)sstv::core::SampleBoundaryScheduler(0);
	}, "zero sample rate accepted");
	requireThrows<std::overflow_error>([] {
		(void)(Duration(std::numeric_limits<std::uint64_t>::max(), 1)
		    + Duration(1, 1));
	}, "duration addition overflow was not detected");
	requireThrows<std::overflow_error>([] {
		(void)(Duration(std::numeric_limits<std::uint64_t>::max(), 1) * 2);
	}, "duration multiplication overflow was not detected");
	requireThrows<std::overflow_error>([] {
		(void)sstv::core::sampleCount(
		    Duration(std::numeric_limits<std::uint64_t>::max(), 1), 48'000);
	}, "sample-count overflow was not detected");
	constexpr std::array<std::pair<std::uint32_t, std::uint64_t>, 5> expected{{
		{8'000, 921'601}, {11'025, 1'270'081}, {44'100, 5'080'327},
		{48'000, 5'529'608}, {96'000, 11'059'216},
	}};
	for (const auto& [rate, frames] : expected) {
		require(sstv::core::sampleCount(
		    sstv::analog::martinM1TransmissionDuration(), rate) == frames,
		    "full transmission boundary differs from external vector");
	}
	sstv::core::SampleBoundaryScheduler scheduler(48'000);
	const Duration pixel = Duration::fromMicroseconds(146'432) / 320;
	for (std::uint64_t index = 1; index <= 100'000; ++index) {
		const std::uint64_t boundary = scheduler.advance(pixel);
		require(boundary == sstv::core::sampleCount(pixel * index, 48'000),
		    "repeated schedule accumulated drift");
	}
}

void
testRenderer()
{
	const std::array<sstv::core::ToneEvent, 3> events{{
		{sstv::core::Duration(11, 10'000), 1'000.0, 0.8F},
		{sstv::core::Duration(7, 4'000), 1'700.0, 0.4F},
		{sstv::core::Duration(1, 800), 2'300.0, 1.0F},
	}};
	const std::vector<float> reference = renderEvents(events, 4'096);
	for (const std::size_t blockSize : {1U, 7U, 256U, 4'096U}) {
		const std::vector<float> actual = renderEvents(events, blockSize);
		require(actual.size() == reference.size(), "block size changed frame count");
		for (std::size_t index = 0; index < actual.size(); ++index) {
			require(std::abs(actual[index] - reference[index]) <= 1.0e-7F,
			    "block size changed renderer output");
		}
	}
	require(std::abs(reference[52]) > 0.01F, "phase reset at tone boundary");
	for (const float sample : reference) {
		require(std::isfinite(sample), "renderer produced non-finite sample");
		require(std::abs(sample) <= 1.0F, "renderer exceeded amplitude");
	}
}

void
testVis()
{
	for (std::uint16_t code = 0; code <= 0x7FU; ++code) {
		const sstv::analog::VisBits bits
		    = sstv::analog::makeVisBits(static_cast<std::uint8_t>(code));
		unsigned int ones = 0;
		for (std::size_t index = 0; index < 7U; ++index) {
			require(bits[index] == ((code & (1U << index)) != 0U),
			    "VIS data is not LSB-first");
			ones += bits[index] ? 1U : 0U;
		}
		ones += bits[7] ? 1U : 0U;
		require(ones % 2U == 0U, "VIS parity is not even");
	}
	requireThrows<std::invalid_argument>([] {
		(void)sstv::analog::makeVisBits(0x80U);
	}, "out-of-range VIS code accepted");
	constexpr std::array<double, 13> expected{
		1'900, 1'200, 1'900, 1'200, 1'300, 1'300, 1'100,
		1'100, 1'300, 1'100, 1'300, 1'100, 1'200,
	};
	const sstv::analog::VisHeader header = sstv::analog::makeVisHeader(44, 0.8F);
	const std::array<sstv::core::Duration, 13> durations{
		sstv::core::Duration(300, 1'000), sstv::core::Duration(10, 1'000),
		sstv::core::Duration(300, 1'000), sstv::core::Duration(30, 1'000),
		sstv::core::Duration(30, 1'000), sstv::core::Duration(30, 1'000),
		sstv::core::Duration(30, 1'000), sstv::core::Duration(30, 1'000),
		sstv::core::Duration(30, 1'000), sstv::core::Duration(30, 1'000),
		sstv::core::Duration(30, 1'000), sstv::core::Duration(30, 1'000),
		sstv::core::Duration(30, 1'000),
	};
	for (std::size_t index = 0; index < header.size(); ++index) {
		require(header[index].frequencyHz() == expected[index],
		    "Martin M1 VIS vector differs from external fixture");
		require(header[index].duration() == durations[index],
		    "Martin M1 VIS duration differs from external fixture");
	}
}

void
testMartinM1()
{
	using sstv::core::Duration;
	const auto& descriptor = sstv::analog::martinM1Descriptor();
	require(descriptor.id == "martin-m1" && descriptor.visCode == 44,
	    "Martin M1 identity is wrong");
	require(descriptor.width == 320 && descriptor.height == 256,
	    "Martin M1 dimensions are wrong");
	require(descriptor.preImageSchedule.empty()
	    && descriptor.firstLineSchedule.size() == 8U
	    && descriptor.firstLineSchedule.data() == descriptor.lineSchedule.data(),
	    "Martin M1 first-line schedule is wrong");
	const auto& sync = std::get<sstv::analog::FixedToneSegment>(
	    descriptor.lineSchedule[0]);
	const auto& porch = std::get<sstv::analog::FixedToneSegment>(
	    descriptor.lineSchedule[1]);
	const auto& green = std::get<sstv::analog::RgbScanSegment>(
	    descriptor.lineSchedule[2]);
	const auto& separator1 = std::get<sstv::analog::FixedToneSegment>(
	    descriptor.lineSchedule[3]);
	const auto& blue = std::get<sstv::analog::RgbScanSegment>(
	    descriptor.lineSchedule[4]);
	const auto& separator2 = std::get<sstv::analog::FixedToneSegment>(
	    descriptor.lineSchedule[5]);
	const auto& red = std::get<sstv::analog::RgbScanSegment>(
	    descriptor.lineSchedule[6]);
	const auto& separator3 = std::get<sstv::analog::FixedToneSegment>(
	    descriptor.lineSchedule[7]);
	require(sync.duration == Duration::fromMicroseconds(4'862)
	    && sync.frequencyHz == 1'200.0
	    && porch.duration == Duration::fromMicroseconds(572)
	    && porch.frequencyHz == 1'500.0
	    && separator1.duration == Duration::fromMicroseconds(572)
	    && separator2.duration == Duration::fromMicroseconds(572)
	    && separator3.duration == Duration::fromMicroseconds(572)
	    && green.channel == sstv::analog::RgbChannel::green
	    && blue.channel == sstv::analog::RgbChannel::blue
	    && red.channel == sstv::analog::RgbChannel::red
	    && green.duration == Duration::fromMicroseconds(146'432)
	    && blue.duration == Duration::fromMicroseconds(146'432)
	    && red.duration == Duration::fromMicroseconds(146'432),
	    "Martin M1 schedule, channel order, or timings are wrong");
	require(sstv::analog::martinM1TransmissionDuration()
	    == Duration(115'200'176, 1'000'000), "nominal duration is wrong");
	require(sstv::analog::martinM1PixelFrequency(0) == 1'500.0
	    && sstv::analog::martinM1PixelFrequency(255) == 2'300.0,
	    "black or white tone mapping is wrong");
	require(std::abs(sstv::analog::martinM1PixelFrequency(128)
	    - 1'901.5686274509803) < 1.0e-10, "gradient mapping is wrong");
	const sstv::core::Rgb8Frame frame = sstv::core::makeDiagnosticPattern(320, 256);
	const sstv::core::Rgb8View view = frame.view();
	require(view.pixel(0, 0).red == 255 && view.pixel(319, 0).green == 255
	    && view.pixel(0, 255).blue == 255
	    && view.pixel(319, 255).red == 255, "diagnostic corners are wrong");
	require(view.pixel(160, 80).red == view.pixel(160, 80).green
	    && view.pixel(160, 80).green == view.pixel(160, 80).blue,
	    "diagnostic grayscale gradient is wrong");
	require(view.pixel(4, 32).red == 0 && view.pixel(4, 32).blue == 0,
	    "diagnostic line marker is wrong");
	const std::vector<sstv::core::ToneEvent> events
	    = sstv::analog::encodeMartinM1(view, 0.8F);
	require(events.size() == 247'053U, "event count differs from vector");
	std::uint64_t eventHash = 1'469'598'103'934'665'603ULL;
	for (const sstv::core::ToneEvent& event : events) {
		mixHash(eventHash, event.duration().numerator());
		mixHash(eventHash, event.duration().denominator());
		mixHash(eventHash, std::bit_cast<std::uint64_t>(event.frequencyHz()));
		mixHash(eventHash, std::bit_cast<std::uint32_t>(event.amplitude()));
	}
	require(eventHash == 10'679'166'461'901'389'520ULL,
	    "Martin M1 ordered event stream changed during the shared-encoder refactor");
	constexpr std::array<std::pair<std::size_t, std::uint64_t>, 12> boundaries{{
		{13, 43'913}, {14, 43'940}, {15, 43'962}, {334, 50'969},
		{335, 50'997}, {336, 51'018}, {655, 58'025}, {656, 58'053},
		{657, 58'075}, {976, 65'081}, {977, 65'109}, {978, 65'342},
	}};
	sstv::core::SampleBoundaryScheduler scheduler(48'000);
	std::size_t selected = 0;
	for (std::size_t index = 0; index <= boundaries.back().first; ++index) {
		const std::uint64_t boundary = scheduler.advance(events[index].duration());
		if (selected < boundaries.size() && index == boundaries[selected].first) {
			require(boundary == boundaries[selected].second,
			    "scanline boundary differs from external vector");
			++selected;
		}
	}
	require(events[13].frequencyHz() == 1'200.0
	    && events[14].frequencyHz() == 1'500.0
	    && events[15].frequencyHz() == 1'500.0
	    && events[334].frequencyHz() == 2'300.0,
	    "first scanline ordering or tones are wrong");
}

void
testWav()
{
	require(sstv::offline::floatToPcm16(-1.0F) == -32'768
	    && sstv::offline::floatToPcm16(1.0F) == 32'767
	    && sstv::offline::floatToPcm16(-2.0F) == -32'768
	    && sstv::offline::floatToPcm16(2.0F) == 32'767
	    && sstv::offline::floatToPcm16(0.5F) == 16'384,
	    "PCM endpoints or rounding are wrong");
	requireThrows<std::invalid_argument>([] {
		(void)sstv::offline::floatToPcm16(
		    std::numeric_limits<float>::quiet_NaN());
	}, "NaN PCM input accepted");
	const std::array<sstv::core::ToneEvent, 1> events{{
		{sstv::core::Duration(1, 100), 1'000.0, 0.8F},
	}};
	const std::filesystem::path path = std::filesystem::temp_directory_path()
	    / "scanline-sstv-m1a-test.wav";
	std::error_code ignored;
	std::filesystem::remove(path, ignored);
	const auto metadata
	    = sstv::offline::writePcm16WavAtomic(path, events, 48'000, false);
	require(metadata.sampleRate == 48'000 && metadata.frameCount == 480,
	    "WAV metadata is wrong");
	std::ifstream stream(path, std::ios::binary);
	std::array<std::uint8_t, 44> header{};
	stream.read(reinterpret_cast<char*>(header.data()),
	    static_cast<std::streamsize>(header.size()));
	require(stream.gcount() == 44, "WAV header is truncated");
	require(std::string(reinterpret_cast<const char*>(header.data()), 4) == "RIFF"
	    && std::string(reinterpret_cast<const char*>(header.data() + 8), 4) == "WAVE",
	    "RIFF/WAVE tags are wrong");
	require(readU32(header, 4) == 996 && readU32(header, 40) == 960
	    && readU32(header, 24) == 48'000 && readU16(header, 20) == 1
	    && readU16(header, 22) == 1 && readU16(header, 34) == 16,
	    "WAV header values are wrong");
	require(std::filesystem::file_size(path) == 1'004,
	    "WAV finalized size is wrong");
	requireThrows<std::runtime_error>([&] {
		(void)sstv::offline::writePcm16WavAtomic(path, events, 48'000, false);
	}, "WAV overwrite refusal failed");
	(void)sstv::offline::writePcm16WavAtomic(path, events, 48'000, true);
	std::filesystem::remove(path);
	const std::array<sstv::core::ToneEvent, 1> huge{{
		{sstv::core::Duration(100'000, 1), 1'000.0, 0.8F},
	}};
	requireThrows<std::overflow_error>([&] {
		(void)sstv::offline::writePcm16WavAtomic(path, huge, 48'000, false);
	}, "RIFF size overflow was not detected");
	require(!std::filesystem::exists(path), "overflow left a destination file");
}

} // namespace

int
main()
{
	testTiming();
	testRenderer();
	testVis();
	testMartinM1();
	testWav();
	return 0;
}
