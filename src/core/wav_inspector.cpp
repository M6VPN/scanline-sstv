// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/wav_inspector.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/offline/wav_inspector.hpp>

#include <sstv/offline/wav_writer.hpp>

#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace sstv::offline {
namespace {

constexpr std::size_t analysisBlockBytes = 8'192U;
constexpr std::uint16_t pcmFormat = 1U;
constexpr std::uint16_t extensibleFormat = 0xfffeU;

class InspectionFailure final : public std::runtime_error {
public:
	InspectionFailure(const WavInspectErrorCode codeValue,
		std::string operationValue, std::string messageValue)
		: std::runtime_error(messageValue), code(codeValue),
		  operation(std::move(operationValue))
	{
	}

	WavInspectErrorCode code;
	std::string operation;
};

class FileDescriptor {
public:
	explicit FileDescriptor(const int descriptorValue) noexcept
		: value(descriptorValue)
	{
	}

	FileDescriptor(const FileDescriptor&) = delete;
	FileDescriptor& operator=(const FileDescriptor&) = delete;

	~FileDescriptor()
	{
		if (value >= 0) {
			::close(value);
		}
	}

	[[nodiscard]] int get() const noexcept
	{
		return value;
	}

private:
	int value;
};

struct FormatChunk {
	std::uint16_t audioFormat;
	std::uint16_t channels;
	std::uint32_t sampleRate;
	std::uint32_t byteRate;
	std::uint16_t blockAlignment;
	std::uint16_t bitsPerSample;
};

struct DataChunk {
	std::uint64_t offset;
	std::uint64_t size;
};

[[noreturn]] void
fail(const WavInspectErrorCode code, const std::string_view operation,
	const std::string_view message)
{
	throw InspectionFailure(code, std::string(operation), std::string(message));
}

[[nodiscard]] std::uint64_t
checkedAdd(const std::uint64_t left, const std::uint64_t right,
	const std::string_view operation)
{
	if (left > std::numeric_limits<std::uint64_t>::max() - right) {
		fail(WavInspectErrorCode::malformedContainer, operation,
			"integer overflow in RIFF offset or size");
	}
	return left + right;
}

[[nodiscard]] bool
hasTag(const std::span<const std::uint8_t> bytes, const std::string_view tag)
{
	if (bytes.size() < tag.size()) {
		return false;
	}
	for (std::size_t index = 0; index < tag.size(); ++index) {
		if (bytes[index] != static_cast<std::uint8_t>(tag[index])) {
			return false;
		}
	}
	return true;
}

[[nodiscard]] std::uint16_t
readU16(const std::span<const std::uint8_t> bytes, const std::size_t offset)
{
	return static_cast<std::uint16_t>(bytes[offset])
	    | static_cast<std::uint16_t>(
	        static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U);
}

[[nodiscard]] std::uint32_t
readU32(const std::span<const std::uint8_t> bytes, const std::size_t offset)
{
	return static_cast<std::uint32_t>(bytes[offset])
	    | (static_cast<std::uint32_t>(bytes[offset + 1U]) << 8U)
	    | (static_cast<std::uint32_t>(bytes[offset + 2U]) << 16U)
	    | (static_cast<std::uint32_t>(bytes[offset + 3U]) << 24U);
}

void
readAt(const int descriptor, const std::uint64_t offset,
	const std::span<std::uint8_t> bytes, const std::string_view operation)
{
	if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) {
		fail(WavInspectErrorCode::malformedContainer, operation,
			"file offset is not representable");
	}
	std::size_t completed = 0;
	while (completed < bytes.size()) {
		const std::uint64_t currentOffset = checkedAdd(offset, completed, operation);
		const ssize_t count = ::pread(descriptor, bytes.data() + completed,
			bytes.size() - completed, static_cast<off_t>(currentOffset));
		if (count < 0 && errno == EINTR) {
			continue;
		}
		if (count < 0) {
			throw std::system_error(errno, std::generic_category(),
				std::string(operation));
		}
		if (count == 0) {
			fail(WavInspectErrorCode::malformedContainer, operation,
				"unexpected end of file");
		}
		completed += static_cast<std::size_t>(count);
	}
}

[[nodiscard]] FormatChunk
readFormatChunk(const int descriptor, const std::uint64_t offset,
	const std::uint32_t size)
{
	if (size != 16U) {
		fail(WavInspectErrorCode::malformedContainer, "parse fmt chunk",
			"PCM fmt chunk must contain exactly 16 bytes");
	}
	std::array<std::uint8_t, 16> bytes{};
	readAt(descriptor, offset, bytes, "read fmt chunk");
	return {
		readU16(bytes, 0),
		readU16(bytes, 2),
		readU32(bytes, 4),
		readU32(bytes, 8),
		readU16(bytes, 12),
		readU16(bytes, 14),
	};
}

void
validateFormat(const FormatChunk& format, const DataChunk& data)
{
	if (format.audioFormat == extensibleFormat) {
		fail(WavInspectErrorCode::unsupportedFormat, "validate WAV format",
			"WAVE_FORMAT_EXTENSIBLE is not supported");
	}
	if (format.audioFormat != pcmFormat) {
		fail(WavInspectErrorCode::unsupportedFormat, "validate WAV format",
			"only linear PCM format 1 is supported");
	}
	if (format.channels != 1U) {
		fail(WavInspectErrorCode::unsupportedFormat, "validate WAV channels",
			"only mono WAV files are supported");
	}
	if (format.bitsPerSample != 16U) {
		fail(WavInspectErrorCode::unsupportedFormat, "validate WAV bit depth",
			"only signed 16-bit PCM is supported");
	}
	if (!isSupportedSampleRate(format.sampleRate)) {
		fail(WavInspectErrorCode::unsupportedFormat, "validate WAV sample rate",
			"sample rate is not supported by the project");
	}
	constexpr std::uint16_t expectedBlockAlignment = 2U;
	if (format.blockAlignment != expectedBlockAlignment) {
		fail(WavInspectErrorCode::malformedContainer, "validate WAV block alignment",
			"block alignment is inconsistent with mono PCM16");
	}
	if (format.sampleRate > std::numeric_limits<std::uint32_t>::max()
	        / expectedBlockAlignment
	    || format.byteRate != format.sampleRate * expectedBlockAlignment) {
		fail(WavInspectErrorCode::malformedContainer, "validate WAV byte rate",
			"byte rate is inconsistent with sample rate and block alignment");
	}
	if (data.size == 0U) {
		fail(WavInspectErrorCode::malformedContainer, "validate data chunk",
			"PCM data chunk must not be empty");
	}
	if (data.size % format.blockAlignment != 0U) {
		fail(WavInspectErrorCode::malformedContainer, "validate data chunk",
			"PCM data size is not frame-aligned");
	}
}

[[nodiscard]] std::int16_t
decodeSample(const std::uint8_t low, const std::uint8_t high) noexcept
{
	const std::uint16_t raw = static_cast<std::uint16_t>(low)
	    | static_cast<std::uint16_t>(static_cast<std::uint16_t>(high) << 8U);
	const std::int32_t value = raw >= 0x8000U
	    ? static_cast<std::int32_t>(raw) - 65'536
	    : static_cast<std::int32_t>(raw);
	return static_cast<std::int16_t>(value);
}

void
checkedAccumulate(std::int64_t& sum, std::uint64_t& squareSum,
	const std::int16_t sample)
{
	if ((sample > 0 && sum > std::numeric_limits<std::int64_t>::max() - sample)
	    || (sample < 0
	        && sum < std::numeric_limits<std::int64_t>::min() - sample)) {
		fail(WavInspectErrorCode::resourceLimit, "calculate sample statistics",
			"sample sum overflow");
	}
	sum += sample;
	const std::int64_t wideSample = sample;
	const std::uint64_t square = static_cast<std::uint64_t>(wideSample * wideSample);
	if (squareSum > std::numeric_limits<std::uint64_t>::max() - square) {
		fail(WavInspectErrorCode::resourceLimit, "calculate sample statistics",
			"sample square sum overflow");
	}
	squareSum += square;
}

[[nodiscard]] WavInspection
analyseSamples(const int descriptor, const FormatChunk& format, const DataChunk& data)
{
	std::array<std::uint8_t, analysisBlockBytes> bytes{};
	std::uint64_t remaining = data.size;
	std::uint64_t offset = data.offset;
	std::int16_t minimum = std::numeric_limits<std::int16_t>::max();
	std::int16_t maximum = std::numeric_limits<std::int16_t>::min();
	std::uint16_t peak = 0U;
	std::int64_t sum = 0;
	std::uint64_t squareSum = 0U;
	std::uint64_t clippedPositive = 0U;
	std::uint64_t clippedNegative = 0U;
	while (remaining != 0U) {
		const std::size_t count = static_cast<std::size_t>(
			std::min<std::uint64_t>(remaining, bytes.size()));
		readAt(descriptor, offset, std::span<std::uint8_t>(bytes.data(), count),
			"read PCM data");
		for (std::size_t index = 0; index < count; index += 2U) {
			const std::int16_t sample = decodeSample(bytes[index], bytes[index + 1U]);
			minimum = std::min(minimum, sample);
			maximum = std::max(maximum, sample);
			const std::uint16_t magnitude = sample < 0
			    ? static_cast<std::uint16_t>(-static_cast<std::int32_t>(sample))
			    : static_cast<std::uint16_t>(sample);
			peak = std::max(peak, magnitude);
			checkedAccumulate(sum, squareSum, sample);
			clippedPositive += sample == std::numeric_limits<std::int16_t>::max()
			    ? 1U : 0U;
			clippedNegative += sample == std::numeric_limits<std::int16_t>::min()
			    ? 1U : 0U;
		}
		offset = checkedAdd(offset, count, "advance PCM data offset");
		remaining -= count;
	}
	const std::uint64_t frameCount = data.size / format.blockAlignment;
	if (frameCount > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
		fail(WavInspectErrorCode::resourceLimit, "calculate WAV duration",
			"frame count is not representable by the duration model");
	}
	const long double count = static_cast<long double>(frameCount);
	const long double mean = static_cast<long double>(sum) / count;
	const long double rms = std::sqrt(static_cast<long double>(squareSum) / count);
	return {
		format.audioFormat,
		format.channels,
		format.sampleRate,
		format.bitsPerSample,
		data.size,
		frameCount,
		core::Duration(frameCount, format.sampleRate),
		minimum,
		maximum,
		peak,
		sum,
		squareSum,
		mean,
		rms,
		clippedPositive,
		clippedNegative,
	};
}

[[nodiscard]] WavInspection
inspectFile(const std::filesystem::path& input, const WavInspectionLimits& limits)
{
	if (limits.maximumInputBytes == 0U || limits.maximumUnknownChunkBytes == 0U
	    || limits.maximumChunkCount == 0U) {
		fail(WavInspectErrorCode::resourceLimit, "validate inspection limits",
			"WAV inspection limits must be nonzero");
	}
	if (input.empty() || input.string().find("://") != std::string::npos) {
		fail(WavInspectErrorCode::invalidPath, "validate WAV input",
			"input must be a local filesystem path");
	}
	struct stat pathStatus {};
	if (::lstat(input.c_str(), &pathStatus) != 0) {
		throw std::system_error(errno, std::generic_category(), "inspect WAV input");
	}
	if (S_ISLNK(pathStatus.st_mode)) {
		fail(WavInspectErrorCode::symlinkRejected, "validate WAV input",
			"symbolic links are not accepted");
	}
	if (!S_ISREG(pathStatus.st_mode)) {
		fail(WavInspectErrorCode::notRegularFile, "validate WAV input",
			"input must be a regular file");
	}
	if (pathStatus.st_size < 0) {
		fail(WavInspectErrorCode::malformedContainer, "validate WAV input size",
			"input file size is invalid");
	}
	const std::uint64_t fileSize = static_cast<std::uint64_t>(pathStatus.st_size);
	if (fileSize > limits.maximumInputBytes) {
		fail(WavInspectErrorCode::inputTooLarge, "validate WAV input size",
			"input exceeds the WAV inspection byte limit");
	}
	const int rawDescriptor = ::open(input.c_str(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
	if (rawDescriptor < 0) {
		throw std::system_error(errno, std::generic_category(), "open WAV input");
	}
	const FileDescriptor descriptor(rawDescriptor);
	struct stat openedStatus {};
	if (::fstat(descriptor.get(), &openedStatus) != 0) {
		throw std::system_error(errno, std::generic_category(), "inspect open WAV input");
	}
	if (!S_ISREG(openedStatus.st_mode) || openedStatus.st_dev != pathStatus.st_dev
	    || openedStatus.st_ino != pathStatus.st_ino
	    || openedStatus.st_size != pathStatus.st_size) {
		fail(WavInspectErrorCode::notRegularFile, "validate open WAV input",
			"input changed while it was being opened");
	}
	if (fileSize < 12U) {
		fail(WavInspectErrorCode::invalidContainer, "parse RIFF header",
			"file is too short for a RIFF/WAVE header");
	}
	std::array<std::uint8_t, 12> riffHeader{};
	readAt(descriptor.get(), 0U, riffHeader, "read RIFF header");
	if (hasTag(std::span<const std::uint8_t>(riffHeader.data(), 4U), "RF64")) {
		fail(WavInspectErrorCode::unsupportedFormat, "parse RIFF header",
			"RF64 is not supported");
	}
	if (hasTag(std::span<const std::uint8_t>(riffHeader.data(), 4U), "RIFX")) {
		fail(WavInspectErrorCode::unsupportedFormat, "parse RIFF header",
			"big-endian RIFX is not supported");
	}
	if (!hasTag(std::span<const std::uint8_t>(riffHeader.data(), 4U), "RIFF")
	    || !hasTag(std::span<const std::uint8_t>(riffHeader.data() + 8U, 4U),
	        "WAVE")) {
		fail(WavInspectErrorCode::invalidContainer, "parse RIFF header",
			"input is not little-endian RIFF/WAVE");
	}
	const std::uint64_t declaredSize = checkedAdd(readU32(riffHeader, 4), 8U,
		"parse RIFF size");
	if (declaredSize != fileSize) {
		fail(WavInspectErrorCode::malformedContainer, "validate RIFF size",
			declaredSize < fileSize ? "file contains trailing data outside RIFF"
			                        : "RIFF size exceeds the input file");
	}
	std::optional<FormatChunk> format;
	std::optional<DataChunk> data;
	std::uint64_t offset = 12U;
	std::uint32_t chunkCount = 0U;
	while (offset < fileSize) {
		if (chunkCount >= limits.maximumChunkCount) {
			fail(WavInspectErrorCode::resourceLimit, "parse RIFF chunks",
				"RIFF chunk count exceeds the inspection limit");
		}
		if (fileSize - offset < 8U) {
			fail(WavInspectErrorCode::malformedContainer, "parse RIFF chunk header",
				"truncated RIFF chunk header");
		}
		std::array<std::uint8_t, 8> chunkHeader{};
		readAt(descriptor.get(), offset, chunkHeader, "read RIFF chunk header");
		const std::uint32_t chunkSize = readU32(chunkHeader, 4);
		const std::uint64_t chunkDataOffset = checkedAdd(offset, 8U,
			"parse RIFF chunk offset");
		const std::uint64_t paddedSize = checkedAdd(chunkSize, chunkSize & 1U,
			"parse RIFF padded chunk size");
		const std::uint64_t nextOffset = checkedAdd(chunkDataOffset, paddedSize,
			"parse RIFF next chunk offset");
		if (nextOffset > fileSize) {
			fail(WavInspectErrorCode::malformedContainer, "parse RIFF chunk",
				"chunk declaration exceeds the RIFF boundary");
		}
		if ((chunkSize & 1U) != 0U) {
			std::array<std::uint8_t, 1> padding{};
			readAt(descriptor.get(), checkedAdd(chunkDataOffset, chunkSize,
				"locate RIFF padding"), padding, "read RIFF padding");
			if (padding[0] != 0U) {
				fail(WavInspectErrorCode::malformedContainer, "validate RIFF padding",
					"odd-sized RIFF chunk has nonzero padding");
			}
		}
		const std::span<const std::uint8_t> chunkTag(chunkHeader.data(), 4U);
		if (hasTag(chunkTag, "fmt ")) {
			if (format.has_value()) {
				fail(WavInspectErrorCode::malformedContainer, "parse fmt chunk",
					"duplicate fmt chunk");
			}
			format = readFormatChunk(descriptor.get(), chunkDataOffset, chunkSize);
		} else if (hasTag(chunkTag, "data")) {
			if (data.has_value()) {
				fail(WavInspectErrorCode::malformedContainer, "parse data chunk",
					"duplicate data chunk");
			}
			data = DataChunk{chunkDataOffset, chunkSize};
		} else if (chunkSize > limits.maximumUnknownChunkBytes) {
			fail(WavInspectErrorCode::resourceLimit, "skip unknown RIFF chunk",
				"unknown chunk exceeds the inspection byte limit");
		}
		offset = nextOffset;
		++chunkCount;
	}
	if (!format.has_value()) {
		fail(WavInspectErrorCode::malformedContainer, "parse WAV chunks",
			"required fmt chunk is missing");
	}
	if (!data.has_value()) {
		fail(WavInspectErrorCode::malformedContainer, "parse WAV chunks",
			"required data chunk is missing");
	}
	validateFormat(*format, *data);
	return analyseSamples(descriptor.get(), *format, *data);
}

} // namespace

WavInspectionLimits
defaultWavInspectionLimits() noexcept
{
	/* Covers the longest current 192 kHz PCM16 output with bounded chunk traversal. */
	return {
		256U * 1'024U * 1'024U,
		16U * 1'024U * 1'024U,
		4'096U,
	};
}

WavInspectionResult
inspectPcm16Wav(const std::filesystem::path& input,
	const WavInspectionLimits& limits)
{
	try {
		return inspectFile(input, limits);
	} catch (const InspectionFailure& error) {
		return WavInspectError{error.code, error.operation, error.what()};
	} catch (const std::system_error& error) {
		return WavInspectError{WavInspectErrorCode::ioFailure,
			"inspect WAV input", error.what()};
	} catch (const std::exception& error) {
		return WavInspectError{WavInspectErrorCode::malformedContainer,
			"inspect WAV input", error.what()};
	}
}

} // namespace sstv::offline
