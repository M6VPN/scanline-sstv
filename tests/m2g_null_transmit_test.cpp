// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m2g_null_transmit_test.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/app/rendered_transmit.hpp>
#include <sstv/audio/audio_discovery.hpp>
#include <sstv/rig/ptt.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace sstv;

void
require(const bool condition, const std::string& message)
{
	if (!condition) throw std::runtime_error(message);
}

class NullTestPttProvider final : public rig::PttProvider {
public:
	[[nodiscard]] rig::PttOperationResult execute(
		const rig::PttRequest& request) noexcept override
	{
		operations.push_back(request.action);
		if (request.action == rig::PttAction::key) {
			return {request.action, rig::PttObservedState::keyed,
				rig::PttCertainty::definitelyKeyed, rig::PttReadback::available,
				rig::PttErrorCategory::none, false, true, request.attempt,
				{}, {}, {}, {}, request.operationId};
		}
		return {request.action, rig::PttObservedState::unkeyed,
			rig::PttCertainty::definitelyUnkeyed, rig::PttReadback::available,
			rig::PttErrorCategory::none, false, false, request.attempt,
			{}, {}, {}, {}, request.operationId};
	}
	std::vector<rig::PttAction> operations;
};

[[nodiscard]] analog::OfflineTransmission
makeTransmission()
{
	std::vector<core::ToneEvent> events;
	events.emplace_back(core::Duration::fromMicroseconds(25'000), 1'200.0, 0.5F);
	events.emplace_back(core::Duration::fromMicroseconds(25'000), 1'900.0, 0.8F);
	return {std::move(events), core::Duration::fromMicroseconds(50'000)};
}

} // namespace

int
main()
{
	auto discoveryProvider = audio::createMiniaudioDiscoveryProvider();
	audio::BackendDiscovery backend = discoveryProvider->discoverBackend(
		audio::AudioBackend::nullDiagnostic, {});
	require(backend.status == audio::BackendStatus::available,
	    "null backend discovery failed");
	auto snapshot = std::make_shared<const audio::AudioDiscoverySnapshot>(
		audio::AudioDiscoverySnapshot{1, {std::move(backend)}, false});
	require(snapshot->backends.size() == 1
	    && snapshot->backends.front().backend == audio::AudioBackend::nullDiagnostic,
	    "null test discovered a non-null backend");
	const audio::AudioDevice* playback = nullptr;
	for (const audio::AudioDevice& device : snapshot->backends.front().devices) {
		if (device.identity.direction == audio::AudioDirection::playback) {
			playback = &device;
			break;
		}
	}
	require(playback != nullptr, "null backend has no playback identity");
	audio::AudioStreamConfiguration configuration;
	configuration.direction = audio::StreamDirection::playback;
	configuration.playbackDevice = audio::StreamDeviceSelection{
		playback->identity, snapshot->generation};
	configuration.playbackChannels = 1;
	configuration.selectedPlaybackChannel = 0;
	configuration.periodFrames = 48;
	configuration.periodCount = 2;
	configuration.playbackPrefillFrames = 480;
	configuration.playbackRingCapacity = 4'800;
	app::TransmitAudioEndpointCreateResult endpointResult
		= app::createAudioStreamTransmitEndpoint(
			{configuration, snapshot}, audio::createMiniaudioStreamAdapter());
	require(std::holds_alternative<std::unique_ptr<app::TransmitAudioEndpoint>>(
	        endpointResult), "null transmit endpoint creation failed");
	auto endpoint = std::get<std::unique_ptr<app::TransmitAudioEndpoint>>(
		std::move(endpointResult));
	app::FiniteSampleSourceCreateResult sourceResult
		= app::createToneEventSampleSource(makeTransmission(), 48'000);
	require(std::holds_alternative<std::unique_ptr<app::FiniteSampleSource>>(
	        sourceResult), "null test rendered source creation failed");
	auto source = std::get<std::unique_ptr<app::FiniteSampleSource>>(
		std::move(sourceResult));
	auto clock = rig::createSteadyMonotonicClock();
	auto scheduler = rig::createSteadyMonotonicScheduler(clock);
	auto pttProvider = std::make_shared<NullTestPttProvider>();
	auto supervisor = std::make_shared<rig::PttSupervisor>(pttProvider, clock);
	app::TransmitCoordinator coordinator(supervisor, clock, scheduler);
	app::TransmitRequest request;
	request.prefillFrames = 480;
	request.blockFrames = 127;
	request.policy.preKeyDelay = std::chrono::milliseconds(0);
	request.policy.postAudioTail = std::chrono::milliseconds(0);
	request.policy.heartbeatInterval = std::chrono::milliseconds(20);
	request.policy.watchdogLease = std::chrono::milliseconds(200);
	const app::TransmitRunResult result = coordinator.run(
		request, std::move(source), std::move(endpoint));
	require(result->outcome == app::TransmitOutcome::completed,
	    "null rendered transmit lifecycle did not complete");
	require(result->submittedFrames == 2'400 && result->sourceFrames == 2'400,
	    "null rendered transmit frame count changed");
	require(pttProvider->operations == std::vector<rig::PttAction>({
		rig::PttAction::query, rig::PttAction::key, rig::PttAction::unkey}),
	    "null test mock PTT sequence changed");
	return 0;
}
