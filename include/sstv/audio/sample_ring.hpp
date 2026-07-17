// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/audio/sample_ring.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sstv::audio {

inline constexpr std::size_t maximumAudioRingCapacity = 16'777'216;

/** Fixed-capacity single-producer/single-consumer float sample ring. */
class FloatSpscRing {
public:
	explicit FloatSpscRing(std::size_t capacity);
	FloatSpscRing(const FloatSpscRing&) = delete;
	FloatSpscRing& operator=(const FloatSpscRing&) = delete;
	FloatSpscRing(FloatSpscRing&&) = delete;
	FloatSpscRing& operator=(FloatSpscRing&&) = delete;
	[[nodiscard]] std::size_t push(std::span<const float> samples) noexcept;
	[[nodiscard]] std::size_t pop(std::span<float> samples) noexcept;
	[[nodiscard]] std::size_t capacity() const noexcept;
	[[nodiscard]] std::size_t availableRead() const noexcept;
	[[nodiscard]] std::size_t availableWrite() const noexcept;
	void reset() noexcept;

private:
	std::vector<float> storage_;
	const std::uint64_t capacity_;
	alignas(64) std::atomic<std::uint64_t> readPosition_{0};
	alignas(64) std::atomic<std::uint64_t> writePosition_{0};
};

} // namespace sstv::audio
