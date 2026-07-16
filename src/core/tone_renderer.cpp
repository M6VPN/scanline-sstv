// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/tone_renderer.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/dsp/tone_renderer.hpp>

#include <sstv/core/timing.hpp>

#include <cmath>
#include <numbers>
#include <stdexcept>

namespace sstv::dsp {

ToneRenderer::ToneRenderer(const std::span<const core::ToneEvent> sourceEvents,
    const std::uint32_t sampleRateValue)
	: events(sourceEvents.begin(), sourceEvents.end()), eventIndex(0), frameIndex(0),
	  phase(0.0), sampleRate(static_cast<double>(sampleRateValue))
{
	if (events.empty()) {
		throw std::invalid_argument("tone sequence must not be empty");
	}
	core::SampleBoundaryScheduler scheduler(sampleRateValue);
	eventEndFrames.reserve(events.size());
	for (const core::ToneEvent& event : events) {
		if (event.frequencyHz() >= sampleRate / 2.0) {
			throw std::invalid_argument("tone frequency must be below Nyquist");
		}
		eventEndFrames.push_back(scheduler.advance(event.duration()));
	}
	if (eventEndFrames.back() == 0) {
		throw std::invalid_argument("tone sequence must render at least one frame");
	}
}

bool
ToneRenderer::finished() const noexcept
{
	return frameIndex >= frameCount();
}

std::uint64_t
ToneRenderer::frameCount() const noexcept
{
	return eventEndFrames.back();
}

std::uint64_t
ToneRenderer::framesRendered() const noexcept
{
	return frameIndex;
}

std::size_t
ToneRenderer::render(const std::span<float> output) noexcept
{
	constexpr double twoPi = 2.0 * std::numbers::pi_v<double>;
	std::size_t written = 0;
	while (written < output.size() && eventIndex < events.size()) {
		while (eventIndex < events.size()
		    && frameIndex >= eventEndFrames[eventIndex]) {
			++eventIndex;
		}
		if (eventIndex == events.size()) {
			break;
		}
		const core::ToneEvent& event = events[eventIndex];
		output[written] = event.amplitude() * static_cast<float>(std::sin(phase));
		phase += twoPi * event.frequencyHz() / sampleRate;
		phase -= twoPi * std::floor(phase / twoPi);
		++frameIndex;
		++written;
	}
	return written;
}

} // namespace sstv::dsp
