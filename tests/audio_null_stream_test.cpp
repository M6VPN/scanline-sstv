// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/audio_null_stream_test.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/audio/audio_stream.hpp>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace {

using namespace sstv::audio;

void
require(const bool condition, const std::string& message)
{
	if (!condition) {
		throw std::runtime_error(message);
	}
}

[[nodiscard]] AudioDevice
makeNullDevice(const AudioDirection direction)
{
	return {{AudioBackend::nullDiagnostic, direction, "0", IdentityStability::sessionOnly},
	    "miniaudio null", false, AudioTransport::virtualDevice, {}, false, {}};
}

} // namespace

int
main()
{
	AudioDiscoverySnapshot snapshot{1,
	    {{AudioBackend::nullDiagnostic, "null", true, BackendStatus::available,
	        {makeNullDevice(AudioDirection::playback),
	            makeNullDevice(AudioDirection::capture)}, {}}}, false};
	AudioStreamConfiguration configuration;
	configuration.direction = StreamDirection::duplex;
	configuration.playbackDevice = StreamDeviceSelection{
	    makeNullDevice(AudioDirection::playback).identity, 1};
	configuration.captureDevice = StreamDeviceSelection{
	    makeNullDevice(AudioDirection::capture).identity, 1};
	configuration.playbackChannels = 1;
	configuration.captureChannels = 1;
	configuration.periodFrames = 48;
	configuration.periodCount = 2;
	configuration.playbackPrefillFrames = 96;
	configuration.playbackRingCapacity = 512;
	configuration.captureRingCapacity = 512;
	AudioStreamCreateResult created = createAudioStream(
	    configuration, createMiniaudioStreamAdapter());
	require(std::holds_alternative<std::unique_ptr<AudioStream>>(created),
	    "null stream construction failed");
	std::unique_ptr<AudioStream> stream
	    = std::move(std::get<std::unique_ptr<AudioStream>>(created));
	require(!stream->open(snapshot), "null device open failed");
	require(!stream->prime(), "null device prime failed");
	std::vector<float> samples(144);
	for (std::size_t index = 0; index < samples.size(); ++index) {
		samples[index] = static_cast<float>(index) / 144.0F;
	}
	require(stream->queuePlayback(samples) == samples.size(),
	    "null playback prefill failed");
	require(!stream->start(), "null device start failed");
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
	AudioStreamStatistics statistics;
	do {
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		statistics = stream->statistics();
	} while ((statistics.playbackFramesDelivered < samples.size()
	    || statistics.playbackFramesZeroFilled == 0
	    || statistics.captureFramesReceived == 0)
	    && std::chrono::steady_clock::now() < deadline);
	require(statistics.playbackFramesDelivered == samples.size(),
	    "null backend did not consume the ordered playback ring exactly once");
	require(statistics.playbackFramesZeroFilled != 0
	    && statistics.underrunFrames == statistics.playbackFramesZeroFilled,
	    "null backend underrun was not silenced and counted");
	require(statistics.captureFramesReceived != 0,
	    "null backend did not exercise capture callbacks");
	std::vector<float> captured(statistics.captureRingFill);
	const std::size_t capturedCount = stream->readCapture(captured);
	require(capturedCount != 0, "null backend captured no diagnostic samples");
	for (std::size_t index = 0; index < capturedCount; ++index) {
		require(captured[index] == 0.0F,
		    "null backend capture produced a non-silent sample");
	}
	require(!stream->requestStop(), "null stop request failed");
	require(!stream->stop(), "null device stop failed");
	require(!stream->close(), "null device close failed");
	require(stream->state() == StreamState::closed,
	    "null device did not reach closed state");
	return 0;
}
