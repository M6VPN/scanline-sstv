// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/dsp/tone_renderer.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/core/tone.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sstv::dsp {

/** Pull renderer with continuous oscillator phase across events and output blocks. */
class ToneRenderer {
public:
	ToneRenderer(std::span<const core::ToneEvent>, std::uint32_t);

	[[nodiscard]] bool finished() const noexcept;
	[[nodiscard]] std::uint64_t frameCount() const noexcept;
	[[nodiscard]] std::uint64_t framesRendered() const noexcept;
	[[nodiscard]] std::size_t render(std::span<float>) noexcept;

private:
	std::vector<std::uint64_t> eventEndFrames;
	std::vector<core::ToneEvent> events;
	std::size_t eventIndex;
	std::uint64_t frameIndex;
	double phase;
	double sampleRate;
};

} // namespace sstv::dsp
