// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m1f_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/fsk_id.hpp>
#include <sstv/analog/offline_tx.hpp>
#include <sstv/core/mode.hpp>
#include <sstv/core/rgb8_frame.hpp>
#include <sstv/core/timing.hpp>
#include <sstv/dsp/tone_renderer.hpp>
#include <sstv/offline/wav_inspector.hpp>
#include <sstv/offline/wav_writer.hpp>

#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace {

constexpr std::array<std::uint32_t, 11> supportedRates{
	8'000, 11'025, 16'000, 22'050, 32'000, 44'100,
	48'000, 88'200, 96'000, 176'400, 192'000,
};

void
require(const bool condition, const std::string& message)
{
	if (!condition) {
		throw std::runtime_error(message);
	}
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

[[nodiscard]] const sstv::analog::FskIdentifier&
requireIdentifier(const sstv::analog::FskIdentifierResult& result)
{
	const auto* identifier = std::get_if<sstv::analog::FskIdentifier>(&result);
	require(identifier != nullptr, "valid FSK identifier was rejected");
	return *identifier;
}

[[nodiscard]] std::uint8_t
wireCode(const char character)
{
	return static_cast<std::uint8_t>(
	    static_cast<unsigned char>(character) - 0x20U);
}

void
verifyCode(const std::span<const sstv::core::ToneEvent> events,
	const std::size_t offset, const std::uint8_t code)
{
	for (unsigned int bit = 0; bit < 6U; ++bit) {
		const double expected = (code & (1U << bit)) != 0U ? 1'900.0 : 2'100.0;
		require(events[offset + bit].duration()
		        == sstv::core::Duration::fromMicroseconds(22'000)
		    && events[offset + bit].frequencyHz() == expected,
		    "FSK six-bit LSB-first event differs from the frozen vector");
	}
}

void
testFskValidationAndVector()
{
	const auto normalized = sstv::analog::validateFskIdentifier("m6vpn-1");
	const sstv::analog::FskIdentifier& identifier = requireIdentifier(normalized);
	require(identifier.value() == "M6VPN-1", "FSK ASCII case normalization is wrong");
	const auto suffix = sstv::analog::generateFskIdSuffix(identifier, 0.8F);
	require(suffix.events.size() == 64U
	    && suffix.duration == sstv::core::Duration(921, 500),
	    "FSK reference event count or duration is wrong");
	require(suffix.events[0].duration()
	        == sstv::core::Duration::fromMicroseconds(300'000)
	    && suffix.events[0].frequencyHz() == 1'500.0
	    && suffix.events[1].duration()
	        == sstv::core::Duration::fromMicroseconds(100'000)
	    && suffix.events[1].frequencyHz() == 2'100.0
	    && suffix.events[2].duration()
	        == sstv::core::Duration::fromMicroseconds(22'000)
	    && suffix.events[2].frequencyHz() == 1'900.0,
	    "FSK physical leader/start sequence differs from the frozen vector");
	verifyCode(suffix.events, 3U, 0x2aU);
	std::uint8_t checksum = 0U;
	std::size_t offset = 9U;
	for (const char character : identifier.value()) {
		const std::uint8_t code = wireCode(character);
		verifyCode(suffix.events, offset, code);
		checksum = static_cast<std::uint8_t>(checksum ^ code);
		offset += 6U;
	}
	require(checksum == 0x0fU, "FSK reference checksum is wrong");
	verifyCode(suffix.events, offset, 0x01U);
	verifyCode(suffix.events, offset + 6U, checksum);
	require(suffix.events.back().duration()
	        == sstv::core::Duration::fromMicroseconds(100'000)
	    && suffix.events.back().frequencyHz() == 1'900.0,
	    "FSK trailing mark differs from the frozen vector");
	for (const std::uint32_t rate : supportedRates) {
		const std::uint64_t expected = (921U * rate) / 500U;
		require(sstv::core::sampleCount(suffix.duration, rate) == expected,
		    "FSK reference frame count is wrong");
	}
	const std::array<std::string_view, 7> invalid{
		"", "         ", "0123456789", "CALL\n", "A`", "A{", "\x7f",
	};
	for (const std::string_view value : invalid) {
		require(std::holds_alternative<sstv::analog::FskIdError>(
		        sstv::analog::validateFskIdentifier(value)),
		    "invalid FSK identifier was accepted");
	}
	const std::string embeddedNul("AB\0CD", 5U);
	require(std::holds_alternative<sstv::analog::FskIdError>(
	        sstv::analog::validateFskIdentifier(embeddedNul)),
	    "embedded NUL was accepted in an FSK identifier");
	require(requireIdentifier(sstv::analog::validateFskIdentifier("ABCDEFGHI"))
	        .value().size() == sstv::analog::maximumFskIdentifierLength,
	    "boundary-length FSK identifier was rejected");
}

[[nodiscard]] sstv::core::Rgb8Frame
makeModeFrame(const sstv::core::ModeDescriptor& mode)
{
	return sstv::core::makeDiagnosticPattern(mode.width, mode.height);
}

void
testCompositionAndFrozenPrefixes()
{
	const auto identifierResult = sstv::analog::validateFskIdentifier("M6VPN-1");
	const sstv::analog::FskIdentifier identifier = requireIdentifier(identifierResult);
	const auto suffix = sstv::analog::generateFskIdSuffix(identifier, 0.8F);
	constexpr std::array<std::size_t, 4> frozenEventCounts{
		247'053U, 246'797U, 116'173U, 635'389U,
	};
	const std::array<sstv::core::Duration, 4> frozenDurations{
		sstv::core::Duration(7'200'011, 62'500),
		sstv::core::Duration(1'381'679, 12'500),
		sstv::core::Duration(3'691, 100),
		sstv::core::Duration(1'587'663, 12'500),
	};
	std::size_t modeIndex = 0U;
	for (const sstv::core::ModeDescriptor& mode : sstv::core::built_in_modes()) {
		require(mode.capabilities.contains(
		        sstv::core::ModeCapability::offlineFskIdTx),
		    "offline analogue mode lacks truthful optional FSK capability");
		const auto frame = makeModeFrame(mode);
		const auto compatibility = sstv::analog::encodeOfflineTransmission(mode.id,
		    sstv::core::ModeCapability::offlineTestPatternTx, frame.view(), 0.8F);
		const auto disabled = sstv::analog::encodeOfflineTransmission(mode.id,
		    sstv::core::ModeCapability::offlineTestPatternTx, frame.view(),
		    sstv::analog::OfflineTransmissionOptions{0.8F, std::nullopt});
		const auto* compatibilityTx
		    = std::get_if<sstv::analog::OfflineTransmission>(&compatibility);
		const auto* disabledTx
		    = std::get_if<sstv::analog::OfflineTransmission>(&disabled);
		require(compatibilityTx != nullptr && disabledTx != nullptr
		    && compatibilityTx->events.size() == frozenEventCounts[modeIndex]
		    && compatibilityTx->duration == frozenDurations[modeIndex]
		    && eventsEqual(compatibilityTx->events, disabledTx->events)
		    && compatibilityTx->duration == disabledTx->duration,
		    "disabled FSK changed a frozen base transmission");
		const auto enabled = sstv::analog::encodeOfflineTransmission(mode.id,
		    sstv::core::ModeCapability::offlineTestPatternTx, frame.view(),
		    sstv::analog::OfflineTransmissionOptions{0.8F, identifier});
		const auto* enabledTx = std::get_if<sstv::analog::OfflineTransmission>(&enabled);
		require(enabledTx != nullptr
		    && enabledTx->events.size() == frozenEventCounts[modeIndex] + 64U
		    && enabledTx->duration == frozenDurations[modeIndex] + suffix.duration
		    && eventsEqual(std::span<const sstv::core::ToneEvent>(enabledTx->events)
		            .first(frozenEventCounts[modeIndex]),
		        compatibilityTx->events)
		    && eventsEqual(std::span<const sstv::core::ToneEvent>(enabledTx->events)
		            .subspan(frozenEventCounts[modeIndex]),
		        suffix.events),
		    "enabled FSK did not preserve the base prefix and exact suffix");
		for (const std::uint32_t rate : supportedRates) {
			require(sstv::core::sampleCount(enabledTx->duration, rate)
			        == sstv::core::sampleCount(
			            frozenDurations[modeIndex] + suffix.duration, rate),
			    "combined transmission frame count is wrong");
		}
		++modeIndex;
	}
}

[[nodiscard]] std::vector<float>
renderEvents(const std::span<const sstv::core::ToneEvent> events,
	const std::size_t blockSize)
{
	sstv::dsp::ToneRenderer renderer(events, 48'000);
	std::vector<float> output;
	output.reserve(static_cast<std::size_t>(renderer.frameCount()));
	std::vector<float> block(blockSize);
	while (!renderer.finished()) {
		const std::size_t count = renderer.render(block);
		output.insert(output.end(), block.begin(),
		    block.begin() + static_cast<std::ptrdiff_t>(count));
	}
	return output;
}

void
testRenderingAcrossBoundary()
{
	const auto identifierResult = sstv::analog::validateFskIdentifier("M6VPN-1");
	const auto suffix = sstv::analog::generateFskIdSuffix(
	    requireIdentifier(identifierResult), 0.8F);
	const std::array<sstv::core::ToneEvent, 2> base{
		sstv::core::ToneEvent(sstv::core::Duration::fromMicroseconds(1'001),
		    2'300.0, 0.8F),
		sstv::core::ToneEvent(sstv::core::Duration::fromMicroseconds(997),
		    1'700.0, 0.8F),
	};
	std::vector<sstv::core::ToneEvent> combined(base.begin(), base.end());
	combined.insert(combined.end(), suffix.events.begin(), suffix.events.end());
	const std::vector<float> single = renderEvents(combined, 1U);
	const std::vector<float> odd = renderEvents(combined, 137U);
	const std::vector<float> large = renderEvents(combined, single.size() + 1U);
	require(single == odd && single == large,
	    "rendering changed with block size across the image/FSK boundary");
	const std::uint64_t boundary = sstv::core::sampleCount(
	    base[0].duration() + base[1].duration(), 48'000);
	require(boundary > 0U && boundary < single.size()
	    && std::isfinite(single[static_cast<std::size_t>(boundary - 1U)])
	    && std::isfinite(single[static_cast<std::size_t>(boundary)]),
	    "renderer did not produce finite continuous-phase boundary samples");
}

void
putU16(std::vector<std::uint8_t>& bytes, const std::uint16_t value)
{
	bytes.push_back(static_cast<std::uint8_t>(value & 0xffU));
	bytes.push_back(static_cast<std::uint8_t>(value >> 8U));
}

void
putU32(std::vector<std::uint8_t>& bytes, const std::uint32_t value)
{
	for (unsigned int shift = 0U; shift < 32U; shift += 8U) {
		bytes.push_back(static_cast<std::uint8_t>((value >> shift) & 0xffU));
	}
}

void
setU32(std::vector<std::uint8_t>& bytes, const std::size_t offset,
	const std::uint32_t value)
{
	for (unsigned int shift = 0U; shift < 32U; shift += 8U) {
		bytes[offset + shift / 8U]
		    = static_cast<std::uint8_t>((value >> shift) & 0xffU);
	}
}

void
putTag(std::vector<std::uint8_t>& bytes, const std::string_view tag)
{
	for (const char character : tag) {
		bytes.push_back(static_cast<std::uint8_t>(character));
	}
}

void
putChunk(std::vector<std::uint8_t>& bytes, const std::string_view tag,
	const std::span<const std::uint8_t> data)
{
	putTag(bytes, tag);
	putU32(bytes, static_cast<std::uint32_t>(data.size()));
	bytes.insert(bytes.end(), data.begin(), data.end());
	if ((data.size() & 1U) != 0U) {
		bytes.push_back(0U);
	}
}

[[nodiscard]] std::vector<std::uint8_t>
makeWave(const std::span<const std::int16_t> samples,
	const std::uint16_t format = 1U, const std::uint16_t channels = 1U,
	const std::uint32_t sampleRate = 8'000U,
	const std::uint16_t bitsPerSample = 16U, const bool includeOddChunk = false)
{
	std::vector<std::uint8_t> bytes;
	putTag(bytes, "RIFF");
	putU32(bytes, 0U);
	putTag(bytes, "WAVE");
	if (includeOddChunk) {
		constexpr std::array<std::uint8_t, 3> unknown{1U, 2U, 3U};
		putChunk(bytes, "JUNK", unknown);
	}
	std::vector<std::uint8_t> formatBytes;
	putU16(formatBytes, format);
	putU16(formatBytes, channels);
	putU32(formatBytes, sampleRate);
	const std::uint16_t alignment
	    = static_cast<std::uint16_t>((channels * bitsPerSample) / 8U);
	putU32(formatBytes, sampleRate * alignment);
	putU16(formatBytes, alignment);
	putU16(formatBytes, bitsPerSample);
	putChunk(bytes, "fmt ", formatBytes);
	std::vector<std::uint8_t> sampleBytes;
	for (const std::int16_t sample : samples) {
		const std::uint16_t raw = static_cast<std::uint16_t>(sample);
		putU16(sampleBytes, raw);
	}
	putChunk(bytes, "data", sampleBytes);
	setU32(bytes, 4U, static_cast<std::uint32_t>(bytes.size() - 8U));
	return bytes;
}

void
writeBytes(const std::filesystem::path& path,
	const std::span<const std::uint8_t> bytes)
{
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	output.write(reinterpret_cast<const char*>(bytes.data()),
	    static_cast<std::streamsize>(bytes.size()));
	require(output.good(), "failed to write generated WAV fixture");
}

[[nodiscard]] bool
filesEqual(const std::filesystem::path& leftPath,
	const std::filesystem::path& rightPath)
{
	std::ifstream left(leftPath, std::ios::binary);
	std::ifstream right(rightPath, std::ios::binary);
	std::array<char, 8'192> leftBytes{};
	std::array<char, 8'192> rightBytes{};
	while (left.good() || right.good()) {
		left.read(leftBytes.data(), static_cast<std::streamsize>(leftBytes.size()));
		right.read(rightBytes.data(), static_cast<std::streamsize>(rightBytes.size()));
		if (left.gcount() != right.gcount()
		    || !std::equal(leftBytes.begin(),
		        leftBytes.begin() + left.gcount(), rightBytes.begin())) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] sstv::offline::WavInspection
requireInspection(const sstv::offline::WavInspectionResult& result)
{
	const auto* inspection = std::get_if<sstv::offline::WavInspection>(&result);
	require(inspection != nullptr, "valid generated WAV was rejected");
	return *inspection;
}

void
requireRejected(const std::filesystem::path& path,
	const std::string& message,
	const sstv::offline::WavInspectionLimits limits
	    = sstv::offline::defaultWavInspectionLimits())
{
	require(std::holds_alternative<sstv::offline::WavInspectError>(
	        sstv::offline::inspectPcm16Wav(path, limits)), message);
}

void
testWavInspection(const std::filesystem::path& directory)
{
	constexpr std::array<std::int16_t, 5> samples{
		std::numeric_limits<std::int16_t>::min(), -1, 0, 1,
		std::numeric_limits<std::int16_t>::max(),
	};
	const std::filesystem::path validPath = directory / "known.wav";
	writeBytes(validPath, makeWave(samples, 1U, 1U, 8'000U, 16U, true));
	const auto inspection = requireInspection(sstv::offline::inspectPcm16Wav(
	    validPath, sstv::offline::defaultWavInspectionLimits()));
	constexpr std::uint64_t expectedSquares
	    = 1'073'741'824ULL + 1ULL + 1ULL + 1'073'676'289ULL;
	require(inspection.audioFormat == 1U && inspection.channels == 1U
	    && inspection.sampleRate == 8'000U && inspection.bitsPerSample == 16U
	    && inspection.dataBytes == 10U && inspection.frameCount == 5U
	    && inspection.duration == sstv::core::Duration(1, 1'600)
	    && inspection.minimumSample == std::numeric_limits<std::int16_t>::min()
	    && inspection.maximumSample == std::numeric_limits<std::int16_t>::max()
	    && inspection.peakAbsolute == 32'768U && inspection.sampleSum == -1
	    && inspection.squareSum == expectedSquares
	    && inspection.clippedPositiveSamples == 1U
	    && inspection.clippedNegativeSamples == 1U,
	    "PCM metadata or exact statistics are wrong");
	require(std::fabs(inspection.dcMean + 0.2L) < 1.0e-15L
	    && std::fabs(inspection.rmsLevel
	        - std::sqrt(static_cast<long double>(expectedSquares) / 5.0L))
	        < 1.0e-12L,
	    "PCM mean or RMS calculation is wrong");
	const std::filesystem::path shortPath = directory / "short.wav";
	writeBytes(shortPath, std::array<std::uint8_t, 1>{0U});
	requireRejected(shortPath, "short WAV was accepted");
	std::vector<std::uint8_t> emptyData = makeWave(
	    std::span<const std::int16_t>{});
	const std::filesystem::path emptyPath = directory / "empty.wav";
	writeBytes(emptyPath, emptyData);
	requireRejected(emptyPath, "empty data chunk was accepted");
	std::vector<std::uint8_t> truncated = makeWave(samples);
	truncated.pop_back();
	const std::filesystem::path truncatedPath = directory / "truncated.wav";
	writeBytes(truncatedPath, truncated);
	requireRejected(truncatedPath, "truncated data chunk was accepted");
	std::vector<std::uint8_t> badRiff = makeWave(samples);
	setU32(badRiff, 4U, 0xfffffff0U);
	const std::filesystem::path badRiffPath = directory / "bad-riff.wav";
	writeBytes(badRiffPath, badRiff);
	requireRejected(badRiffPath, "oversized RIFF declaration was accepted");
	for (const std::string_view tag : {std::string_view("RF64"),
	         std::string_view("RIFX")}) {
		std::vector<std::uint8_t> unsupportedContainer = makeWave(samples);
		for (std::size_t index = 0; index < tag.size(); ++index) {
			unsupportedContainer[index] = static_cast<std::uint8_t>(tag[index]);
		}
		const std::filesystem::path path
		    = directory / (std::string(tag) + ".wav");
		writeBytes(path, unsupportedContainer);
		requireRejected(path, "unsupported RIFF container was accepted");
	}
	std::vector<std::uint8_t> badChunk = makeWave(samples);
	setU32(badChunk, 16U, 0xffffffffU);
	const std::filesystem::path badChunkPath = directory / "bad-chunk.wav";
	writeBytes(badChunkPath, badChunk);
	requireRejected(badChunkPath, "chunk offset overflow attempt was accepted");
	std::vector<std::uint8_t> shortFormat = makeWave(samples);
	setU32(shortFormat, 16U, 10U);
	const std::filesystem::path shortFormatPath = directory / "short-format.wav";
	writeBytes(shortFormatPath, shortFormat);
	requireRejected(shortFormatPath, "malformed fmt chunk size was accepted");
	std::vector<std::uint8_t> badPadding = makeWave(samples, 1U, 1U, 8'000U, 16U, true);
	badPadding[23U] = 1U;
	const std::filesystem::path badPaddingPath = directory / "bad-padding.wav";
	writeBytes(badPaddingPath, badPadding);
	requireRejected(badPaddingPath, "nonzero odd-chunk padding was accepted");
	for (const auto& [name, bytes] : std::array{
		std::pair{"float", makeWave(samples, 3U)},
		std::pair{"compressed", makeWave(samples, 6U)},
		std::pair{"stereo", makeWave(samples, 1U, 2U)},
		std::pair{"rate", makeWave(samples, 1U, 1U, 12'345U)},
		std::pair{"depth", makeWave(samples, 1U, 1U, 8'000U, 8U)},
		std::pair{"extensible", makeWave(samples, 0xfffeU)},
	}) {
		const std::filesystem::path path = directory / (std::string(name) + ".wav");
		writeBytes(path, bytes);
		requireRejected(path, "unsupported WAV format was accepted");
	}
	std::vector<std::uint8_t> duplicate = makeWave(samples);
	constexpr std::array<std::uint8_t, 2> moreData{0U, 0U};
	putChunk(duplicate, "data", moreData);
	setU32(duplicate, 4U, static_cast<std::uint32_t>(duplicate.size() - 8U));
	const std::filesystem::path duplicatePath = directory / "duplicate.wav";
	writeBytes(duplicatePath, duplicate);
	requireRejected(duplicatePath, "duplicate critical chunk was accepted");
	std::vector<std::uint8_t> duplicateFormat = makeWave(samples);
	const std::vector<std::uint8_t> formatChunk(
	    duplicateFormat.begin() + 12, duplicateFormat.begin() + 36);
	duplicateFormat.insert(duplicateFormat.begin() + 36,
	    formatChunk.begin(), formatChunk.end());
	setU32(duplicateFormat, 4U,
	    static_cast<std::uint32_t>(duplicateFormat.size() - 8U));
	const std::filesystem::path duplicateFormatPath
	    = directory / "duplicate-format.wav";
	writeBytes(duplicateFormatPath, duplicateFormat);
	requireRejected(duplicateFormatPath, "duplicate fmt chunk was accepted");
	std::vector<std::uint8_t> noFormat = makeWave(samples);
	noFormat.erase(noFormat.begin() + 12, noFormat.begin() + 36);
	setU32(noFormat, 4U, static_cast<std::uint32_t>(noFormat.size() - 8U));
	const std::filesystem::path noFormatPath = directory / "no-format.wav";
	writeBytes(noFormatPath, noFormat);
	requireRejected(noFormatPath, "missing fmt chunk was accepted");
	std::vector<std::uint8_t> noData = makeWave(samples);
	noData.erase(noData.begin() + 36, noData.end());
	setU32(noData, 4U, static_cast<std::uint32_t>(noData.size() - 8U));
	const std::filesystem::path noDataPath = directory / "no-data.wav";
	writeBytes(noDataPath, noData);
	requireRejected(noDataPath, "missing data chunk was accepted");
	std::vector<std::uint8_t> badAlignment = makeWave(samples);
	badAlignment[32U] = 4U;
	const std::filesystem::path alignmentPath = directory / "alignment.wav";
	writeBytes(alignmentPath, badAlignment);
	requireRejected(alignmentPath, "bad block alignment was accepted");
	std::vector<std::uint8_t> badByteRate = makeWave(samples);
	badByteRate[28U] = 1U;
	const std::filesystem::path byteRatePath = directory / "byte-rate.wav";
	writeBytes(byteRatePath, badByteRate);
	requireRejected(byteRatePath, "bad byte rate was accepted");
	std::vector<std::uint8_t> oddData = makeWave(samples);
	setU32(oddData, 40U, 9U);
	oddData.back() = 0U;
	const std::filesystem::path oddDataPath = directory / "odd-data.wav";
	writeBytes(oddDataPath, oddData);
	requireRejected(oddDataPath, "non-frame-aligned data size was accepted");
	std::vector<std::uint8_t> trailing = makeWave(samples);
	trailing.push_back(0U);
	const std::filesystem::path trailingPath = directory / "trailing.wav";
	writeBytes(trailingPath, trailing);
	requireRejected(trailingPath, "trailing data outside RIFF was accepted");
	const auto smallLimits = sstv::offline::WavInspectionLimits{20U, 1U, 1U};
	requireRejected(validPath, "WAV input byte limit was not enforced", smallLimits);
	const auto unknownLimits = sstv::offline::WavInspectionLimits{
		1'024U, 2U, 10U,
	};
	requireRejected(validPath, "unknown chunk byte limit was not enforced",
	    unknownLimits);
	const auto chunkLimits = sstv::offline::WavInspectionLimits{
		1'024U, 1'024U, 1U,
	};
	requireRejected(validPath, "chunk-count limit was not enforced", chunkLimits);
	const auto zeroLimits = sstv::offline::WavInspectionLimits{0U, 0U, 0U};
	requireRejected(validPath, "zero inspection limits were accepted", zeroLimits);
	requireRejected("https://example.invalid/test.wav", "WAV URL was accepted");
	requireRejected(directory, "WAV directory was accepted");
	const std::filesystem::path linkPath = directory / "known-link.wav";
	std::filesystem::create_symlink(validPath, linkPath);
	requireRejected(linkPath, "WAV symlink was accepted");
	const std::filesystem::path fifoPath = directory / "input.fifo";
	require(::mkfifo(fifoPath.c_str(), S_IRUSR | S_IWUSR) == 0,
	    "failed to create FIFO test fixture");
	requireRejected(fifoPath, "WAV FIFO was accepted");
}

void
testGeneratedModeWavs(const std::filesystem::path& directory)
{
	for (const sstv::core::ModeDescriptor& mode : sstv::core::built_in_modes()) {
		const auto frame = makeModeFrame(mode);
		const auto result = sstv::analog::encodeOfflineTransmission(mode.id,
		    sstv::core::ModeCapability::offlineTestPatternTx, frame.view(), 0.8F);
		const auto disabledResult = sstv::analog::encodeOfflineTransmission(mode.id,
		    sstv::core::ModeCapability::offlineTestPatternTx, frame.view(),
		    sstv::analog::OfflineTransmissionOptions{0.8F, std::nullopt});
		const auto* transmission
		    = std::get_if<sstv::analog::OfflineTransmission>(&result);
		const auto* disabledTransmission
		    = std::get_if<sstv::analog::OfflineTransmission>(&disabledResult);
		require(transmission != nullptr && disabledTransmission != nullptr,
		    "registered mode did not encode");
		const std::filesystem::path path
		    = directory / (std::string(mode.id) + ".wav");
		const std::filesystem::path disabledPath
		    = directory / (std::string(mode.id) + "-disabled.wav");
		const auto metadata = sstv::offline::writePcm16WavAtomic(
		    path, transmission->events, 8'000U, false);
		(void)sstv::offline::writePcm16WavAtomic(
		    disabledPath, disabledTransmission->events, 8'000U, false);
		const auto inspection = requireInspection(sstv::offline::inspectPcm16Wav(
		    path, sstv::offline::defaultWavInspectionLimits()));
		require(inspection.frameCount == metadata.frameCount
		    && inspection.dataBytes == metadata.frameCount * 2U
		    && std::isfinite(inspection.dcMean)
		    && std::isfinite(inspection.rmsLevel)
		    && inspection.minimumSample <= inspection.maximumSample,
		    "generated mode WAV inspection metadata is wrong");
		require(filesEqual(path, disabledPath),
		    "disabled FSK changed frozen generated PCM bytes");
	}
}

} // namespace

int
main()
{
	const std::filesystem::path directory
	    = std::filesystem::temp_directory_path()
	    / ("scanline-sstv-m1f-" + std::to_string(static_cast<long long>(::getpid())));
	std::filesystem::remove_all(directory);
	std::filesystem::create_directories(directory);
	try {
		testFskValidationAndVector();
		testCompositionAndFrozenPrefixes();
		testRenderingAcrossBoundary();
		testWavInspection(directory);
		testGeneratedModeWavs(directory);
		std::filesystem::remove_all(directory);
		return 0;
	} catch (...) {
		std::filesystem::remove_all(directory);
		throw;
	}
}
