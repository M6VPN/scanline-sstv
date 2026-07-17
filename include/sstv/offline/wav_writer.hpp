// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/offline/wav_writer.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/core/tone.hpp>

#include <cstdint>
#include <filesystem>
#include <span>

namespace sstv::offline {

struct WavMetadata {
	std::uint32_t sampleRate;
	std::uint64_t frameCount;
};

/** Convert one finite float sample to documented signed PCM16. */
[[nodiscard]] std::int16_t floatToPcm16(float);

/** Return whether a rate is supported by the offline diagnostic writer. */
[[nodiscard]] bool isSupportedSampleRate(std::uint32_t) noexcept;
[[nodiscard]] std::span<const std::uint32_t> supportedSampleRates() noexcept;

/** Stream events to a temporary mono PCM16 WAV and atomically publish it. */
[[nodiscard]] WavMetadata writePcm16WavAtomic(const std::filesystem::path&,
    std::span<const core::ToneEvent>, std::uint32_t, bool);

} // namespace sstv::offline
