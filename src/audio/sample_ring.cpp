// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/audio/sample_ring.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/audio/sample_ring.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace sstv::audio {
namespace {

static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
    "M2B requires lock-free 64-bit audio counters");

[[nodiscard]] std::size_t
validateCapacity(const std::size_t capacity)
{
	if (capacity == 0) {
		throw std::invalid_argument("audio ring capacity must be nonzero");
	}
	if (capacity > maximumAudioRingCapacity
	    || capacity > std::numeric_limits<std::size_t>::max() / sizeof(float)) {
		throw std::length_error("audio ring capacity exceeds the supported limit");
	}
	return capacity;
}

[[nodiscard]] std::uint64_t
boundedDistance(const std::uint64_t newer, const std::uint64_t older,
    const std::uint64_t capacity) noexcept
{
	const std::uint64_t distance = newer - older;
	return std::min(distance, capacity);
}

} // namespace

FloatSpscRing::FloatSpscRing(const std::size_t capacity)
	: storage_(validateCapacity(capacity)), capacity_(capacity)
{
}

std::size_t
FloatSpscRing::push(const std::span<const float> samples) noexcept
{
	const std::uint64_t write = writePosition_.load(std::memory_order_relaxed);
	const std::uint64_t read = readPosition_.load(std::memory_order_acquire);
	const std::uint64_t used = boundedDistance(write, read, capacity_);
	const std::size_t count = std::min<std::size_t>(
	    samples.size(), static_cast<std::size_t>(capacity_ - used));
	if (count == 0) {
		return 0;
	}
	const std::size_t offset = static_cast<std::size_t>(write % capacity_);
	const std::size_t first = std::min(count, storage_.size() - offset);
	std::copy_n(samples.data(), first, storage_.data() + offset);
	std::copy_n(samples.data() + first, count - first, storage_.data());
	writePosition_.store(write + count, std::memory_order_release);
	return count;
}

std::size_t
FloatSpscRing::pop(const std::span<float> samples) noexcept
{
	const std::uint64_t read = readPosition_.load(std::memory_order_relaxed);
	const std::uint64_t write = writePosition_.load(std::memory_order_acquire);
	const std::size_t count = std::min<std::size_t>(samples.size(),
	    static_cast<std::size_t>(boundedDistance(write, read, capacity_)));
	if (count == 0) {
		return 0;
	}
	const std::size_t offset = static_cast<std::size_t>(read % capacity_);
	const std::size_t first = std::min(count, storage_.size() - offset);
	std::copy_n(storage_.data() + offset, first, samples.data());
	std::copy_n(storage_.data(), count - first, samples.data() + first);
	readPosition_.store(read + count, std::memory_order_release);
	return count;
}

std::size_t
FloatSpscRing::capacity() const noexcept
{
	return storage_.size();
}

std::size_t
FloatSpscRing::availableRead() const noexcept
{
	const std::uint64_t write = writePosition_.load(std::memory_order_acquire);
	const std::uint64_t read = readPosition_.load(std::memory_order_acquire);
	return static_cast<std::size_t>(boundedDistance(write, read, capacity_));
}

std::size_t
FloatSpscRing::availableWrite() const noexcept
{
	return capacity() - availableRead();
}

void
FloatSpscRing::reset() noexcept
{
	readPosition_.store(0, std::memory_order_relaxed);
	writePosition_.store(0, std::memory_order_relaxed);
}

} // namespace sstv::audio
