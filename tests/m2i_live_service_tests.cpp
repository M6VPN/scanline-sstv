// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m2i_live_service_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/app/live_transmit.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <variant>
#include <vector>

namespace {

void
require(const bool condition, const std::string& message)
{
	if (!condition) throw std::runtime_error(message);
}

class FailingDiscoveryProvider final : public sstv::audio::AudioDiscoveryProvider {
public:
	[[nodiscard]] bool isBackendCompiled(const sstv::audio::AudioBackend) const override
	{
		return true;
	}
	[[nodiscard]] sstv::audio::BackendDiscovery discoverBackend(
		const sstv::audio::AudioBackend backend, const std::stop_token) override
	{
		return {backend, std::string(sstv::audio::audioBackendApiName(backend)), true,
			sstv::audio::BackendStatus::unavailable, {}, "injected discovery failure"};
	}
};

class UnusedPttProvider final : public sstv::rig::PttProvider {
public:
	[[nodiscard]] sstv::rig::PttOperationResult execute(
		const sstv::rig::PttRequest&) noexcept override
	{
		++calls;
		return {};
	}
	std::atomic<std::size_t> calls{0};
};

class CountingRuntime final : public sstv::app::LiveTransmitRuntime {
public:
	[[nodiscard]] std::shared_ptr<sstv::audio::AudioDiscoveryProvider>
	createDiscoveryProvider() override
	{
		++discoveryCreations;
		return std::make_shared<FailingDiscoveryProvider>();
	}
	[[nodiscard]] std::unique_ptr<sstv::audio::AudioStreamAdapter>
	createAudioAdapter() override
	{
		++audioCreations;
		return nullptr;
	}
	[[nodiscard]] std::shared_ptr<sstv::rig::MonotonicClock> createClock() override
	{
		++clockCreations;
		return sstv::rig::createSteadyMonotonicClock();
	}
	[[nodiscard]] std::shared_ptr<sstv::rig::MonotonicScheduler> createScheduler(
		std::shared_ptr<sstv::rig::MonotonicClock> clock) override
	{
		return sstv::rig::createSteadyMonotonicScheduler(std::move(clock));
	}
	[[nodiscard]] std::variant<std::shared_ptr<sstv::rig::PttProvider>,
		sstv::app::LiveTransmitError> createPttProvider(
		const sstv::app::LivePttConfiguration&,
		std::shared_ptr<sstv::rig::MonotonicClock>) override
	{
		++pttCreations;
		return std::shared_ptr<sstv::rig::PttProvider>(provider);
	}
	std::shared_ptr<UnusedPttProvider> provider = std::make_shared<UnusedPttProvider>();
	std::atomic<std::size_t> discoveryCreations{0};
	std::atomic<std::size_t> audioCreations{0};
	std::atomic<std::size_t> clockCreations{0};
	std::atomic<std::size_t> pttCreations{0};
};

[[nodiscard]] sstv::app::LiveTransmitPrepared
prepare()
{
	sstv::image::SourceImageInfo source{sstv::image::RasterFormat::png,
		1, 1, 1, 1, sstv::image::ExifOrientation::identity, 8, false, false};
	sstv::image::PreparedImage image{sstv::core::Rgb8Frame(1, 1,
		std::vector<sstv::core::Rgb8Pixel>{{0, 0, 0}}), source,
		std::nullopt, sstv::image::FitMode::contain};
	const sstv::core::Duration duration
		= sstv::core::Duration::fromMicroseconds(10'000);
	sstv::analog::OfflineTransmission transmission{{
		sstv::core::ToneEvent(duration, 1'000.0, 0.8F)}, duration};
	auto snapshot = std::make_shared<const sstv::app::OfflineEditorSnapshot>(
		sstv::app::OfflineEditorSnapshot{91,
			{"test", "Test", "RGB", 1, 1, std::nullopt, duration}, {},
			std::move(image), std::move(transmission), 48'000, 480, 1'004,
			std::nullopt});
	const auto result = sstv::app::prepareLiveTransmitSnapshot(snapshot, -30.0);
	require(std::holds_alternative<sstv::app::LiveTransmitPrepared>(result),
		"live preparation failed");
	return std::get<sstv::app::LiveTransmitPrepared>(result);
}

[[nodiscard]] sstv::app::LiveTransmitConfiguration
configuration()
{
	sstv::app::LiveTransmitConfiguration value;
	value.revision = 91;
	value.playback = {sstv::audio::AudioBackend::alsa, "hex:0102", 2, 0};
	value.ptt = {sstv::app::LivePttProviderKind::flrig, "127.0.0.1", 12'345,
		std::string("/RPC2")};
	value.preKeyDelay = std::chrono::milliseconds(250);
	value.postAudioTail = std::chrono::milliseconds(250);
	return value;
}

void
testConfirmationAndAcquisitionGate()
{
	auto runtime = std::make_shared<CountingRuntime>();
	sstv::app::LiveTransmitService service(runtime);
	require(!service.configure(prepare(), configuration()),
		"valid service configuration failed");
	require(runtime->discoveryCreations == 0 && runtime->audioCreations == 0
		&& runtime->clockCreations == 0 && runtime->pttCreations == 0,
		"configuration acquired live resources");
	const auto rejected = service.confirm({91, true, true, false,
		std::string(sstv::app::liveTransmitConfirmationPhrase)});
	require(std::holds_alternative<sstv::app::LiveTransmitError>(rejected),
		"missing arm was accepted");
	require(runtime->discoveryCreations == 0 && runtime->pttCreations == 0,
		"rejected confirmation acquired resources");
	const auto stale = service.confirm({92, true, true, true,
		std::string(sstv::app::liveTransmitConfirmationPhrase)});
	require(std::holds_alternative<sstv::app::LiveTransmitError>(stale),
		"stale revision confirmation was accepted");
	const auto accepted = service.confirm({91, true, true, true,
		std::string(sstv::app::liveTransmitConfirmationPhrase)});
	require(std::holds_alternative<sstv::app::LiveTransmitConfirmation>(accepted),
		"valid confirmation was rejected");
	const auto token = std::get<sstv::app::LiveTransmitConfirmation>(accepted);
	require(!service.start(token), "confirmed start was rejected");
	for (std::size_t attempt = 0; attempt < 200; ++attempt) {
		const auto snapshot = service.snapshot();
		if (snapshot->state == sstv::app::LiveServiceState::faulted) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
	const auto finalSnapshot = service.shutdown();
	require(finalSnapshot->state == sstv::app::LiveServiceState::faulted,
		"injected discovery failure did not terminate safely");
	require(runtime->clockCreations == 1 && runtime->pttCreations == 1
		&& runtime->discoveryCreations == 1 && runtime->audioCreations == 0,
		"resource ordering or failure boundary changed");
	require(runtime->provider->calls == 0,
		"PTT was contacted after discovery failed");
	require(service.start(token).has_value(), "single-use confirmation was reused");
}

void
testInvalidConfiguration()
{
	auto runtime = std::make_shared<CountingRuntime>();
	sstv::app::LiveTransmitService service(runtime);
	auto value = configuration();
	value.ptt.address = "localhost";
	require(service.configure(prepare(), value).has_value(),
		"hostname PTT endpoint was accepted");
	value = configuration();
	value.revision = 92;
	require(service.configure(prepare(), value).has_value(),
		"stale preparation revision was accepted");
	require(runtime->clockCreations == 0 && runtime->discoveryCreations == 0,
		"invalid configuration acquired resources");
}

} // namespace

int
main()
{
	testConfirmationAndAcquisitionGate();
	testInvalidConfiguration();
	return 0;
}
