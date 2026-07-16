// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/wav_writer.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/offline/wav_writer.hpp>

#include <sstv/dsp/tone_renderer.hpp>

#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>

namespace sstv::offline {
namespace {

constexpr std::size_t renderBlockFrames = 4'096;

class TemporaryFile {
public:
	explicit TemporaryFile(const std::filesystem::path& destination)
		: descriptor(-1)
	{
		for (unsigned int attempt = 0; attempt < 100U; ++attempt) {
			path = destination.string() + ".tmp."
			    + std::to_string(static_cast<long long>(::getpid())) + "."
			    + std::to_string(attempt);
			descriptor = ::open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
			if (descriptor >= 0) {
				return;
			}
			if (errno != EEXIST) {
				throw std::system_error(errno, std::generic_category(),
				    "create temporary WAV");
			}
		}
		throw std::runtime_error("could not create a unique temporary WAV");
	}

	TemporaryFile(const TemporaryFile&) = delete;
	TemporaryFile& operator=(const TemporaryFile&) = delete;

	~TemporaryFile()
	{
		if (descriptor >= 0) {
			::close(descriptor);
		}
		if (!path.empty()) {
			::unlink(path.c_str());
		}
	}

	void closeAndSync()
	{
		if (::fsync(descriptor) != 0) {
			throw std::system_error(errno, std::generic_category(), "sync temporary WAV");
		}
		if (::close(descriptor) != 0) {
			descriptor = -1;
			throw std::system_error(errno, std::generic_category(), "close temporary WAV");
		}
		descriptor = -1;
	}

	[[nodiscard]] int fileDescriptor() const noexcept
	{
		return descriptor;
	}

	void publish(const std::filesystem::path& destination, const bool force)
	{
		const int result = force ? ::rename(path.c_str(), destination.c_str())
		    : ::link(path.c_str(), destination.c_str());
		if (result != 0) {
			throw std::system_error(errno, std::generic_category(), "publish WAV");
		}
		if (!force && ::unlink(path.c_str()) != 0) {
			const int unlinkError = errno;
			::unlink(destination.c_str());
			throw std::system_error(unlinkError, std::generic_category(),
			    "remove temporary WAV link");
		}
		path.clear();
	}

private:
	int descriptor;
	std::filesystem::path path;
};

void
putTag(std::array<std::uint8_t, 44>& header, const std::size_t offset,
    const char first, const char second, const char third, const char fourth) noexcept
{
	header[offset] = static_cast<std::uint8_t>(first);
	header[offset + 1U] = static_cast<std::uint8_t>(second);
	header[offset + 2U] = static_cast<std::uint8_t>(third);
	header[offset + 3U] = static_cast<std::uint8_t>(fourth);
}

void
putU16(std::array<std::uint8_t, 44>& header, const std::size_t offset,
    const std::uint16_t value) noexcept
{
	header[offset] = static_cast<std::uint8_t>(value & 0xFFU);
	header[offset + 1U] = static_cast<std::uint8_t>(value >> 8U);
}

void
putU32(std::array<std::uint8_t, 44>& header, const std::size_t offset,
    const std::uint32_t value) noexcept
{
	header[offset] = static_cast<std::uint8_t>(value & 0xFFU);
	header[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
	header[offset + 2U] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
	header[offset + 3U] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

void
writeAll(const int descriptor, const std::span<const std::uint8_t> bytes)
{
	std::size_t written = 0;
	while (written < bytes.size()) {
		const ssize_t result = ::write(descriptor, bytes.data() + written,
		    bytes.size() - written);
		if (result < 0 && errno == EINTR) {
			continue;
		}
		if (result <= 0) {
			throw std::system_error(errno, std::generic_category(), "write WAV");
		}
		written += static_cast<std::size_t>(result);
	}
}

[[nodiscard]] std::array<std::uint8_t, 44>
wavHeader(const std::uint32_t sampleRate, const std::uint32_t dataBytes)
{
	std::array<std::uint8_t, 44> header{};
	putTag(header, 0, 'R', 'I', 'F', 'F');
	putU32(header, 4, 36U + dataBytes);
	putTag(header, 8, 'W', 'A', 'V', 'E');
	putTag(header, 12, 'f', 'm', 't', ' ');
	putU32(header, 16, 16);
	putU16(header, 20, 1);
	putU16(header, 22, 1);
	putU32(header, 24, sampleRate);
	putU32(header, 28, sampleRate * 2U);
	putU16(header, 32, 2);
	putU16(header, 34, 16);
	putTag(header, 36, 'd', 'a', 't', 'a');
	putU32(header, 40, dataBytes);
	return header;
}

} // namespace

std::int16_t
floatToPcm16(const float sample)
{
	if (!std::isfinite(sample)) {
		throw std::invalid_argument("PCM input sample must be finite");
	}
	if (sample <= -1.0F) {
		return std::numeric_limits<std::int16_t>::min();
	}
	if (sample >= 1.0F) {
		return std::numeric_limits<std::int16_t>::max();
	}
	const double scale = sample < 0.0F ? 32'768.0 : 32'767.0;
	return static_cast<std::int16_t>(std::lround(static_cast<double>(sample) * scale));
}

bool
isSupportedSampleRate(const std::uint32_t sampleRate) noexcept
{
	constexpr std::array<std::uint32_t, 11> rates{
		8'000, 11'025, 16'000, 22'050, 32'000, 44'100,
		48'000, 88'200, 96'000, 176'400, 192'000,
	};
	for (const std::uint32_t rate : rates) {
		if (rate == sampleRate) {
			return true;
		}
	}
	return false;
}

WavMetadata
writePcm16WavAtomic(const std::filesystem::path& destination,
    const std::span<const core::ToneEvent> events, const std::uint32_t sampleRate,
    const bool force)
{
	if (!isSupportedSampleRate(sampleRate)) {
		throw std::invalid_argument("unsupported sample rate");
	}
	std::error_code error;
	if (!force && std::filesystem::exists(destination, error)) {
		throw std::runtime_error("output already exists; use --force to replace it");
	}
	if (error) {
		throw std::system_error(error, "inspect WAV destination");
	}
	dsp::ToneRenderer renderer(events, sampleRate);
	constexpr std::uint64_t maximumFrames
	    = (std::numeric_limits<std::uint32_t>::max() - 36U) / 2U;
	if (renderer.frameCount() > maximumFrames) {
		throw std::overflow_error("PCM data exceeds RIFF size limits");
	}
	const std::uint32_t dataBytes = static_cast<std::uint32_t>(renderer.frameCount() * 2U);
	TemporaryFile temporary(destination);
	const std::array<std::uint8_t, 44> header = wavHeader(sampleRate, dataBytes);
	writeAll(temporary.fileDescriptor(), header);
	std::array<float, renderBlockFrames> samples{};
	std::array<std::uint8_t, renderBlockFrames * 2U> pcmBytes{};
	while (!renderer.finished()) {
		const std::size_t count = renderer.render(samples);
		for (std::size_t index = 0; index < count; ++index) {
			const std::uint16_t pcm = static_cast<std::uint16_t>(floatToPcm16(samples[index]));
			pcmBytes[index * 2U] = static_cast<std::uint8_t>(pcm & 0xFFU);
			pcmBytes[index * 2U + 1U] = static_cast<std::uint8_t>(pcm >> 8U);
		}
		writeAll(temporary.fileDescriptor(),
		    std::span<const std::uint8_t>(pcmBytes.data(), count * 2U));
	}
	temporary.closeAndSync();
	temporary.publish(destination, force);
	return {sampleRate, renderer.frameCount()};
}

} // namespace sstv::offline
