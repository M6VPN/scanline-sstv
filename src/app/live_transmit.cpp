// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/app/live_transmit.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/app/live_transmit.hpp>
#include <sstv/rig/flrig.hpp>
#include <sstv/rig/rigctld.hpp>

#include <algorithm>
#include <cmath>
#include <exception>
#include <limits>
#include <stdexcept>
#include <utility>

namespace sstv::app {
namespace {

[[nodiscard]] LiveTransmitError
makeError(const LiveTransmitErrorCode code, std::string operation,
	std::string message)
{
	return {code, std::move(operation), std::move(message)};
}

class ProductionLiveTransmitRuntime final : public LiveTransmitRuntime {
public:
	[[nodiscard]] std::shared_ptr<audio::AudioDiscoveryProvider>
	createDiscoveryProvider() override
	{
		return audio::createMiniaudioDiscoveryProvider();
	}

	[[nodiscard]] std::unique_ptr<audio::AudioStreamAdapter>
	createAudioAdapter() override
	{
		return audio::createMiniaudioStreamAdapter();
	}

	[[nodiscard]] std::shared_ptr<rig::MonotonicClock> createClock() override
	{
		return rig::createSteadyMonotonicClock();
	}

	[[nodiscard]] std::shared_ptr<rig::MonotonicScheduler> createScheduler(
		std::shared_ptr<rig::MonotonicClock> clock) override
	{
		return rig::createSteadyMonotonicScheduler(std::move(clock));
	}

	[[nodiscard]] std::variant<std::shared_ptr<rig::PttProvider>, LiveTransmitError>
	createPttProvider(const LivePttConfiguration& configuration,
		std::shared_ptr<rig::MonotonicClock> clock) override
	{
		if (configuration.provider == LivePttProviderKind::flrig) {
			rig::FlrigConfiguration value;
			value.address = configuration.address;
			value.port = configuration.port;
			value.path = configuration.flrigPath.value_or("");
			if (const auto error = rig::validateFlrigConfiguration(value)) {
				return makeError(LiveTransmitErrorCode::invalidPtt,
					"validate flrig", error->message);
			}
			return std::shared_ptr<rig::PttProvider>(
				std::make_shared<rig::FlrigPttProvider>(std::move(value),
					std::move(clock)));
		}
		rig::RigctldConfiguration value;
		value.address = configuration.address;
		value.port = configuration.port;
		if (const auto error = rig::validateRigctldConfiguration(value)) {
			return makeError(LiveTransmitErrorCode::invalidPtt,
				"validate rigctld", error->message);
		}
		return std::shared_ptr<rig::PttProvider>(
			std::make_shared<rig::RigctldPttProvider>(std::move(value),
				std::move(clock)));
	}
};

[[nodiscard]] std::variant<LiveTransmitGain, LiveTransmitError>
validateGain(const double decibels,
	const std::span<const core::ToneEvent> events)
{
	if (!std::isfinite(decibels) || decibels < minimumLiveTransmitGainDbfs
		|| decibels > maximumLiveTransmitGainDbfs) {
		return makeError(LiveTransmitErrorCode::invalidGain, "validate gain",
			"transmit gain must be finite and within -60 to -6 dBFS");
	}
	const double calculated = std::pow(10.0, decibels / 20.0);
	if (!std::isfinite(calculated) || calculated <= 0.0
		|| calculated > static_cast<double>(std::numeric_limits<float>::max())) {
		return makeError(LiveTransmitErrorCode::invalidGain, "validate gain",
			"transmit gain cannot be represented safely");
	}
	const float scalar = static_cast<float>(calculated);
	for (const core::ToneEvent& event : events) {
		if (!std::isfinite(event.amplitude())
			|| std::abs(event.amplitude()) * scalar > 1.0F) {
			return makeError(LiveTransmitErrorCode::invalidGain, "validate gain",
				"transmit gain can produce clipped or non-finite samples");
		}
	}
	return LiveTransmitGain{decibels, scalar};
}

} // namespace

LiveTransmitPreparationResult
prepareLiveTransmit(const LiveTransmitPreparationRequest& request)
{
	SnapshotResult result = prepareOfflineEditor(request.editor);
	if (const auto* error = std::get_if<EditorError>(&result)) {
		return makeError(LiveTransmitErrorCode::invalidPreparation,
			error->operation, error->message);
	}
	auto snapshot = std::get<std::shared_ptr<const OfflineEditorSnapshot>>(
		std::move(result));
	return prepareLiveTransmitSnapshot(std::move(snapshot),
		request.gainDecibelsFullScale);
}

LiveTransmitPreparationResult
prepareLiveTransmitSnapshot(std::shared_ptr<const OfflineEditorSnapshot> snapshot,
	const double gainDecibelsFullScale)
{
	if (snapshot == nullptr) {
		return makeError(LiveTransmitErrorCode::invalidPreparation,
			"validate snapshot", "prepared live snapshot is missing");
	}
	auto gain = validateGain(gainDecibelsFullScale, snapshot->transmission.events);
	if (const auto* error = std::get_if<LiveTransmitError>(&gain)) return *error;
	if (snapshot->frameCount == 0
		|| snapshot->frameCount > maximumTransmitSourceFrames) {
		return makeError(LiveTransmitErrorCode::invalidPreparation,
			"validate frame count", "prepared transmission exceeds live limits");
	}
	return LiveTransmitPrepared{std::move(snapshot),
		std::get<LiveTransmitGain>(gain)};
}

LivePlaybackSelectionResult
selectLivePlaybackDevice(const audio::AudioDiscoverySnapshot& snapshot,
	const LivePlaybackSelectionRequest& request)
{
	if (!audio::isRealAudioBackend(request.backend)) {
		return makeError(LiveTransmitErrorCode::invalidDevice, "select device",
			"live transmission requires a real explicitly selected backend");
	}
	if (request.opaqueIdentity.empty() || request.playbackChannels == 0
		|| request.playbackChannels > audio::maximumAudioChannels
		|| request.selectedOutputChannel >= request.playbackChannels) {
		return makeError(LiveTransmitErrorCode::invalidDevice, "select device",
			"playback identity, channel count, or output channel is invalid");
	}
	const audio::AudioDevice* selected = nullptr;
	std::size_t matches = 0;
	for (const audio::BackendDiscovery& backend : snapshot.backends) {
		if (backend.backend != request.backend) continue;
		for (const audio::AudioDevice& device : backend.devices) {
			if (device.identity.direction != audio::AudioDirection::playback
				|| device.identity.opaque != request.opaqueIdentity) continue;
			selected = &device;
			++matches;
		}
	}
	if (matches == 0 || selected == nullptr) {
		return makeError(LiveTransmitErrorCode::deviceNotFound, "select device",
			"the exact playback identity was not found in the fresh snapshot");
	}
	if (matches != 1 || selected->hasIdentityCollision) {
		return makeError(LiveTransmitErrorCode::identityCollision, "select device",
			"the playback identity is duplicated or collision-marked");
	}
	audio::AudioStreamConfiguration configuration;
	configuration.direction = audio::StreamDirection::playback;
	configuration.playbackDevice = audio::StreamDeviceSelection{
		selected->identity, snapshot.generation};
	configuration.sampleRate = audio::nominalAudioSampleRate;
	configuration.playbackChannels = request.playbackChannels;
	configuration.selectedPlaybackChannel = request.selectedOutputChannel;
	configuration.playbackMapping = audio::PlaybackMappingPolicy::selectedChannel;
	configuration.periodFrames = 480;
	configuration.periodCount = 3;
	configuration.playbackPrefillFrames = 1'440;
	configuration.playbackRingCapacity = 4'800;
	return configuration;
}

FiniteSampleSourceCreateResult
createLiveTransmitSource(const LiveTransmitPrepared& prepared)
{
	if (prepared.snapshot == nullptr) {
		return SampleSourceError{"prepared live transmission is missing"};
	}
	return createToneEventSampleSource(prepared.snapshot->transmission,
		prepared.snapshot->sampleRate, prepared.gain.scalar);
}

std::shared_ptr<LiveTransmitRuntime>
createLiveTransmitRuntime()
{
	return std::make_shared<ProductionLiveTransmitRuntime>();
}

LiveTransmitService::LiveTransmitService(
	std::shared_ptr<LiveTransmitRuntime> runtime)
	: runtime_(std::move(runtime)),
	  snapshot_(std::make_shared<const LiveTransmitServiceSnapshot>())
{
	if (runtime_ == nullptr) {
		throw std::invalid_argument("live transmit runtime is required");
	}
}

LiveTransmitService::~LiveTransmitService()
{
	static_cast<void>(shutdown());
}

LiveTransmitServiceResult
LiveTransmitService::validateConfiguration(const LiveTransmitPrepared& prepared,
	const LiveTransmitConfiguration& configuration) const
{
	if (prepared.snapshot == nullptr || configuration.revision == 0
		|| prepared.snapshot->revision != configuration.revision) {
		return makeError(LiveTransmitErrorCode::staleRevision,
			"validate revision", "prepared and configuration revisions differ");
	}
	if (configuration.preKeyDelay.count() < 0
		|| configuration.postAudioTail.count() < 0
		|| static_cast<std::uint64_t>(configuration.preKeyDelay.count())
			> maximumLiveTransmitDelayMilliseconds
		|| static_cast<std::uint64_t>(configuration.postAudioTail.count())
			> maximumLiveTransmitDelayMilliseconds) {
		return makeError(LiveTransmitErrorCode::invalidDelay,
			"validate delays", "live transmit delays must be within 0 to 10000 ms");
	}
	if (!audio::isRealAudioBackend(configuration.playback.backend)
		|| configuration.playback.opaqueIdentity.empty()
		|| configuration.playback.playbackChannels == 0
		|| configuration.playback.playbackChannels > audio::maximumAudioChannels
		|| configuration.playback.selectedOutputChannel
			>= configuration.playback.playbackChannels) {
		return makeError(LiveTransmitErrorCode::invalidDevice,
			"validate playback", "exact playback selection is invalid");
	}
	if (configuration.ptt.provider == LivePttProviderKind::flrig) {
		rig::FlrigConfiguration value{configuration.ptt.address,
			configuration.ptt.port, configuration.ptt.flrigPath.value_or("")};
		if (const auto error = rig::validateFlrigConfiguration(value)) {
			return makeError(LiveTransmitErrorCode::invalidPtt,
				"validate flrig", error->message);
		}
	} else {
		if (configuration.ptt.flrigPath.has_value()) {
			return makeError(LiveTransmitErrorCode::invalidPtt,
				"validate rigctld", "flrig path is not valid for rigctld");
		}
		rig::RigctldConfiguration value;
		value.address = configuration.ptt.address;
		value.port = configuration.ptt.port;
		if (const auto error = rig::validateRigctldConfiguration(value)) {
			return makeError(LiveTransmitErrorCode::invalidPtt,
				"validate rigctld", error->message);
		}
	}
	return std::nullopt;
}

LiveTransmitServiceResult
LiveTransmitService::configure(LiveTransmitPrepared prepared,
	LiveTransmitConfiguration configuration)
{
	if (const auto error = validateConfiguration(prepared, configuration)) {
		return error;
	}
	std::lock_guard lock(mutex_);
	if (isRunning_.load(std::memory_order_acquire)) {
		return makeError(LiveTransmitErrorCode::concurrentOperation,
			"configure", "a live transmit operation is active");
	}
	if (snapshot_->hasUnresolvedPttHazard) {
		return makeError(LiveTransmitErrorCode::unresolvedHazard,
			"configure", "PTT remains in an unresolved hazardous state");
	}
	prepared_ = std::move(prepared);
	configuration_ = std::move(configuration);
	confirmation_.reset();
	auto value = std::make_shared<LiveTransmitServiceSnapshot>();
	value->sequence = nextSequence_++;
	value->revision = configuration_->revision;
	value->state = LiveServiceState::ready;
	value->prepared = prepared_->snapshot;
	snapshot_ = std::move(value);
	return std::nullopt;
}

std::variant<LiveTransmitConfirmation, LiveTransmitError>
LiveTransmitService::confirm(const LiveTransmitArming& arming)
{
	std::lock_guard lock(mutex_);
	if (!prepared_ || !configuration_ || arming.revision != configuration_->revision) {
		return makeError(LiveTransmitErrorCode::staleRevision,
			"confirm", "confirmation does not match the current revision");
	}
	if (isRunning_.load(std::memory_order_acquire)) {
		return makeError(LiveTransmitErrorCode::concurrentOperation,
			"confirm", "a live transmit operation is active");
	}
	if (snapshot_->hasUnresolvedPttHazard) {
		return makeError(LiveTransmitErrorCode::unresolvedHazard,
			"confirm", "PTT remains in an unresolved hazardous state");
	}
	if (!arming.hasRealAudioArm || !arming.hasAutomaticPttArm
		|| !arming.hasLiveTransmitArm
		|| arming.confirmationPhrase != liveTransmitConfirmationPhrase) {
		return makeError(LiveTransmitErrorCode::notConfirmed,
			"confirm", "all three arms and the exact confirmation phrase are required");
	}
	confirmation_ = LiveTransmitConfirmation{arming.revision, nextNonce_++};
	auto value = std::make_shared<LiveTransmitServiceSnapshot>(*snapshot_);
	value->sequence = nextSequence_++;
	value->state = LiveServiceState::confirmed;
	value->isConfirmationAvailable = true;
	snapshot_ = std::move(value);
	return *confirmation_;
}

LiveTransmitServiceResult
LiveTransmitService::start(const LiveTransmitConfirmation& confirmation)
{
	if (isRunning_.load(std::memory_order_acquire)) {
		return makeError(LiveTransmitErrorCode::concurrentOperation,
			"start", "a live transmit operation is active");
	}
	if (worker_.joinable()) worker_.join();
	LiveTransmitPrepared prepared;
	LiveTransmitConfiguration configuration;
	{
		std::lock_guard lock(mutex_);
		if (isRunning_.load(std::memory_order_acquire)) {
			return makeError(LiveTransmitErrorCode::concurrentOperation,
				"start", "a live transmit operation is active");
		}
		if (!confirmation_ || confirmation_->nonce != confirmation.nonce
			|| confirmation_->revision != confirmation.revision) {
			return makeError(LiveTransmitErrorCode::confirmationConsumed,
				"start", "confirmation is missing, stale, or already consumed");
		}
		if (snapshot_->hasUnresolvedPttHazard || !prepared_ || !configuration_) {
			return makeError(LiveTransmitErrorCode::unresolvedHazard,
				"start", "live transmission cannot start in the current state");
		}
		prepared = *prepared_;
		configuration = *configuration_;
		confirmation_.reset();
		isRunning_.store(true, std::memory_order_release);
		auto value = std::make_shared<LiveTransmitServiceSnapshot>(*snapshot_);
		value->sequence = nextSequence_++;
		value->state = LiveServiceState::checkingPtt;
		value->isConfirmationAvailable = false;
		snapshot_ = std::move(value);
	}
	worker_ = std::jthread([this, revision = confirmation.revision,
		prepared = std::move(prepared), configuration = std::move(configuration)]() mutable {
		run(revision, std::move(prepared), std::move(configuration));
	});
	return std::nullopt;
}

std::variant<std::shared_ptr<rig::PttProvider>, LiveTransmitError>
LiveTransmitService::createProvider(const LivePttConfiguration& configuration,
	const std::shared_ptr<rig::MonotonicClock>& clock) const
{
	return runtime_->createPttProvider(configuration, clock);
}

void
LiveTransmitService::run(const std::uint64_t revision,
	LiveTransmitPrepared prepared, LiveTransmitConfiguration configuration) noexcept
{
	try {
		auto clock = runtime_->createClock();
		auto scheduler = runtime_->createScheduler(clock);
		auto providerResult = createProvider(configuration.ptt, clock);
		if (const auto* error = std::get_if<LiveTransmitError>(&providerResult)) {
			publish(LiveServiceState::faulted, error->operation + ": " + error->message);
			isRunning_.store(false, std::memory_order_release);
			return;
		}
		audio::AudioDiscoveryService discovery(runtime_->createDiscoveryProvider());
		auto discoveryResult = discovery.refresh({{configuration.playback.backend}, false});
		if (const auto* error = std::get_if<audio::DiscoveryError>(&discoveryResult)) {
			publish(LiveServiceState::faulted, error->message);
			isRunning_.store(false, std::memory_order_release);
			return;
		}
		auto discoverySnapshot
			= std::get<std::shared_ptr<const audio::AudioDiscoverySnapshot>>(discoveryResult);
		auto selection = selectLivePlaybackDevice(*discoverySnapshot,
			configuration.playback);
		if (const auto* error = std::get_if<LiveTransmitError>(&selection)) {
			publish(LiveServiceState::faulted, error->operation + ": " + error->message);
			isRunning_.store(false, std::memory_order_release);
			return;
		}
		auto source = createLiveTransmitSource(prepared);
		if (const auto* error = std::get_if<SampleSourceError>(&source)) {
			publish(LiveServiceState::faulted, error->message);
			isRunning_.store(false, std::memory_order_release);
			return;
		}
		auto endpoint = createAudioStreamTransmitEndpoint(
			{std::get<audio::AudioStreamConfiguration>(selection), discoverySnapshot},
			runtime_->createAudioAdapter());
		if (const auto* error = std::get_if<TransmitAudioError>(&endpoint)) {
			publish(LiveServiceState::faulted, error->operation + ": " + error->message);
			isRunning_.store(false, std::memory_order_release);
			return;
		}
		auto provider = std::get<std::shared_ptr<rig::PttProvider>>(providerResult);
		auto supervisor = std::make_shared<rig::PttSupervisor>(provider, clock);
		auto coordinator = std::make_shared<TransmitCoordinator>(
			supervisor, clock, scheduler);
		{
			std::lock_guard lock(mutex_);
			if (snapshot_->revision != revision) {
				isRunning_.store(false, std::memory_order_release);
				return;
			}
			coordinator_ = coordinator;
			supervisor_ = supervisor;
			scheduler_ = scheduler;
		}
		TransmitRequest request;
		request.policy.preKeyDelay = configuration.preKeyDelay;
		request.policy.postAudioTail = configuration.postAudioTail;
		auto result = coordinator->run(request,
			std::get<std::unique_ptr<FiniteSampleSource>>(std::move(source)),
			std::get<std::unique_ptr<TransmitAudioEndpoint>>(std::move(endpoint)));
		LiveServiceState state = LiveServiceState::faulted;
		if (result->outcome == TransmitOutcome::completed) state = LiveServiceState::completed;
		else if (result->outcome == TransmitOutcome::cancelled) state = LiveServiceState::cancelled;
		else if (result->ptt.hasHazard) state = LiveServiceState::hazardous;
		publish(state, result->outcome == TransmitOutcome::completed
			? std::string{} : result->message, result);
	} catch (const std::exception& error) {
		publish(LiveServiceState::faulted, error.what());
	} catch (...) {
		publish(LiveServiceState::faulted, "unknown live transmit failure");
	}
	isRunning_.store(false, std::memory_order_release);
}

void
LiveTransmitService::publish(const LiveServiceState state, std::string error,
	std::shared_ptr<const TransmitSessionSnapshot> session)
{
	std::lock_guard lock(mutex_);
	auto value = std::make_shared<LiveTransmitServiceSnapshot>(*snapshot_);
	value->sequence = nextSequence_++;
	value->state = state;
	value->primaryError = std::move(error);
	value->session = std::move(session);
	value->cleanupErrors.clear();
	if (value->session) {
		value->cleanupErrors = value->session->secondaryErrors;
		value->hasUnresolvedPttHazard = value->session->ptt.hasHazard;
	}
	value->isConfirmationAvailable = false;
	snapshot_ = std::move(value);
}

void
LiveTransmitService::requestCancel() noexcept
{
	std::shared_ptr<TransmitCoordinator> coordinator;
	{
		std::lock_guard lock(mutex_);
		coordinator = coordinator_;
		if (isRunning_.load(std::memory_order_acquire)) {
			auto value = std::make_shared<LiveTransmitServiceSnapshot>(*snapshot_);
			value->sequence = nextSequence_++;
			value->state = LiveServiceState::stopping;
			snapshot_ = std::move(value);
		}
	}
	if (coordinator) coordinator->requestCancel();
}

std::shared_ptr<const LiveTransmitServiceSnapshot>
LiveTransmitService::snapshot() const
{
	std::lock_guard lock(mutex_);
	if (coordinator_ && isRunning_.load(std::memory_order_acquire)) {
		auto value = std::make_shared<LiveTransmitServiceSnapshot>(*snapshot_);
		value->session = coordinator_->snapshot();
		return value;
	}
	return snapshot_;
}

LivePttQueryResult
LiveTransmitService::checkPtt(const LivePttConfiguration& configuration)
{
	if (isRunning_.load(std::memory_order_acquire)) {
		return makeError(LiveTransmitErrorCode::concurrentOperation,
			"check PTT", "a live transmit operation is active");
	}
	auto clock = runtime_->createClock();
	auto providerResult = createProvider(configuration, clock);
	if (const auto* error = std::get_if<LiveTransmitError>(&providerResult)) return *error;
	auto supervisor = std::make_shared<rig::PttSupervisor>(
		std::get<std::shared_ptr<rig::PttProvider>>(providerResult), clock);
	auto result = supervisor->execute(rig::PttAction::query,
		clock->now() + std::chrono::seconds(2));
	if (result.certainty != rig::PttCertainty::definitelyUnkeyed) {
		auto scheduler = runtime_->createScheduler(clock);
		std::lock_guard lock(mutex_);
		supervisor_ = std::move(supervisor);
		scheduler_ = std::move(scheduler);
		auto value = std::make_shared<LiveTransmitServiceSnapshot>(*snapshot_);
		value->sequence = nextSequence_++;
		value->state = LiveServiceState::hazardous;
		value->hasUnresolvedPttHazard = true;
		value->primaryError = "PTT check did not confirm an unkeyed state";
		value->isConfirmationAvailable = false;
		snapshot_ = std::move(value);
	}
	return result;
}

audio::DiscoveryResult
LiveTransmitService::discover(const audio::AudioBackend backend,
	std::stop_token stopToken)
{
	if (isRunning_.load(std::memory_order_acquire)) {
		return audio::DiscoveryError{audio::DiscoveryErrorCode::refreshInProgress,
			"live transmission is active", nullptr};
	}
	audio::AudioDiscoveryService discovery(runtime_->createDiscoveryProvider());
	return discovery.refresh({{backend}, false}, stopToken);
}

rig::PttCleanupResult
LiveTransmitService::retryUnkey()
{
	std::shared_ptr<TransmitCoordinator> coordinator;
	std::shared_ptr<rig::PttSupervisor> supervisor;
	std::shared_ptr<rig::MonotonicScheduler> scheduler;
	{
		std::lock_guard lock(mutex_);
		coordinator = coordinator_;
		supervisor = supervisor_;
		scheduler = scheduler_;
	}
	if (isRunning_.load(std::memory_order_acquire)) return {};
	rig::PttCleanupResult result;
	if (coordinator) result = coordinator->retryHazardCleanup({});
	else if (supervisor && scheduler) result = supervisor->unkey({}, *scheduler);
	else return result;
	if (!result.hasHazard) {
		std::lock_guard lock(mutex_);
		auto value = std::make_shared<LiveTransmitServiceSnapshot>(*snapshot_);
		value->sequence = nextSequence_++;
		value->state = LiveServiceState::faulted;
		value->hasUnresolvedPttHazard = false;
		value->primaryError = "PTT was confirmed unkeyed; prepare and confirm a new session";
		snapshot_ = std::move(value);
	}
	return result;
}

std::shared_ptr<const LiveTransmitServiceSnapshot>
LiveTransmitService::shutdown()
{
	requestCancel();
	std::shared_ptr<TransmitCoordinator> coordinator;
	{
		std::lock_guard lock(mutex_);
		coordinator = coordinator_;
	}
	if (coordinator) static_cast<void>(coordinator->shutdown());
	if (worker_.joinable()) worker_.join();
	isRunning_.store(false, std::memory_order_release);
	return snapshot();
}

} // namespace sstv::app
