// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/offline/wav_inspector.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/core/timing.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>

namespace sstv::offline {

enum class WavInspectErrorCode {
	invalidPath,
	symlinkRejected,
	notRegularFile,
	inputTooLarge,
	resourceLimit,
	ioFailure,
	invalidContainer,
	malformedContainer,
	unsupportedFormat,
};

struct WavInspectError {
	WavInspectErrorCode code;
	std::string operation;
	std::string message;
};

struct WavInspectionLimits {
	std::uint64_t maximumInputBytes;
	std::uint64_t maximumUnknownChunkBytes;
	std::uint32_t maximumChunkCount;
};

struct WavInspection {
	std::uint16_t audioFormat;
	std::uint16_t channels;
	std::uint32_t sampleRate;
	std::uint16_t bitsPerSample;
	std::uint64_t dataBytes;
	std::uint64_t frameCount;
	core::Duration duration;
	std::int16_t minimumSample;
	std::int16_t maximumSample;
	std::uint16_t peakAbsolute;
	std::int64_t sampleSum;
	std::uint64_t squareSum;
	long double dcMean;
	long double rmsLevel;
	std::uint64_t clippedPositiveSamples;
	std::uint64_t clippedNegativeSamples;
};

using WavInspectionResult = std::variant<WavInspection, WavInspectError>;

/** Return bounded defaults covering every current offline encoder output. */
[[nodiscard]] WavInspectionLimits defaultWavInspectionLimits() noexcept;

/** Inspect one bounded local mono PCM16 RIFF/WAVE file without decoding SSTV. */
[[nodiscard]] WavInspectionResult inspectPcm16Wav(
	const std::filesystem::path&, const WavInspectionLimits&);

} // namespace sstv::offline
