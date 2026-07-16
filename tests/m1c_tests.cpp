// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m1c_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/martin_m1.hpp>
#include <sstv/analog/offline_tx.hpp>
#include <sstv/analog/scottie_s1.hpp>
#include <sstv/analog/vis.hpp>
#include <sstv/core/rgb8_frame.hpp>
#include <sstv/core/timing.hpp>
#include <sstv/dsp/tone_renderer.hpp>
#include <sstv/offline/wav_writer.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
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

void
testDescriptor()
{
	using sstv::analog::FixedToneSegment;
	using sstv::analog::RgbChannel;
	using sstv::analog::RgbScanSegment;
	using sstv::core::Duration;
	const auto& descriptor = sstv::analog::scottieS1Descriptor();
	require(descriptor.id == "scottie-s1" && descriptor.displayName == "Scottie S1"
	    && descriptor.visCode == 60, "Scottie S1 identity is wrong");
	require(descriptor.width == 320 && descriptor.height == 256,
	    "Scottie S1 dimensions are wrong");
	require(descriptor.preImageSchedule.empty()
	    && descriptor.firstLineSchedule.size() == 7U
	    && descriptor.firstLineSchedule.data() == descriptor.lineSchedule.data(),
	    "Scottie S1 first-line schedule is wrong");
	const auto& gap1 = std::get<FixedToneSegment>(descriptor.lineSchedule[0]);
	const auto& green = std::get<RgbScanSegment>(descriptor.lineSchedule[1]);
	const auto& gap2 = std::get<FixedToneSegment>(descriptor.lineSchedule[2]);
	const auto& blue = std::get<RgbScanSegment>(descriptor.lineSchedule[3]);
	const auto& sync = std::get<FixedToneSegment>(descriptor.lineSchedule[4]);
	const auto& gap3 = std::get<FixedToneSegment>(descriptor.lineSchedule[5]);
	const auto& red = std::get<RgbScanSegment>(descriptor.lineSchedule[6]);
	require(gap1.duration == Duration::fromMicroseconds(1'500)
	    && gap2.duration == Duration::fromMicroseconds(1'500)
	    && gap3.duration == Duration::fromMicroseconds(1'500)
	    && gap1.frequencyHz == 1'500.0 && gap2.frequencyHz == 1'500.0
	    && gap3.frequencyHz == 1'500.0,
	    "Scottie S1 gap schedule is wrong");
	require(green.channel == RgbChannel::green && blue.channel == RgbChannel::blue
	    && red.channel == RgbChannel::red
	    && green.duration == Duration::fromMicroseconds(138'240)
	    && blue.duration == Duration::fromMicroseconds(138'240)
	    && red.duration == Duration::fromMicroseconds(138'240),
	    "Scottie S1 channel order or scan timing is wrong");
	require(sync.duration == Duration::fromMicroseconds(9'000)
	    && sync.frequencyHz == 1'200.0,
	    "Scottie S1 sync timing or placement is wrong");
	require(descriptor.blackHz == 1'500.0 && descriptor.whiteHz == 2'300.0,
	    "Scottie S1 video frequency endpoints are wrong");
}

void
testEncoding()
{
	using sstv::core::Duration;
	const sstv::core::Rgb8Frame frame = sstv::core::makeDiagnosticPattern(320, 256);
	const std::vector<sstv::core::ToneEvent> events
	    = sstv::analog::encodeScottieS1(frame.view(), 0.8F);
	require(events.size() == 246'797U, "Scottie S1 event count differs from vector");
	const Duration expectedDuration(1'381'679, 12'500);
	require(sstv::analog::scottieS1TransmissionDuration() == expectedDuration,
	    "Scottie S1 complete duration differs from vector");
	Duration eventDuration(0, 1);
	for (const sstv::core::ToneEvent& event : events) {
		eventDuration = eventDuration + event.duration();
	}
	require(eventDuration == expectedDuration,
	    "Scottie S1 event sum differs from descriptor duration");
	constexpr std::array<std::pair<std::size_t, std::uint64_t>, 16> boundaries{{
		{13, 43'752}, {14, 43'772}, {333, 50'387}, {334, 50'459},
		{335, 50'480}, {654, 57'095}, {655, 57'527}, {656, 57'599},
		{657, 57'619}, {976, 64'234}, {977, 64'306}, {978, 64'327},
		{1'619, 78'081}, {245'833, 5'285'164},
		{246'475, 5'298'939}, {246'796, 5'305'647},
	}};
	sstv::core::SampleBoundaryScheduler scheduler(48'000);
	std::size_t selected = 0;
	for (std::size_t index = 0; index < events.size(); ++index) {
		const std::uint64_t boundary = scheduler.advance(events[index].duration());
		if (selected < boundaries.size() && index == boundaries[selected].first) {
			require(boundary == boundaries[selected].second,
			    "Scottie S1 boundary differs from independent vector");
			++selected;
		}
	}
	require(selected == boundaries.size(), "Scottie S1 boundary vector was incomplete");
	require(events[13].frequencyHz() == 1'500.0
	    && events[14].frequencyHz() == 1'500.0
	    && events[333].frequencyHz() == 2'300.0
	    && events[334].frequencyHz() == 1'500.0
	    && events[335].frequencyHz() == 1'500.0
	    && events[655].frequencyHz() == 1'200.0
	    && events[656].frequencyHz() == 1'500.0
	    && events[657].frequencyHz() == 2'300.0,
	    "Scottie S1 first-line order or tones differ from vector");
	require(events[977].frequencyHz() == 1'500.0
	    && events[1'619].frequencyHz() == 1'200.0
	    && events[245'833].frequencyHz() == 1'500.0
	    && events[246'475].frequencyHz() == 1'200.0
	    && events[246'796].frequencyHz() == 2'300.0,
	    "Scottie S1 second or final line differs from vector");
	requireThrows<std::invalid_argument>([] {
		const auto wrong = sstv::core::makeDiagnosticPattern(160, 256);
		(void)sstv::analog::encodeScottieS1(wrong.view(), 0.8F);
	}, "Scottie S1 accepted wrong frame dimensions");
}

void
testRegistryAndDispatch()
{
	using sstv::analog::OfflineTransmission;
	using sstv::analog::OfflineTxError;
	using sstv::analog::OfflineTxErrorCode;
	using sstv::core::ModeCapability;
	const auto* const martin = sstv::core::find_mode("martin-m1");
	const auto* const scottie = sstv::core::find_mode("scottie-s1");
	require(martin != nullptr && scottie != nullptr,
	    "evidence-approved offline modes are missing from the registry");
	for (const sstv::core::ModeDescriptor* const mode : {martin, scottie}) {
		require(mode->capabilities.contains(ModeCapability::offlineTestPatternTx)
		    && mode->capabilities.contains(ModeCapability::offlineImageTx)
		    && !mode->capabilities.contains(ModeCapability::liveTx)
		    && !mode->capabilities.contains(ModeCapability::receive),
		    "offline mode capabilities are not truthful");
		const sstv::core::Rgb8Frame frame
		    = sstv::core::makeDiagnosticPattern(mode->width, mode->height);
		for (const ModeCapability capability : {
		    ModeCapability::offlineTestPatternTx, ModeCapability::offlineImageTx}) {
			auto result = sstv::analog::encodeOfflineTransmission(
			    mode->id, capability, frame.view(), 0.8F);
			require(std::holds_alternative<OfflineTransmission>(result),
			    "advertised offline mode lacks a dispatch strategy");
		}
	}
	const sstv::core::Rgb8Frame frame = sstv::core::makeDiagnosticPattern(320, 256);
	auto martinResult = sstv::analog::encodeOfflineTransmission("martin-m1",
	    ModeCapability::offlineTestPatternTx, frame.view(), 0.8F);
	auto scottieResult = sstv::analog::encodeOfflineTransmission("scottie-s1",
	    ModeCapability::offlineTestPatternTx, frame.view(), 0.8F);
	const auto& martinTransmission = std::get<OfflineTransmission>(martinResult);
	const auto& scottieTransmission = std::get<OfflineTransmission>(scottieResult);
	require(eventsEqual(martinTransmission.events,
	    sstv::analog::encodeMartinM1(frame.view(), 0.8F))
	    && martinTransmission.duration == sstv::analog::martinM1TransmissionDuration(),
	    "Martin M1 dispatch differs from its accepted direct encoder");
	require(eventsEqual(scottieTransmission.events,
	    sstv::analog::encodeScottieS1(frame.view(), 0.8F))
	    && scottieTransmission.duration == sstv::analog::scottieS1TransmissionDuration(),
	    "Scottie S1 dispatch differs from its direct encoder");
	const sstv::core::Rgb8Frame wrong = sstv::core::makeDiagnosticPattern(160, 256);
	auto wrongResult = sstv::analog::encodeOfflineTransmission("scottie-s1",
	    ModeCapability::offlineImageTx, wrong.view(), 0.8F);
	require(std::get<OfflineTxError>(wrongResult).code
	    == OfflineTxErrorCode::invalidFrameDimensions,
	    "dispatch accepted wrong Scottie S1 dimensions");
	auto missingResult = sstv::analog::encodeOfflineTransmission("scottie-s1",
	    ModeCapability::liveTx, frame.view(), 0.8F);
	require(std::get<OfflineTxError>(missingResult).code
	    == OfflineTxErrorCode::missingCapability,
	    "dispatch accepted a capability the mode does not advertise");
	auto unknownResult = sstv::analog::encodeOfflineTransmission("unknown",
	    ModeCapability::offlineTestPatternTx, frame.view(), 0.8F);
	require(std::get<OfflineTxError>(unknownResult).code
	    == OfflineTxErrorCode::unknownMode,
	    "dispatch accepted an unknown mode");
}

void
testRenderer()
{
	const sstv::core::Rgb8Frame frame = sstv::core::makeDiagnosticPattern(320, 256);
	const std::vector<sstv::core::ToneEvent> events
	    = sstv::analog::encodeScottieS1(frame.view(), 0.8F);
	const std::span<const sstv::core::ToneEvent> firstLine(events.data() + 13, 964);
	const std::vector<float> reference = renderEvents(firstLine, 65'536);
	for (const std::size_t blockSize : {1U, 257U, 4'096U, 65'536U}) {
		const std::vector<float> actual = renderEvents(firstLine, blockSize);
		require(actual.size() == reference.size(),
		    "Scottie S1 renderer block size changed frame count");
		for (std::size_t index = 0; index < actual.size(); ++index) {
			require(std::abs(actual[index] - reference[index]) <= 1.0e-7F,
			    "Scottie S1 renderer output changed with block size");
		}
	}
	require(std::abs(reference[72]) > 0.01F,
	    "Scottie S1 oscillator phase reset at the first tone boundary");
	for (const float sample : reference) {
		require(std::isfinite(sample), "Scottie S1 renderer produced a non-finite sample");
		require(std::abs(sample) <= 0.8F + 1.0e-7F,
		    "Scottie S1 renderer exceeded explicit amplitude");
		(void)sstv::offline::floatToPcm16(sample);
	}
}

void
testTimingAndMapping()
{
	using sstv::core::Duration;
	const Duration duration = sstv::analog::scottieS1TransmissionDuration();
	constexpr std::array<std::pair<std::uint32_t, std::uint64_t>, 5> expected{{
		{8'000, 884'274}, {11'025, 1'218'640}, {44'100, 4'874'563},
		{48'000, 5'305'647}, {96'000, 10'611'294},
	}};
	for (const auto& [rate, frames] : expected) {
		require(sstv::core::sampleCount(duration, rate) == frames,
		    "Scottie S1 frame count differs from independent vector");
	}
	const Duration lineDuration(21'411, 50'000);
	sstv::core::SampleBoundaryScheduler scheduler(48'000);
	for (std::uint64_t index = 1; index <= 100'000; ++index) {
		const std::uint64_t boundary = scheduler.advance(lineDuration);
		require(boundary == sstv::core::sampleCount(lineDuration * index, 48'000),
		    "Scottie S1 repeated schedule accumulated drift");
	}
	require(sstv::analog::scottieS1PixelFrequency(0) == 1'500.0
	    && sstv::analog::scottieS1PixelFrequency(255) == 2'300.0,
	    "Scottie S1 black or white mapping is wrong");
	require(std::abs(sstv::analog::scottieS1PixelFrequency(128)
	    - 1'901.5686274509803) < 1.0e-10,
	    "Scottie S1 gradient mapping is wrong");
}

void
testVis()
{
	constexpr std::array<double, 13> expected{
		1'900, 1'200, 1'900, 1'200, 1'300, 1'300, 1'100,
		1'100, 1'100, 1'100, 1'300, 1'300, 1'200,
	};
	const sstv::analog::VisBits bits = sstv::analog::makeVisBits(60);
	constexpr std::array<bool, 8> expectedBits{
		false, false, true, true, true, true, false, false,
	};
	require(bits == expectedBits, "Scottie S1 VIS bits or parity differ from vector");
	const sstv::analog::VisHeader header = sstv::analog::makeVisHeader(60, 0.8F);
	for (std::size_t index = 0; index < header.size(); ++index) {
		require(header[index].frequencyHz() == expected[index],
		    "Scottie S1 VIS framing differs from vector");
	}
}

void
testWav()
{
	const sstv::core::Rgb8Frame frame = sstv::core::makeDiagnosticPattern(320, 256);
	const std::vector<sstv::core::ToneEvent> events
	    = sstv::analog::encodeScottieS1(frame.view(), 0.8F);
	const std::filesystem::path path = std::filesystem::temp_directory_path()
	    / "scanline-sstv-m1c-test.wav";
	std::error_code ignored;
	std::filesystem::remove(path, ignored);
	const sstv::offline::WavMetadata metadata
	    = sstv::offline::writePcm16WavAtomic(path, events, 8'000, false);
	require(metadata.sampleRate == 8'000 && metadata.frameCount == 884'274,
	    "Scottie S1 WAV metadata differs from vector");
	require(std::filesystem::file_size(path) == 1'768'592,
	    "Scottie S1 WAV file size is wrong");
	std::filesystem::remove(path);
}

} // namespace

int
main()
{
	testDescriptor();
	testEncoding();
	testRegistryAndDispatch();
	testRenderer();
	testTimingAndMapping();
	testVis();
	testWav();
	return 0;
}
