// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/audio/miniaudio_stream_adapter.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "miniaudio_adapter.hpp"

#include <sstv/audio/audio_stream.hpp>

#include <miniaudio.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace sstv::audio {
namespace {

[[nodiscard]] StreamError
adapterError(std::string operation, const ma_result result,
    const bool isRecoverable = true)
{
	return {StreamErrorCode::adapterFailure, std::move(operation),
	    ma_result_description(result), isRecoverable};
}

[[nodiscard]] ma_device_type
toDeviceType(const StreamDirection direction) noexcept
{
	switch (direction) {
	case StreamDirection::playback: return ma_device_type_playback;
	case StreamDirection::capture: return ma_device_type_capture;
	case StreamDirection::duplex: return ma_device_type_duplex;
	}
	return ma_device_type_playback;
}

[[nodiscard]] SampleFormat
toProjectFormat(const ma_format format) noexcept
{
	switch (format) {
	case ma_format_u8: return SampleFormat::unsigned8;
	case ma_format_s16: return SampleFormat::signed16;
	case ma_format_s24: return SampleFormat::signed24;
	case ma_format_s32: return SampleFormat::signed32;
	case ma_format_f32: return SampleFormat::float32;
	default: return SampleFormat::unknown;
	}
}

class MiniaudioStreamAdapter final : public AudioStreamAdapter {
public:
	~MiniaudioStreamAdapter() override
	{
		(void)close();
	}

	[[nodiscard]] AdapterOpenResult open(const AudioStreamConfiguration& configuration,
	    const AudioCallbackBinding& callback, const std::stop_token stopToken) noexcept override
	{
		if (isContextInitialized_ || isDeviceInitialized_) {
			return StreamError{StreamErrorCode::invalidTransition, "adapter-open",
			    "miniaudio adapter is already open", true};
		}
		if (stopToken.stop_requested()) {
			return StreamError{StreamErrorCode::cancelled, "adapter-open",
			    "miniaudio adapter opening was cancelled", true};
		}
		callback_ = callback;
		backend_ = configuration.playbackDevice
		    ? configuration.playbackDevice->identity.backend
		    : configuration.captureDevice->identity.backend;
		const std::optional<ma_backend> backend = detail::toMiniaudioBackend(backend_);
		if (!backend) {
			return StreamError{StreamErrorCode::invalidIdentity, "adapter-open",
			    "selected backend has no miniaudio mapping", false};
		}
		if (configuration.playbackDevice
		    && !detail::deserializeDeviceId(backend_,
		        configuration.playbackDevice->identity.opaque, playbackId_)) {
			return StreamError{StreamErrorCode::invalidIdentity, "adapter-open",
			    "playback device identity encoding is invalid", false};
		}
		if (configuration.captureDevice
		    && !detail::deserializeDeviceId(backend_,
		        configuration.captureDevice->identity.opaque, captureId_)) {
			return StreamError{StreamErrorCode::invalidIdentity, "adapter-open",
			    "capture device identity encoding is invalid", false};
		}
		ma_context_config contextConfiguration = ma_context_config_init();
		contextConfiguration.pulse.pApplicationName = "Scanline SSTV stream";
		contextConfiguration.pulse.tryAutoSpawn = MA_FALSE;
		contextConfiguration.jack.pClientName = "scanline-sstv-stream";
		contextConfiguration.jack.tryStartServer = MA_FALSE;
		const ma_result contextResult = ma_context_init(
		    &*backend, 1, &contextConfiguration, &context_);
		if (contextResult != MA_SUCCESS) {
			return adapterError("context-open", contextResult);
		}
		isContextInitialized_ = true;
		ma_device_config deviceConfiguration = ma_device_config_init(
		    toDeviceType(configuration.direction));
		deviceConfiguration.sampleRate = configuration.sampleRate;
		deviceConfiguration.periodSizeInFrames = configuration.periodFrames;
		deviceConfiguration.periods = configuration.periodCount;
		deviceConfiguration.noPreSilencedOutputBuffer = MA_TRUE;
		deviceConfiguration.noClip = MA_TRUE;
		deviceConfiguration.noFixedSizedCallback = MA_TRUE;
		deviceConfiguration.dataCallback = dataCallback;
		deviceConfiguration.notificationCallback = notificationCallback;
		deviceConfiguration.pUserData = this;
		if (configuration.playbackDevice) {
			deviceConfiguration.playback.pDeviceID = &playbackId_;
			deviceConfiguration.playback.format = ma_format_f32;
			deviceConfiguration.playback.channels = configuration.playbackChannels;
		}
		if (configuration.captureDevice) {
			deviceConfiguration.capture.pDeviceID = &captureId_;
			deviceConfiguration.capture.format = ma_format_f32;
			deviceConfiguration.capture.channels = configuration.captureChannels;
		}
		const ma_result deviceResult = ma_device_init(
		    &context_, &deviceConfiguration, &device_);
		if (deviceResult != MA_SUCCESS) {
			(void)ma_context_uninit(&context_);
			isContextInitialized_ = false;
			return adapterError("device-open", deviceResult);
		}
		isDeviceInitialized_ = true;
		NegotiatedStreamFacts facts;
		facts.backend = backend_;
		facts.callbackSampleRate = device_.sampleRate;
		facts.periodFrames = configuration.periodFrames;
		facts.periodCount = configuration.periodCount;
		if (configuration.playbackDevice) {
			facts.playback = NegotiatedEndpointFacts{toProjectFormat(device_.playback.format),
			    device_.playback.channels, toProjectFormat(device_.playback.internalFormat),
			    device_.playback.internalChannels, device_.playback.internalSampleRate};
			facts.periodFrames = device_.playback.internalPeriodSizeInFrames != 0
			    ? device_.playback.internalPeriodSizeInFrames : facts.periodFrames;
			facts.periodCount = device_.playback.internalPeriods != 0
			    ? device_.playback.internalPeriods : facts.periodCount;
		}
		if (configuration.captureDevice) {
			facts.capture = NegotiatedEndpointFacts{toProjectFormat(device_.capture.format),
			    device_.capture.channels, toProjectFormat(device_.capture.internalFormat),
			    device_.capture.internalChannels, device_.capture.internalSampleRate};
			if (!configuration.playbackDevice) {
				facts.periodFrames = device_.capture.internalPeriodSizeInFrames != 0
				    ? device_.capture.internalPeriodSizeInFrames : facts.periodFrames;
				facts.periodCount = device_.capture.internalPeriods != 0
				    ? device_.capture.internalPeriods : facts.periodCount;
			}
		}
		return facts;
	}

	[[nodiscard]] StreamOperationResult prime() noexcept override
	{
		if (!isDeviceInitialized_) {
			return StreamError{StreamErrorCode::invalidTransition, "adapter-prime",
			    "miniaudio device is not open", true};
		}
		return std::nullopt;
	}

	[[nodiscard]] StreamOperationResult start() noexcept override
	{
		if (!isDeviceInitialized_ || isStarted_) {
			return StreamError{StreamErrorCode::invalidTransition, "adapter-start",
			    "miniaudio device is not open or is already started", true};
		}
		isStopRequested_.store(false, std::memory_order_release);
		const ma_result result = ma_device_start(&device_);
		if (result != MA_SUCCESS) {
			return adapterError("device-start", result);
		}
		isStarted_ = true;
		return std::nullopt;
	}

	[[nodiscard]] StreamOperationResult requestStop() noexcept override
	{
		isStopRequested_.store(true, std::memory_order_release);
		return std::nullopt;
	}

	[[nodiscard]] StreamOperationResult stop() noexcept override
	{
		isStopRequested_.store(true, std::memory_order_release);
		if (!isDeviceInitialized_ || !isStarted_) {
			return std::nullopt;
		}
		const ma_result result = ma_device_stop(&device_);
		if (result != MA_SUCCESS) {
			return adapterError("device-stop", result);
		}
		isStarted_ = false;
		return std::nullopt;
	}

	[[nodiscard]] StreamOperationResult close() noexcept override
	{
		StreamOperationResult stopResult = stop();
		if (isDeviceInitialized_) {
			ma_device_uninit(&device_);
			isDeviceInitialized_ = false;
		}
		if (isContextInitialized_) {
			(void)ma_context_uninit(&context_);
			isContextInitialized_ = false;
		}
		callback_ = {};
		return stopResult;
	}

private:
	static void dataCallback(ma_device* const device, void* const output,
	    const void* const input, const ma_uint32 frameCount) noexcept
	{
		auto& adapter = *static_cast<MiniaudioStreamAdapter*>(device->pUserData);
		if (adapter.callback_.process != nullptr) {
			adapter.callback_.process(adapter.callback_.state,
			    static_cast<float*>(output), static_cast<const float*>(input), frameCount);
		}
	}

	static void notificationCallback(
	    const ma_device_notification* const notification) noexcept
	{
		auto& adapter = *static_cast<MiniaudioStreamAdapter*>(
		    notification->pDevice->pUserData);
		if (adapter.callback_.notifyFault == nullptr) {
			return;
		}
		if (notification->type == ma_device_notification_type_rerouted) {
			adapter.callback_.notifyFault(
			    adapter.callback_.state, StreamFaultReason::deviceRemoved);
		} else if (notification->type == ma_device_notification_type_interruption_began
		    || (notification->type == ma_device_notification_type_stopped
		        && !adapter.isStopRequested_.load(std::memory_order_acquire))) {
			adapter.callback_.notifyFault(
			    adapter.callback_.state, StreamFaultReason::backendDisconnect);
		}
	}

	AudioCallbackBinding callback_{};
	AudioBackend backend_ = AudioBackend::nullDiagnostic;
	ma_context context_{};
	ma_device device_{};
	ma_device_id playbackId_{};
	ma_device_id captureId_{};
	std::atomic<bool> isStopRequested_{false};
	bool isContextInitialized_ = false;
	bool isDeviceInitialized_ = false;
	bool isStarted_ = false;
};

} // namespace

std::unique_ptr<AudioStreamAdapter>
createMiniaudioStreamAdapter()
{
	return std::make_unique<MiniaudioStreamAdapter>();
}

} // namespace sstv::audio
