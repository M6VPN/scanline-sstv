// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/app/transmit_coordinator.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/app/transmit_coordinator.hpp>

#include <algorithm>
#include <exception>
#include <limits>
#include <utility>

namespace sstv::app {
namespace {

class DefaultWatchdogFactory final : public TransmitWatchdogFactory {
public:
	[[nodiscard]] std::unique_ptr<rig::PttWatchdogControl> create(
		std::shared_ptr<rig::PttSupervisor> supervisor,
		std::shared_ptr<rig::MonotonicClock> clock,
		std::shared_ptr<rig::MonotonicScheduler> scheduler,
		std::shared_ptr<rig::PttSafetyRecord> safetyRecord,
		const rig::PttCleanupPolicy cleanupPolicy) override
	{
		return std::make_unique<rig::PttWatchdog>(std::move(supervisor),
			std::move(clock), std::move(scheduler), std::move(safetyRecord), cleanupPolicy);
	}
};

class ActiveSessionGuard final {
public:
	ActiveSessionGuard(std::atomic<bool>& isActive, std::condition_variable& condition)
		: isActive_(isActive), condition_(condition) {}
	~ActiveSessionGuard()
	{
		isActive_.store(false, std::memory_order_release);
		condition_.notify_all();
	}

private:
	std::atomic<bool>& isActive_;
	std::condition_variable& condition_;
};

class AudioEndpointGuard final {
public:
	AudioEndpointGuard(TransmitAudioEndpoint* endpoint, TransmitSessionSnapshot& snapshot)
		: endpoint_(endpoint), snapshot_(snapshot) {}
	~AudioEndpointGuard() { cleanup(); }
	void markOpen() noexcept { isOpen_ = true; }
	void cleanup() noexcept
	{
		if (!isOpen_) return;
		snapshot_.audioStopWasAttempted = true;
		const auto requested = endpoint_->requestStop();
		const auto stopped = endpoint_->stop();
		snapshot_.audioCloseWasAttempted = true;
		const auto closed = endpoint_->close();
		hasFailed_ = requested.has_value() || stopped.has_value() || closed.has_value();
		if (requested) snapshot_.secondaryErrors.push_back(
			"audio request-stop cleanup failed: " + requested->message);
		if (stopped) snapshot_.secondaryErrors.push_back(
			"audio stop cleanup failed: " + stopped->message);
		if (closed) snapshot_.secondaryErrors.push_back(
			"audio close cleanup failed: " + closed->message);
		isOpen_ = false;
	}
	[[nodiscard]] bool hasFailed() const noexcept { return hasFailed_; }

private:
	TransmitAudioEndpoint* endpoint_;
	TransmitSessionSnapshot& snapshot_;
	bool isOpen_ = false;
	bool hasFailed_ = false;
};

[[nodiscard]] bool
isBoundedTimeout(const std::chrono::milliseconds value,
	const std::chrono::milliseconds maximum)
{
	return value.count() > 0 && value <= maximum;
}

[[nodiscard]] bool
isBoundedDelay(const std::chrono::milliseconds value,
	const std::chrono::milliseconds maximum)
{
	return value.count() >= 0 && value <= maximum;
}

[[nodiscard]] TransmitErrorCode
audioFaultCode(const TransmitAudioFault fault)
{
	if (fault == TransmitAudioFault::underrun) return TransmitErrorCode::underrun;
	if (fault == TransmitAudioFault::disconnected
		|| fault == TransmitAudioFault::deviceRemoved) return TransmitErrorCode::disconnect;
	return TransmitErrorCode::audioFailure;
}

} // namespace

struct TransmitCoordinator::RunContext {
	TransmitSessionSnapshot snapshot;
	TransmitRequest request;
	std::unique_ptr<FiniteSampleSource> source;
	std::unique_ptr<TransmitAudioEndpoint> endpoint;
	std::unique_ptr<rig::PttWatchdogControl> watchdog;
	std::unique_ptr<rig::PttLease> lease;
	bool hasFault = false;
	bool wasCancelled = false;
};

TransmitCoordinator::TransmitCoordinator(std::shared_ptr<rig::PttSupervisor> supervisor,
	std::shared_ptr<rig::MonotonicClock> clock,
	std::shared_ptr<rig::MonotonicScheduler> scheduler,
	std::shared_ptr<TransmitWatchdogFactory> watchdogFactory)
	: supervisor_(std::move(supervisor)), clock_(std::move(clock)),
	  scheduler_(std::move(scheduler)), safetyRecord_(std::make_shared<rig::PttSafetyRecord>()),
	  watchdogFactory_(watchdogFactory != nullptr ? std::move(watchdogFactory)
		: std::make_shared<DefaultWatchdogFactory>()),
	  snapshot_(std::make_shared<TransmitSessionSnapshot>()) {}

TransmitCoordinator::~TransmitCoordinator() { (void)shutdown(); }

std::optional<std::string>
TransmitCoordinator::validate(const TransmitRequest& request,
	const FiniteSourceFacts& source) const
{
	const TransmitPolicy& policy = request.policy;
	if (request.sampleRate != audio::nominalAudioSampleRate
		|| source.sampleRate != request.sampleRate) return "M2D accepts only mono float32 at 48000 Hz";
	if (source.frameCount == 0 || source.frameCount > maximumTransmitSourceFrames)
		return "finite source frame count is outside the M2D bound";
	if (request.blockFrames == 0 || request.blockFrames > maximumTransmitBlockFrames)
		return "transmit block size is outside the M2D bound";
	if (request.prefillFrames == 0 || request.prefillFrames > maximumTransmitSourceFrames)
		return "playback prefill is outside the M2D bound";
	if (!isBoundedTimeout(policy.keyTimeout, std::chrono::seconds(10))
		|| !isBoundedTimeout(policy.readbackTimeout, std::chrono::seconds(10))
		|| !isBoundedTimeout(policy.heartbeatInterval, std::chrono::seconds(10))
		|| !isBoundedTimeout(policy.watchdogLease, std::chrono::seconds(10))
		|| !isBoundedTimeout(policy.unkeyTimeout, std::chrono::seconds(10)))
		return "PTT and watchdog timeouts must be between 1 ms and 10 s";
	if (!isBoundedTimeout(policy.audioDrainTimeout, std::chrono::seconds(30))
		|| !isBoundedTimeout(policy.shutdownDeadline, std::chrono::seconds(30)))
		return "drain and shutdown timeouts must be between 1 ms and 30 s";
	if (!isBoundedDelay(policy.preKeyDelay, std::chrono::seconds(5))
		|| !isBoundedDelay(policy.postAudioTail, std::chrono::seconds(2))
		|| !isBoundedDelay(policy.retryDelay, std::chrono::seconds(2)))
		return "transmit delay is outside its bound";
	if (policy.unkeyAttempts == 0 || policy.unkeyAttempts > 8)
		return "unkey attempts must be between 1 and 8";
	if (policy.heartbeatInterval >= policy.watchdogLease)
		return "watchdog lease must exceed the heartbeat interval";
	const std::uint64_t cleanupMilliseconds
		= static_cast<std::uint64_t>(policy.unkeyTimeout.count()) * policy.unkeyAttempts
		+ static_cast<std::uint64_t>(policy.retryDelay.count()) * (policy.unkeyAttempts - 1);
	if (cleanupMilliseconds > static_cast<std::uint64_t>(policy.shutdownDeadline.count()))
		return "shutdown deadline does not cover mandatory unkey retries";
	return std::nullopt;
}

void
TransmitCoordinator::publish(const RunContext& context)
{
	auto value = std::make_shared<TransmitSessionSnapshot>(context.snapshot);
	value->ptt = safetyRecord_->snapshot();
	std::lock_guard lock(mutex_);
	snapshot_ = std::move(value);
}

TransmitRunResult
TransmitCoordinator::run(const TransmitRequest& request,
	std::unique_ptr<FiniteSampleSource> source,
	std::unique_ptr<TransmitAudioEndpoint> endpoint)
{
	bool expected = false;
	if (!isActive_.compare_exchange_strong(expected, true)) {
		auto rejected = std::make_shared<TransmitSessionSnapshot>();
		rejected->outcome = TransmitOutcome::rejected;
		rejected->error = TransmitErrorCode::concurrentSession;
		rejected->message = "another transmit session is active";
		return rejected;
	}
	ActiveSessionGuard activeGuard(isActive_, condition_);
	RunContext context;
	context.request = request;
	context.source = std::move(source);
	context.endpoint = std::move(endpoint);
	AudioEndpointGuard audioGuard(context.endpoint.get(), context.snapshot);
	context.snapshot.trace.reserve(maximumTransmitTraceEntries);
	isCancelled_.store(false, std::memory_order_release);
	auto setState = [this, &context](const TransmitState state, std::string detail) {
		context.snapshot.state = state;
		if (context.snapshot.trace.size() < maximumTransmitTraceEntries) {
			context.snapshot.trace.push_back({state, clock_->now(), std::move(detail)});
		}
		publish(context);
	};
	auto finishActive = [this, &context]() -> TransmitRunResult {
		publish(context);
		return snapshot();
	};
	setState(TransmitState::preparing, "validating finite prepared transmission");
	if (context.source == nullptr || context.endpoint == nullptr) {
		context.snapshot.error = TransmitErrorCode::invalidConfiguration;
		context.snapshot.message = "source and audio endpoint are required";
		context.snapshot.outcome = TransmitOutcome::rejected;
		setState(TransmitState::faulted, context.snapshot.message);
		return finishActive();
	}
	const FiniteSourceFacts sourceFacts = context.source->facts();
	context.snapshot.sourceFrames = sourceFacts.frameCount;
	if (const auto error = validate(request, sourceFacts)) {
		context.snapshot.error = TransmitErrorCode::invalidConfiguration;
		context.snapshot.message = *error;
		context.snapshot.outcome = TransmitOutcome::rejected;
		setState(TransmitState::faulted, *error);
		return finishActive();
	}
	if (isCancelled_.load(std::memory_order_acquire)
		|| isShuttingDown_.load(std::memory_order_acquire)) {
		context.wasCancelled = true;
		context.snapshot.error = TransmitErrorCode::cancelled;
		context.snapshot.message = "transmit session cancelled during preparation";
		context.snapshot.outcome = TransmitOutcome::cancelled;
		setState(TransmitState::cancelled, context.snapshot.message);
		return finishActive();
	}
	if (safetyRecord_->snapshot().hasHazard) {
		context.snapshot.error = TransmitErrorCode::unresolvedPttHazard;
		context.snapshot.message = "unresolved PTT state blocks a new session";
		context.snapshot.outcome = TransmitOutcome::hazardous;
		setState(TransmitState::faulted, context.snapshot.message);
		return finishActive();
	}
	setState(TransmitState::checkingPtt, "confirming PTT is unkeyed before audio open");
	const rig::PttOperationResult preflight = supervisor_->execute(rig::PttAction::query,
		clock_->now() + request.policy.readbackTimeout);
	const bool isDefinitelyUnkeyed = preflight.error == rig::PttErrorCategory::none
		&& preflight.certainty == rig::PttCertainty::definitelyUnkeyed
		&& preflight.observed == rig::PttObservedState::unkeyed;
	if (!isDefinitelyUnkeyed) {
		const rig::PttCleanupPolicy cleanup{request.policy.unkeyTimeout,
			request.policy.unkeyAttempts, request.policy.retryDelay};
		const rig::PttCleanupResult cleanupResult = supervisor_->unkey(cleanup, *scheduler_);
		safetyRecord_->recordCleanup(cleanupResult, false);
		if (cleanupResult.hasHazard) {
			context.snapshot.error = TransmitErrorCode::unresolvedPttHazard;
			context.snapshot.message = "PTT preflight could not confirm an unkeyed state";
			context.snapshot.outcome = TransmitOutcome::hazardous;
			setState(TransmitState::faulted, context.snapshot.message);
			return finishActive();
		}
	}
	std::vector<float> block(request.blockFrames);
	auto fail = [&context, &setState](const TransmitErrorCode code, std::string message) {
		if (context.hasFault) {
			context.snapshot.secondaryErrors.push_back(std::move(message));
			return;
		}
		context.hasFault = true;
		context.snapshot.error = code;
		context.snapshot.message = std::move(message);
		context.endpoint->gateSignal();
		context.source->cancel();
		setState(TransmitState::faulting, context.snapshot.message);
	};
	auto checkControl = [this, &context, &fail]() -> bool {
		if (isCancelled_.load(std::memory_order_acquire)
			|| isShuttingDown_.load(std::memory_order_acquire)) {
			context.wasCancelled = true;
			fail(TransmitErrorCode::cancelled, "transmit session cancelled");
			return false;
		}
		if (context.watchdog != nullptr && context.watchdog->hasExpired()) {
			fail(TransmitErrorCode::watchdogFailure, "PTT watchdog expired");
			return false;
		}
		const TransmitAudioStatus status = context.endpoint->status();
		if (status.fault != TransmitAudioFault::none) {
			fail(audioFaultCode(status.fault), "audio endpoint reported a fault");
			return false;
		}
		return true;
	};
	auto waitPhase = [this, &context, &checkControl](const std::chrono::milliseconds duration) {
		const rig::MonotonicTime finish = clock_->now() + duration;
		while (clock_->now() < finish) {
			if (!checkControl()) return false;
			if (context.watchdog != nullptr && !context.watchdog->heartbeat()) return false;
			const rig::MonotonicTime next = std::min(finish,
				clock_->now() + context.request.policy.heartbeatInterval);
			(void)scheduler_->waitUntil(next, {});
		}
		return checkControl();
	};
	setState(TransmitState::openingAudio, "opening exact audio endpoint");
	if (const auto error = context.endpoint->open()) {
		fail(TransmitErrorCode::audioFailure, error->message);
	} else {
		audioGuard.markOpen();
		context.snapshot.negotiated = context.endpoint->negotiated();
		(void)checkControl();
	}
	if (!context.hasFault) {
		setState(TransmitState::primingAudio, "prefilling, priming, and starting silent audio");
		if (const auto error = context.endpoint->prefillSilence(request.prefillFrames))
			fail(TransmitErrorCode::audioFailure, error->message);
		else if (const auto primeError = context.endpoint->prime())
			fail(TransmitErrorCode::audioFailure, primeError->message);
		else if (const auto startError = context.endpoint->start())
			fail(TransmitErrorCode::audioFailure, startError->message);
		else
			(void)checkControl();
	}
	if (!context.hasFault) {
		setState(TransmitState::armingWatchdog, "arming independent PTT watchdog");
		rig::PttCleanupPolicy cleanup{request.policy.unkeyTimeout,
			request.policy.unkeyAttempts, request.policy.retryDelay};
		try {
			context.watchdog = watchdogFactory_->create(supervisor_, clock_, scheduler_,
				safetyRecord_, cleanup);
		} catch (const std::exception& error) {
			fail(TransmitErrorCode::watchdogFailure, error.what());
		} catch (...) {
			fail(TransmitErrorCode::watchdogFailure, "PTT watchdog construction failed");
		}
		context.lease = std::make_unique<rig::PttLease>(supervisor_, scheduler_,
			safetyRecord_, cleanup);
		if (!context.hasFault && (context.watchdog == nullptr
			|| !context.watchdog->arm(request.policy.watchdogLease)))
			fail(TransmitErrorCode::watchdogFailure, "PTT watchdog failed to arm");
		else
			(void)checkControl();
	}
	if (!context.hasFault) {
		setState(TransmitState::keying, "requesting PTT key");
		context.snapshot.keyWasAttempted = true;
		context.lease->markKeyAttempt();
		rig::PttOperationResult key = supervisor_->execute(rig::PttAction::key,
			clock_->now() + request.policy.keyTimeout);
		context.lease->recordKeyResult(key);
		const bool isKeyed = key.error == rig::PttErrorCategory::none
			&& key.certainty == rig::PttCertainty::definitelyKeyed;
		if (!isKeyed) fail(TransmitErrorCode::keyFailure,
			"PTT key was not accepted with sufficient certainty");
	}
	if (!context.hasFault) {
		setState(TransmitState::preKeyDelay, "holding silence during pre-key delay");
		if (!waitPhase(request.policy.preKeyDelay) && !context.hasFault)
			fail(TransmitErrorCode::cancelled, "pre-key delay interrupted");
	}
	if (!context.hasFault) {
		setState(TransmitState::transmitting, "releasing finite mock samples");
		while (!context.source->isExhausted() && !context.hasFault) {
			if (!checkControl()) break;
			if (!context.watchdog->heartbeat()) {
				fail(TransmitErrorCode::watchdogFailure, "PTT watchdog heartbeat failed");
				break;
			}
			const SampleReadResult read = context.source->read(block);
			if (const auto* error = std::get_if<SampleSourceError>(&read)) {
				fail(TransmitErrorCode::sourceFailure, error->message);
				break;
			}
			const std::size_t count = std::get<std::size_t>(read);
			if (count == 0) {
				fail(TransmitErrorCode::sourceFailure, "finite source stopped before exhaustion");
				break;
			}
			std::size_t offset = 0;
			while (offset < count && !context.hasFault) {
				const std::span<const float> remaining(block.data() + offset, count - offset);
				const std::size_t queued = context.endpoint->queue(remaining);
				if (queued > remaining.size()) {
					fail(TransmitErrorCode::audioFailure, "audio endpoint over-reported queued frames");
					break;
				}
				if (queued == 0) {
					(void)scheduler_->waitUntil(clock_->now()
						+ request.policy.heartbeatInterval, {});
					if (!checkControl()) break;
					continue;
				}
				offset += queued;
				context.snapshot.submittedFrames += queued;
				context.snapshot.nonSilentAudioWasReleased
					= context.snapshot.nonSilentAudioWasReleased || sourceFacts.containsNonSilence;
			}
		}
		if (!context.hasFault) context.endpoint->finishSignal();
	}
	if (!context.hasFault) {
		setState(TransmitState::draining, "waiting for bounded audio drain");
		const rig::MonotonicTime deadline = clock_->now() + request.policy.audioDrainTimeout;
		if (!checkControl()) context.endpoint->gateSignal();
		while (!context.hasFault && !context.endpoint->status().isDrained
			&& clock_->now() < deadline) {
			if (!waitPhase(request.policy.heartbeatInterval)) break;
		}
		if (!context.hasFault) (void)checkControl();
		if (!context.hasFault && !context.endpoint->status().isDrained)
			fail(TransmitErrorCode::drainTimeout, "audio drain timed out");
	}
	if (!context.hasFault) {
		setState(TransmitState::postAudioTail, "holding bounded post-audio tail");
		(void)waitPhase(request.policy.postAudioTail);
	}
	if (context.hasFault) context.endpoint->gateSignal();
	if (context.lease != nullptr && context.lease->isActive()) {
		setState(TransmitState::unkeying, "performing mandatory PTT unkey cleanup");
		const rig::PttCleanupResult cleanup = context.lease->release();
		if (cleanup.hasHazard) {
			if (context.hasFault) {
				context.snapshot.secondaryErrors.push_back(
					"PTT could not be confirmed unkeyed");
			} else {
				context.hasFault = true;
				context.snapshot.error = TransmitErrorCode::cleanupFailure;
				context.snapshot.message = "PTT could not be confirmed unkeyed";
			}
		}
	}
	if (context.watchdog != nullptr) {
		const rig::PttSafetySnapshot safety = safetyRecord_->snapshot();
		context.watchdog->finish(safety.certainty, safety.hasHazard);
	}
	if (isCancelled_.load(std::memory_order_acquire)
		|| isShuttingDown_.load(std::memory_order_acquire)) context.wasCancelled = true;
	audioGuard.cleanup();
	if (audioGuard.hasFailed() && !context.hasFault) {
			context.hasFault = true;
			context.snapshot.error = TransmitErrorCode::cleanupFailure;
			context.snapshot.message = "audio cleanup failed";
	}
	const rig::PttSafetySnapshot finalSafety = safetyRecord_->snapshot();
	if (finalSafety.hasHazard) {
		context.snapshot.outcome = TransmitOutcome::hazardous;
		setState(TransmitState::faulted, "unresolved PTT hazard");
	} else if (context.wasCancelled) {
		context.snapshot.outcome = TransmitOutcome::cancelled;
		setState(TransmitState::cancelled, context.snapshot.message);
	} else if (context.hasFault) {
		context.snapshot.outcome = TransmitOutcome::faulted;
		setState(TransmitState::faulted, context.snapshot.message);
	} else {
		context.snapshot.outcome = TransmitOutcome::completed;
		setState(TransmitState::completed, "transmit orchestration completed safely");
		setState(TransmitState::idle, "coordinator returned to idle");
	}
	return finishActive();
}

void
TransmitCoordinator::requestCancel() noexcept
{
	isCancelled_.store(true, std::memory_order_release);
	scheduler_->notify();
}

TransmitRunResult
TransmitCoordinator::shutdown()
{
	isShuttingDown_.store(true, std::memory_order_release);
	requestCancel();
	std::unique_lock lock(mutex_);
	condition_.wait(lock, [this] { return !isActive_.load(std::memory_order_acquire); });
	return snapshot_;
}

rig::PttCleanupResult
TransmitCoordinator::retryHazardCleanup(const rig::PttCleanupPolicy& policy)
{
	if (isActive_.load(std::memory_order_acquire))
		return rig::PttCleanupResult{rig::PttCertainty::indeterminate, 0, true};
	rig::PttCleanupResult cleanup = supervisor_->unkey(policy, *scheduler_);
	safetyRecord_->recordCleanup(cleanup, false);
	return cleanup;
}

TransmitRunResult
TransmitCoordinator::snapshot() const
{
	std::lock_guard lock(mutex_);
	return snapshot_;
}

bool
TransmitCoordinator::isActive() const noexcept
{
	return isActive_.load(std::memory_order_acquire);
}

} // namespace sstv::app
