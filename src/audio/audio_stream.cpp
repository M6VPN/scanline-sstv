// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/audio/audio_stream.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/audio/audio_stream.hpp>

#include <algorithm>
#include <atomic>
#include <exception>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace sstv::audio {
namespace {

static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
static_assert(std::atomic<std::uint32_t>::is_always_lock_free);

[[nodiscard]] bool
hasPlayback(const StreamDirection direction) noexcept
{
	return direction == StreamDirection::playback || direction == StreamDirection::duplex;
}

[[nodiscard]] bool
hasCapture(const StreamDirection direction) noexcept
{
	return direction == StreamDirection::capture || direction == StreamDirection::duplex;
}

[[nodiscard]] StreamError
makeError(const StreamErrorCode code, std::string operation,
    std::string message, const bool isRecoverable = false)
{
	return {code, std::move(operation), std::move(message), isRecoverable};
}

[[nodiscard]] StreamOperationResult
validateConfiguration(const AudioStreamConfiguration& configuration,
    const AudioStreamAdapter* const adapter)
{
	if (adapter == nullptr) {
		return makeError(StreamErrorCode::invalidConfiguration,
		    "create", "audio stream adapter must not be null");
	}
	const bool playback = hasPlayback(configuration.direction);
	const bool capture = hasCapture(configuration.direction);
	if (configuration.sampleRate != nominalAudioSampleRate) {
		return makeError(StreamErrorCode::invalidConfiguration, "create",
		    "M2B accepts only the 48000 Hz internal sample rate");
	}
	if (configuration.periodFrames == 0
	    || configuration.periodFrames > maximumAudioPeriodFrames
	    || configuration.periodCount == 0
	    || configuration.periodCount > maximumAudioPeriodCount) {
		return makeError(StreamErrorCode::invalidConfiguration, "create",
		    "audio period size or count is outside the supported range");
	}
	if (playback != configuration.playbackDevice.has_value()
	    || capture != configuration.captureDevice.has_value()) {
		return makeError(StreamErrorCode::invalidConfiguration, "create",
		    "stream direction and selected devices do not match");
	}
	if ((playback && (configuration.playbackChannels == 0
	        || configuration.playbackChannels > maximumAudioChannels
	        || configuration.selectedPlaybackChannel >= configuration.playbackChannels))
	    || (!playback && configuration.playbackChannels != 0)) {
		return makeError(StreamErrorCode::invalidConfiguration, "create",
		    "playback channels or selected channel are invalid");
	}
	if ((capture && (configuration.captureChannels == 0
	        || configuration.captureChannels > maximumAudioChannels
	        || configuration.selectedCaptureChannel >= configuration.captureChannels))
	    || (!capture && configuration.captureChannels != 0)) {
		return makeError(StreamErrorCode::invalidConfiguration, "create",
		    "capture channels or selected channel are invalid");
	}
	if (!playback && configuration.playbackPrefillFrames != 0) {
		return makeError(StreamErrorCode::invalidConfiguration, "create",
		    "capture-only streams cannot request playback prefill");
	}
	if (configuration.playbackPrefillFrames > configuration.playbackRingCapacity) {
		return makeError(StreamErrorCode::invalidConfiguration, "create",
		    "playback prefill exceeds the playback ring capacity");
	}
	if (playback && configuration.playbackDevice->identity.direction
	        != AudioDirection::playback) {
		return makeError(StreamErrorCode::invalidIdentity, "create",
		    "playback selection has the wrong direction");
	}
	if (capture && configuration.captureDevice->identity.direction
	        != AudioDirection::capture) {
		return makeError(StreamErrorCode::invalidIdentity, "create",
		    "capture selection has the wrong direction");
	}
	if (playback && capture
	    && configuration.playbackDevice->identity.backend
	        != configuration.captureDevice->identity.backend) {
		return makeError(StreamErrorCode::invalidConfiguration, "create",
		    "duplex playback and capture must use the same backend");
	}
	return std::nullopt;
}

void
updateHighWater(std::atomic<std::uint64_t>& highWater,
    const std::size_t value) noexcept
{
	const std::uint64_t current = highWater.load(std::memory_order_relaxed);
	if (value > current) {
		highWater.store(value, std::memory_order_relaxed);
	}
}

} // namespace

struct AudioStream::CallbackState {
	CallbackState(FloatSpscRing* playback, FloatSpscRing* capture,
	    const std::size_t scratchSize, const AudioStreamConfiguration& configuration)
		: playbackRing(playback), captureRing(capture), playbackScratch(scratchSize),
		  captureScratch(scratchSize), hasPlayback(
		      configuration.direction != StreamDirection::capture), hasCapture(
		      configuration.direction != StreamDirection::playback),
		  playbackMapping(configuration.playbackMapping),
		  playbackChannels(configuration.playbackChannels),
		  captureChannels(configuration.captureChannels),
		  selectedPlaybackChannel(configuration.selectedPlaybackChannel),
		  selectedCaptureChannel(configuration.selectedCaptureChannel)
	{
	}
	FloatSpscRing* playbackRing;
	FloatSpscRing* captureRing;
	std::vector<float> playbackScratch;
	std::vector<float> captureScratch;
	bool hasPlayback;
	bool hasCapture;
	PlaybackMappingPolicy playbackMapping;
	std::uint32_t playbackChannels;
	std::uint32_t captureChannels;
	std::uint32_t selectedPlaybackChannel;
	std::uint32_t selectedCaptureChannel;
	std::atomic<std::uint64_t> callbacksExecuted{0};
	std::atomic<std::uint64_t> playbackFramesRequested{0};
	std::atomic<std::uint64_t> playbackFramesDelivered{0};
	std::atomic<std::uint64_t> playbackFramesZeroFilled{0};
	std::atomic<std::uint64_t> underrunCallbacks{0};
	std::atomic<std::uint64_t> underrunFrames{0};
	std::atomic<std::uint64_t> captureFramesReceived{0};
	std::atomic<std::uint64_t> captureFramesWritten{0};
	std::atomic<std::uint64_t> captureFramesDropped{0};
	std::atomic<std::uint64_t> overrunCallbacks{0};
	std::atomic<std::uint64_t> overrunFrames{0};
	std::atomic<std::uint64_t> discontinuityGeneration{0};
	std::atomic<std::uint64_t> playbackRingHighWater{0};
	std::atomic<std::uint64_t> captureRingHighWater{0};
	std::atomic<std::uint64_t> startCount{0};
	std::atomic<std::uint64_t> stopCount{0};
	std::atomic<std::uint64_t> faultCount{0};
	std::atomic<std::uint32_t> faultReason{
	    static_cast<std::uint32_t>(StreamFaultReason::none)};
	std::atomic<std::uint32_t> activeCallbacks{0};
	std::atomic<bool> stopRequested{false};
};

void
AudioStream::markAdapterFault(CallbackState& state) noexcept
{
	state.faultReason.store(static_cast<std::uint32_t>(StreamFaultReason::adapterFailure),
	    std::memory_order_release);
	state.faultCount.fetch_add(1, std::memory_order_relaxed);
	state.discontinuityGeneration.fetch_add(1, std::memory_order_relaxed);
}

AudioStream::AudioStream(AudioStreamConfiguration configuration,
    std::unique_ptr<AudioStreamAdapter> adapter)
	: configuration_(std::move(configuration)),
	  playbackRing_(configuration_.playbackRingCapacity),
	  captureRing_(configuration_.captureRingCapacity), adapter_(std::move(adapter))
{
	const std::size_t scratchSize = configuration_.periodFrames;
	callbackState_ = std::make_unique<CallbackState>(
	    &playbackRing_, &captureRing_, scratchSize, configuration_);
}

AudioStream::~AudioStream()
{
	(void)close();
}

StreamOperationResult
AudioStream::validateSnapshot(const AudioDiscoverySnapshot& snapshot) const
{
	const auto validate = [&snapshot](const StreamDeviceSelection& selection,
	                          const AudioDirection expectedDirection)
	    -> StreamOperationResult {
		if (selection.discoveryGeneration == 0
		    || selection.discoveryGeneration != snapshot.generation) {
			return makeError(StreamErrorCode::staleIdentity, "open",
			    "selected device identity is outside its discovery generation", true);
		}
		if (selection.identity.direction != expectedDirection
		    || selection.identity.opaque.empty()) {
			return makeError(StreamErrorCode::invalidIdentity, "open",
			    "selected device identity is malformed or has the wrong direction");
		}
		const AudioDevice* match = nullptr;
		std::size_t matches = 0;
		for (const BackendDiscovery& backend : snapshot.backends) {
			for (const AudioDevice& device : backend.devices) {
				if (device.identity.backend == selection.identity.backend
				    && device.identity.direction == selection.identity.direction
				    && device.identity.opaque == selection.identity.opaque) {
					match = &device;
					++matches;
				}
			}
		}
		if (matches == 0) {
			return makeError(StreamErrorCode::deviceNotFound, "open",
			    "selected device is not present in the discovery snapshot", true);
		}
		if (matches != 1 || match->hasIdentityCollision) {
			return makeError(StreamErrorCode::identityCollision, "open",
			    "selected device identity is colliding or ambiguous");
		}
		return std::nullopt;
	};
	if (configuration_.playbackDevice) {
		if (StreamOperationResult result = validate(
		        *configuration_.playbackDevice, AudioDirection::playback)) {
			return result;
		}
	}
	if (configuration_.captureDevice) {
		return validate(*configuration_.captureDevice, AudioDirection::capture);
	}
	return std::nullopt;
}

StreamOperationResult
AudioStream::transitionError(std::string operation, std::string message) const
{
	return makeError(StreamErrorCode::invalidTransition,
	    std::move(operation), std::move(message), true);
}

StreamOperationResult
AudioStream::open(const AudioDiscoverySnapshot& snapshot,
    const std::stop_token stopToken)
{
	if (state_ != StreamState::closed) {
		return transitionError("open", "stream must be closed before opening");
	}
	if (StreamOperationResult validation = validateSnapshot(snapshot)) {
		return validation;
	}
	if (stopToken.stop_requested()) {
		return makeError(StreamErrorCode::cancelled, "open",
		    "stream opening was cancelled", true);
	}
	state_ = StreamState::opening;
	callbackState_->faultReason.store(
	    static_cast<std::uint32_t>(StreamFaultReason::none), std::memory_order_relaxed);
	callbackState_->stopRequested.store(false, std::memory_order_relaxed);
	const AudioCallbackBinding binding{
	    callbackState_.get(), processCallback, notifyCallbackFault};
	AdapterOpenResult result = adapter_->open(configuration_, binding, stopToken);
	if (const auto* error = std::get_if<StreamError>(&result)) {
		state_ = StreamState::faulted;
		markAdapterFault(*callbackState_);
		return *error;
	}
	if (stopToken.stop_requested()) {
		(void)adapter_->close();
		state_ = StreamState::closed;
		return makeError(StreamErrorCode::cancelled, "open",
		    "stream opening was cancelled", true);
	}
	NegotiatedStreamFacts facts = std::get<NegotiatedStreamFacts>(std::move(result));
	const AudioBackend requestedBackend = configuration_.playbackDevice
	    ? configuration_.playbackDevice->identity.backend
	    : configuration_.captureDevice->identity.backend;
	const bool invalidPlayback = hasPlayback(configuration_.direction)
	    && (!facts.playback
	        || facts.playback->callbackFormat != SampleFormat::float32
	        || facts.playback->callbackChannels == 0
	        || configuration_.selectedPlaybackChannel
	            >= facts.playback->callbackChannels);
	const bool invalidCapture = hasCapture(configuration_.direction)
	    && (!facts.capture
	        || facts.capture->callbackFormat != SampleFormat::float32
	        || facts.capture->callbackChannels == 0
	        || configuration_.selectedCaptureChannel
	            >= facts.capture->callbackChannels);
	if (facts.backend != requestedBackend
	    || facts.callbackSampleRate != configuration_.sampleRate
	    || facts.periodFrames == 0 || invalidPlayback || invalidCapture) {
		(void)adapter_->close();
		state_ = StreamState::closed;
		return makeError(StreamErrorCode::adapterFailure, "open",
		    "adapter returned an incompatible negotiated configuration");
	}
	callbackState_->playbackChannels = facts.playback
	    ? facts.playback->callbackChannels : 0;
	callbackState_->captureChannels = facts.capture
	    ? facts.capture->callbackChannels : 0;
	negotiated_ = std::move(facts);
	state_ = StreamState::opened;
	return std::nullopt;
}

StreamOperationResult
AudioStream::prime(const std::stop_token stopToken)
{
	if (state_ != StreamState::opened) {
		return transitionError("prime", "stream must be opened before priming");
	}
	if (stopToken.stop_requested()) {
		return makeError(StreamErrorCode::cancelled, "prime",
		    "stream priming was cancelled", true);
	}
	if (StreamOperationResult result = adapter_->prime()) {
		state_ = StreamState::faulted;
		markAdapterFault(*callbackState_);
		return result;
	}
	if (stopToken.stop_requested()) {
		return makeError(StreamErrorCode::cancelled, "prime",
		    "stream priming was cancelled", true);
	}
	state_ = StreamState::primed;
	return std::nullopt;
}

StreamOperationResult
AudioStream::start()
{
	if (state_ != StreamState::primed) {
		return transitionError("start", "stream must be primed before starting");
	}
	if (hasPlayback(configuration_.direction)
	    && playbackRing_.availableRead() < configuration_.playbackPrefillFrames) {
		return makeError(StreamErrorCode::insufficientPrefill, "start",
		    "playback ring has not reached the required prefill", true);
	}
	if (StreamOperationResult result = adapter_->start()) {
		state_ = StreamState::faulted;
		markAdapterFault(*callbackState_);
		return result;
	}
	callbackState_->stopRequested.store(false, std::memory_order_release);
	callbackState_->startCount.fetch_add(1, std::memory_order_relaxed);
	state_ = StreamState::running;
	return std::nullopt;
}

StreamOperationResult
AudioStream::requestStop()
{
	if (state_ == StreamState::closed || state_ == StreamState::stopping) {
		return std::nullopt;
	}
	if (state_ != StreamState::running && state_ != StreamState::primed
	    && state_ != StreamState::opened && state_ != StreamState::faulted) {
		return transitionError("request-stop", "stream cannot stop from its current state");
	}
	callbackState_->stopRequested.store(true, std::memory_order_release);
	if (StreamOperationResult result = adapter_->requestStop()) {
		state_ = StreamState::faulted;
		markAdapterFault(*callbackState_);
		return result;
	}
	state_ = StreamState::stopping;
	return std::nullopt;
}

StreamOperationResult
AudioStream::stop()
{
	if (state_ == StreamState::closed || state_ == StreamState::opened
	    || state_ == StreamState::primed) {
		return std::nullopt;
	}
	if (state_ == StreamState::running) {
		if (StreamOperationResult result = requestStop()) {
			return result;
		}
	}
	if (state_ != StreamState::stopping && state_ != StreamState::faulted) {
		return transitionError("stop", "stream cannot stop from its current state");
	}
	if (StreamOperationResult result = adapter_->stop()) {
		state_ = StreamState::faulted;
		markAdapterFault(*callbackState_);
		return result;
	}
	callbackState_->stopCount.fetch_add(1, std::memory_order_relaxed);
	state_ = StreamState::opened;
	return std::nullopt;
}

StreamOperationResult
AudioStream::close()
{
	if (state_ == StreamState::closed) {
		return std::nullopt;
	}
	StreamOperationResult firstError;
	if (state_ == StreamState::running || state_ == StreamState::stopping
	    || state_ == StreamState::faulted) {
		firstError = requestStop();
		if (StreamOperationResult stopResult = stop(); !firstError && stopResult) {
			firstError = std::move(stopResult);
		}
	}
	if (StreamOperationResult result = adapter_->close()) {
		state_ = StreamState::faulted;
		markAdapterFault(*callbackState_);
		if (!firstError) {
			firstError = std::move(result);
		}
	}
	if (!firstError) {
		state_ = StreamState::closed;
	}
	return firstError;
}

StreamOperationResult
AudioStream::resetRings()
{
	if (state_ != StreamState::closed && state_ != StreamState::opened
	    && state_ != StreamState::primed) {
		return transitionError("reset-rings",
		    "audio rings can reset only while the callback is stopped");
	}
	playbackRing_.reset();
	captureRing_.reset();
	return std::nullopt;
}

std::size_t
AudioStream::queuePlayback(const std::span<const float> samples) noexcept
{
	if (!callbackState_->hasPlayback) {
		return 0;
	}
	const std::size_t written = playbackRing_.push(samples);
	updateHighWater(callbackState_->playbackRingHighWater,
	    playbackRing_.availableRead());
	return written;
}

std::size_t
AudioStream::readCapture(const std::span<float> samples) noexcept
{
	return callbackState_->hasCapture ? captureRing_.pop(samples) : 0;
}

StreamState
AudioStream::poll() noexcept
{
	const auto reason = static_cast<StreamFaultReason>(
	    callbackState_->faultReason.load(std::memory_order_acquire));
	if (reason != StreamFaultReason::none && state_ != StreamState::faulted
	    && state_ != StreamState::closed) {
		state_ = StreamState::faulted;
		callbackState_->faultCount.fetch_add(1, std::memory_order_relaxed);
	}
	return state_;
}

StreamState
AudioStream::state() const noexcept
{
	return state_;
}

AudioStreamStatistics
AudioStream::statistics() const
{
	const CallbackState& state = *callbackState_;
	return {state_,
	    state.callbacksExecuted.load(std::memory_order_relaxed),
	    state.playbackFramesRequested.load(std::memory_order_relaxed),
	    state.playbackFramesDelivered.load(std::memory_order_relaxed),
	    state.playbackFramesZeroFilled.load(std::memory_order_relaxed),
	    state.underrunCallbacks.load(std::memory_order_relaxed),
	    state.underrunFrames.load(std::memory_order_relaxed),
	    state.captureFramesReceived.load(std::memory_order_relaxed),
	    state.captureFramesWritten.load(std::memory_order_relaxed),
	    state.captureFramesDropped.load(std::memory_order_relaxed),
	    state.overrunCallbacks.load(std::memory_order_relaxed),
	    state.overrunFrames.load(std::memory_order_relaxed),
	    state.discontinuityGeneration.load(std::memory_order_relaxed),
	    playbackRing_.availableRead(), captureRing_.availableRead(),
	    static_cast<std::size_t>(state.playbackRingHighWater.load(std::memory_order_relaxed)),
	    static_cast<std::size_t>(state.captureRingHighWater.load(std::memory_order_relaxed)),
	    state.startCount.load(std::memory_order_relaxed),
	    state.stopCount.load(std::memory_order_relaxed),
	    state.faultCount.load(std::memory_order_relaxed),
	    static_cast<StreamFaultReason>(state.faultReason.load(std::memory_order_relaxed)),
	    negotiated_};
}

const AudioStreamConfiguration&
AudioStream::configuration() const noexcept
{
	return configuration_;
}

void
AudioStream::processCallback(void* const opaqueState, float* const output,
    const float* const input, const std::uint32_t frameCount) noexcept
{
	auto& state = *static_cast<CallbackState*>(opaqueState);
	state.activeCallbacks.fetch_add(1, std::memory_order_acq_rel);
	state.callbacksExecuted.fetch_add(1, std::memory_order_relaxed);
	if (state.hasPlayback) {
		state.playbackFramesRequested.fetch_add(frameCount, std::memory_order_relaxed);
		if (output != nullptr) {
			for (std::uint32_t frame = 0; frame < frameCount; ++frame) {
				for (std::uint32_t channel = 0; channel < state.playbackChannels; ++channel) {
					output[static_cast<std::size_t>(frame) * state.playbackChannels + channel]
					    = 0.0F;
				}
			}
		}
		std::uint32_t processed = 0;
		std::uint64_t delivered = 0;
		while (output != nullptr && processed < frameCount) {
			const std::size_t chunk = std::min<std::size_t>(
			    frameCount - processed, state.playbackScratch.size());
			const std::size_t popped = state.playbackRing->pop(
			    std::span<float>(state.playbackScratch.data(), chunk));
			for (std::size_t frame = 0; frame < popped; ++frame) {
				const std::size_t outputFrame = processed + frame;
				if (state.playbackMapping == PlaybackMappingPolicy::duplicateAllChannels) {
					for (std::uint32_t channel = 0;
					     channel < state.playbackChannels; ++channel) {
						output[outputFrame * state.playbackChannels + channel]
						    = state.playbackScratch[frame];
					}
				} else {
					output[outputFrame * state.playbackChannels
					    + state.selectedPlaybackChannel] = state.playbackScratch[frame];
				}
			}
			delivered += popped;
			processed += static_cast<std::uint32_t>(chunk);
			if (popped < chunk) {
				break;
			}
		}
		const std::uint64_t zeroFilled = frameCount - delivered;
		state.playbackFramesDelivered.fetch_add(delivered, std::memory_order_relaxed);
		state.playbackFramesZeroFilled.fetch_add(zeroFilled, std::memory_order_relaxed);
		if (zeroFilled != 0) {
			state.underrunCallbacks.fetch_add(1, std::memory_order_relaxed);
			state.underrunFrames.fetch_add(zeroFilled, std::memory_order_relaxed);
			state.discontinuityGeneration.fetch_add(1, std::memory_order_relaxed);
		}
	}
	if (state.hasCapture) {
		state.captureFramesReceived.fetch_add(frameCount, std::memory_order_relaxed);
		std::uint32_t processed = 0;
		std::uint64_t written = 0;
		while (input != nullptr && processed < frameCount) {
			const std::size_t chunk = std::min<std::size_t>(
			    frameCount - processed, state.captureScratch.size());
			for (std::size_t frame = 0; frame < chunk; ++frame) {
				state.captureScratch[frame] = input[(processed + frame)
				    * state.captureChannels + state.selectedCaptureChannel];
			}
			written += state.captureRing->push(
			    std::span<const float>(state.captureScratch.data(), chunk));
			processed += static_cast<std::uint32_t>(chunk);
		}
		const std::uint64_t dropped = frameCount - written;
		state.captureFramesWritten.fetch_add(written, std::memory_order_relaxed);
		state.captureFramesDropped.fetch_add(dropped, std::memory_order_relaxed);
		updateHighWater(state.captureRingHighWater, state.captureRing->availableRead());
		if (dropped != 0) {
			state.overrunCallbacks.fetch_add(1, std::memory_order_relaxed);
			state.overrunFrames.fetch_add(dropped, std::memory_order_relaxed);
			state.discontinuityGeneration.fetch_add(1, std::memory_order_relaxed);
		}
		if (input == nullptr && frameCount != 0) {
			notifyCallbackFault(&state, StreamFaultReason::callbackContract);
		}
	}
	state.activeCallbacks.fetch_sub(1, std::memory_order_release);
}

void
AudioStream::notifyCallbackFault(void* const opaqueState,
    const StreamFaultReason reason) noexcept
{
	auto& state = *static_cast<CallbackState*>(opaqueState);
	std::uint32_t expected = static_cast<std::uint32_t>(StreamFaultReason::none);
	(void)state.faultReason.compare_exchange_strong(expected,
	    static_cast<std::uint32_t>(reason), std::memory_order_release,
	    std::memory_order_relaxed);
	state.discontinuityGeneration.fetch_add(1, std::memory_order_relaxed);
}

AudioStreamCreateResult
createAudioStream(AudioStreamConfiguration configuration,
    std::unique_ptr<AudioStreamAdapter> adapter)
{
	if (StreamOperationResult validation
	    = validateConfiguration(configuration, adapter.get())) {
		return *validation;
	}
	try {
		return std::unique_ptr<AudioStream>(
		    new AudioStream(std::move(configuration), std::move(adapter)));
	} catch (const std::length_error& error) {
		return makeError(StreamErrorCode::resourceLimit, "create", error.what());
	} catch (const std::exception& error) {
		return makeError(StreamErrorCode::adapterFailure, "create", error.what());
	}
}

} // namespace sstv::audio
