// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/app/rendered_transmit.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/app/rendered_transmit.hpp>

#include <sstv/dsp/tone_renderer.hpp>
#include <sstv/offline/wav_writer.hpp>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cmath>
#include <exception>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace sstv::app {
namespace {

class ToneEventSampleSource final : public FiniteSampleSource {
public:
	ToneEventSampleSource(analog::OfflineTransmission transmission,
		const std::uint32_t sampleRate, const float gain)
		: transmission_(std::move(transmission)),
		  renderer_(transmission_.events, sampleRate),
		  facts_{sampleRate, renderer_.frameCount(), hasSignal(transmission_.events)},
		  gain_(gain) {}
	[[nodiscard]] FiniteSourceFacts facts() const noexcept override { return facts_; }
	[[nodiscard]] SampleReadResult read(
		const std::span<float> destination) noexcept override
	{
		if (isCancelled_.load(std::memory_order_acquire)) return std::size_t{0};
		const std::size_t rendered = renderer_.render(destination);
		std::transform(destination.begin(), destination.begin()
			+ static_cast<std::ptrdiff_t>(rendered), destination.begin(),
			[this](const float sample) { return sample * gain_; });
		return rendered;
	}
	[[nodiscard]] bool isExhausted() const noexcept override
	{
		return isCancelled_.load(std::memory_order_acquire) || renderer_.finished();
	}
	void cancel() noexcept override
	{
		isCancelled_.store(true, std::memory_order_release);
	}

private:
	[[nodiscard]] static bool hasSignal(
		const std::span<const core::ToneEvent> events) noexcept
	{
		return std::ranges::any_of(events,
			[](const core::ToneEvent& event) { return event.amplitude() != 0.0F; });
	}
	const analog::OfflineTransmission transmission_;
	dsp::ToneRenderer renderer_;
	FiniteSourceFacts facts_;
	float gain_ = 1.0F;
	std::atomic<bool> isCancelled_{false};
};

[[nodiscard]] TransmitAudioError
endpointError(std::string operation, std::string message)
{
	return {std::move(operation), std::move(message)};
}

[[nodiscard]] TransmitAudioError
endpointError(const audio::StreamError& error)
{
	return endpointError(error.operation, error.message);
}

[[nodiscard]] TransmitAudioFault
mapFault(const audio::StreamFaultReason reason) noexcept
{
	switch (reason) {
	case audio::StreamFaultReason::none: return TransmitAudioFault::none;
	case audio::StreamFaultReason::backendDisconnect:
		return TransmitAudioFault::disconnected;
	case audio::StreamFaultReason::deviceRemoved:
		return TransmitAudioFault::deviceRemoved;
	case audio::StreamFaultReason::callbackContract:
	case audio::StreamFaultReason::adapterFailure:
		return TransmitAudioFault::adapterFailure;
	}
	return TransmitAudioFault::adapterFailure;
}

class AudioStreamTransmitEndpoint final : public TransmitAudioEndpoint {
public:
	AudioStreamTransmitEndpoint(std::unique_ptr<audio::AudioStream> stream,
		std::shared_ptr<const audio::AudioDiscoverySnapshot> discovery)
		: stream_(std::move(stream)), discovery_(std::move(discovery)),
		  silence_(stream_->configuration().periodFrames, 0.0F) {}
	[[nodiscard]] TransmitAudioResult open() noexcept override
	{
		if (discovery_ == nullptr) {
			return endpointError("open", "audio discovery snapshot is required");
		}
		if (const audio::StreamOperationResult result = stream_->open(*discovery_)) {
			return endpointError(*result);
		}
		return std::nullopt;
	}
	[[nodiscard]] std::optional<audio::NegotiatedStreamFacts>
	negotiated() const noexcept override
	{
		return stream_->statistics().negotiated;
	}
	[[nodiscard]] TransmitAudioResult prefillSilence(
		const std::size_t frames) noexcept override
	{
		std::size_t remaining = frames;
		while (remaining != 0) {
			const std::size_t chunk = std::min(remaining, silence_.size());
			const std::size_t written = stream_->queuePlayback(
				std::span<const float>(silence_.data(), chunk));
			if (written == 0) {
				return endpointError("prefill", "playback ring cannot accept required silence");
			}
			remaining -= written;
		}
		return std::nullopt;
	}
	[[nodiscard]] TransmitAudioResult prime() noexcept override
	{
		if (const audio::StreamOperationResult result = stream_->prime()) {
			return endpointError(*result);
		}
		return std::nullopt;
	}
	[[nodiscard]] TransmitAudioResult start() noexcept override
	{
		if (const audio::StreamOperationResult result = stream_->start()) {
			return endpointError(*result);
		}
		return std::nullopt;
	}
	[[nodiscard]] std::size_t queue(
		const std::span<const float> samples) noexcept override
	{
		if (isSignalFinished_ || isSignalGated_) return 0;
		const std::size_t written = stream_->queuePlayback(samples);
		if (written != 0 && !hasSignalStarted_) {
			hasSignalStarted_ = true;
			underrunBaseline_ = stream_->statistics().underrunFrames;
		}
		return written;
	}
	void finishSignal() noexcept override
	{
		if (hasSignalStarted_
			&& stream_->statistics().underrunFrames > underrunBaseline_) {
			hasSignalUnderrun_ = true;
		}
		isSignalFinished_ = true;
	}
	void gateSignal() noexcept override
	{
		isSignalGated_ = true;
		stream_->gatePlaybackSignal();
	}
	[[nodiscard]] TransmitAudioStatus status() const noexcept override
	{
		const audio::StreamState state = stream_->poll();
		const audio::AudioStreamStatistics statistics = stream_->statistics();
		TransmitAudioFault fault = mapFault(statistics.faultReason);
		if (fault == TransmitAudioFault::none && hasSignalUnderrun_) {
			fault = TransmitAudioFault::underrun;
		} else if (fault == TransmitAudioFault::none && hasSignalStarted_
			&& !isSignalFinished_ && statistics.underrunFrames > underrunBaseline_) {
			fault = TransmitAudioFault::underrun;
		}
		return {fault, statistics.playbackRingFill,
			isSignalFinished_ && statistics.playbackRingFill == 0,
			state != audio::StreamState::closed,
			state == audio::StreamState::primed || state == audio::StreamState::running,
			state == audio::StreamState::running};
	}
	[[nodiscard]] TransmitAudioResult requestStop() noexcept override
	{
		if (const audio::StreamOperationResult result = stream_->requestStop()) {
			return endpointError(*result);
		}
		return std::nullopt;
	}
	[[nodiscard]] TransmitAudioResult stop() noexcept override
	{
		if (const audio::StreamOperationResult result = stream_->stop()) {
			return endpointError(*result);
		}
		return std::nullopt;
	}
	[[nodiscard]] TransmitAudioResult close() noexcept override
	{
		if (const audio::StreamOperationResult result = stream_->close()) {
			return endpointError(*result);
		}
		return std::nullopt;
	}

private:
	std::unique_ptr<audio::AudioStream> stream_;
	std::shared_ptr<const audio::AudioDiscoverySnapshot> discovery_;
	std::vector<float> silence_;
	std::uint64_t underrunBaseline_ = 0;
	bool hasSignalStarted_ = false;
	bool hasSignalUnderrun_ = false;
	bool isSignalFinished_ = false;
	bool isSignalGated_ = false;
};

} // namespace

FiniteSampleSourceCreateResult
createToneEventSampleSource(analog::OfflineTransmission transmission,
	const std::uint32_t sampleRate)

{
	return createToneEventSampleSource(std::move(transmission), sampleRate, 1.0F);
}

FiniteSampleSourceCreateResult
createToneEventSampleSource(analog::OfflineTransmission transmission,
	const std::uint32_t sampleRate, const float gain)
{
	try {
		if (!offline::isSupportedSampleRate(sampleRate)) {
			return SampleSourceError{"unsupported rendered source sample rate"};
		}
		if (!std::isfinite(gain) || gain <= 0.0F) {
			return SampleSourceError{"rendered source gain must be finite and positive"};
		}
		core::Duration duration(0, 1);
		for (const core::ToneEvent& event : transmission.events) {
			if (!std::isfinite(event.frequencyHz())
				|| !std::isfinite(event.amplitude())) {
				return SampleSourceError{"tone event contains a non-finite value"};
			}
			if (std::abs(event.amplitude()) * gain > 1.0F) {
				return SampleSourceError{"rendered source gain can clip a tone event"};
			}
			duration = duration + event.duration();
		}
		if (!(duration == transmission.duration)) {
			return SampleSourceError{"tone-event duration does not match transmission duration"};
		}
		auto source = std::make_unique<ToneEventSampleSource>(
			std::move(transmission), sampleRate, gain);
		if (source->facts().frameCount == 0
			|| source->facts().frameCount > maximumTransmitSourceFrames) {
			return SampleSourceError{"rendered source frame count exceeds transmit limits"};
		}
		return std::unique_ptr<FiniteSampleSource>(std::move(source));
	} catch (const std::exception& error) {
		return SampleSourceError{error.what()};
	}
}

TransmitAudioEndpointCreateResult
createAudioStreamTransmitEndpoint(AudioStreamTransmitEndpointRequest request,
	std::unique_ptr<audio::AudioStreamAdapter> adapter)
{
	if (request.discovery == nullptr) {
		return endpointError("create", "audio discovery snapshot is required");
	}
	if (request.configuration.direction != audio::StreamDirection::playback) {
		return endpointError("create", "transmit endpoint must be playback-only");
	}
	audio::AudioStreamCreateResult created = audio::createAudioStream(
		std::move(request.configuration), std::move(adapter));
	if (const auto* error = std::get_if<audio::StreamError>(&created)) {
		return endpointError(*error);
	}
	return std::unique_ptr<TransmitAudioEndpoint>(
		std::make_unique<AudioStreamTransmitEndpoint>(
			std::get<std::unique_ptr<audio::AudioStream>>(std::move(created)),
			std::move(request.discovery)));
}

} // namespace sstv::app
