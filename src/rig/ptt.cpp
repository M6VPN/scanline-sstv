// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/rig/ptt.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/rig/ptt.hpp>

#include <condition_variable>
#include <utility>

namespace sstv::rig {
namespace {

class SteadyClock final : public MonotonicClock {
public:
	[[nodiscard]] MonotonicTime now() const noexcept override
	{
		return std::chrono::duration_cast<MonotonicTime>(
			std::chrono::steady_clock::now().time_since_epoch());
	}
};

class SteadyScheduler final : public MonotonicScheduler {
public:
	explicit SteadyScheduler(std::shared_ptr<MonotonicClock> clock)
		: clock_(std::move(clock)) {}
	[[nodiscard]] WaitResult waitUntil(
		const MonotonicTime deadline, const std::stop_token stopToken) noexcept override
	{
		std::unique_lock lock(mutex_);
		const std::uint64_t generation = generation_;
		while (clock_->now() < deadline && generation == generation_) {
			const MonotonicTime remaining = deadline - clock_->now();
			const auto wake = std::chrono::steady_clock::now() + remaining;
			if (!condition_.wait_until(lock, stopToken, wake,
				[this, generation] { return generation != generation_; })) {
				if (stopToken.stop_requested()) return WaitResult::cancelled;
			}
		}
		if (stopToken.stop_requested()) return WaitResult::cancelled;
		return clock_->now() >= deadline
			? WaitResult::deadlineReached : WaitResult::notified;
	}
	void notify() noexcept override
	{
		{
			std::lock_guard lock(mutex_);
			++generation_;
		}
		condition_.notify_all();
	}

private:
	std::shared_ptr<MonotonicClock> clock_;
	std::mutex mutex_;
	std::condition_variable_any condition_;
	std::uint64_t generation_ = 0;
};

} // namespace

PttSupervisor::PttSupervisor(std::shared_ptr<PttProvider> provider,
	std::shared_ptr<MonotonicClock> clock)
	: provider_(std::move(provider)), clock_(std::move(clock)) {}

PttOperationResult
PttSupervisor::execute(const PttAction action, const MonotonicTime deadline,
	const std::size_t attempt) noexcept
{
	std::lock_guard lock(mutex_);
	const MonotonicTime started = clock_->now();
	if (deadline <= started || attempt == 0) {
		return PttOperationResult{action, PttObservedState::unknown,
			PttCertainty::indeterminate, PttReadback::unavailable,
			PttErrorCategory::invalidRequest, false, action == PttAction::key,
			attempt, started, started, "invalid PTT deadline or attempt"};
	}
	PttRequest request{action, deadline, attempt,
		nextOperationId_.fetch_add(1, std::memory_order_relaxed)};
	PttOperationResult result = provider_->execute(request);
	result.action = action;
	result.attempt = attempt;
	if (result.started == MonotonicTime{}) result.started = started;
	if (result.completed == MonotonicTime{}) result.completed = clock_->now();
	if (result.completed > deadline && result.error == PttErrorCategory::none) {
		result.error = PttErrorCategory::timeout;
		result.certainty = PttCertainty::indeterminate;
		result.mayHaveKeyed = action == PttAction::key;
		result.message = "PTT provider exceeded its deadline";
	}
	return result;
}

PttCleanupResult
PttSupervisor::unkey(const PttCleanupPolicy& policy,
	MonotonicScheduler& scheduler, const std::stop_token) noexcept
{
	PttCleanupResult cleanup;
	for (std::size_t attempt = 1; attempt <= policy.unkeyAttempts; ++attempt) {
		const MonotonicTime deadline = clock_->now() + policy.unkeyTimeout;
		PttOperationResult result = execute(PttAction::unkey, deadline, attempt);
		cleanup.attempts = attempt;
		if (result.certainty == PttCertainty::definitelyUnkeyed) {
			cleanup.certainty = PttCertainty::definitelyUnkeyed;
			cleanup.hasHazard = false;
			return cleanup;
		}
		if (attempt < policy.unkeyAttempts && policy.retryDelay.count() > 0) {
			(void)scheduler.waitUntil(clock_->now() + policy.retryDelay, {});
		}
	}
	return cleanup;
}

void
PttSafetyRecord::recordKeyAttempt() noexcept
{
	std::lock_guard lock(mutex_);
	snapshot_.certainty = PttCertainty::indeterminate;
	snapshot_.hasHazard = true;
}

void
PttSafetyRecord::recordCleanup(const PttCleanupResult& result,
	const bool watchdogExpired) noexcept
{
	std::lock_guard lock(mutex_);
	snapshot_.certainty = result.certainty;
	snapshot_.unkeyAttempts += result.attempts;
	snapshot_.hasHazard = result.hasHazard;
	snapshot_.watchdogExpired = snapshot_.watchdogExpired || watchdogExpired;
}

PttSafetySnapshot
PttSafetyRecord::snapshot() const noexcept
{
	std::lock_guard lock(mutex_);
	return snapshot_;
}

PttWatchdog::PttWatchdog(std::shared_ptr<PttSupervisor> supervisor,
	std::shared_ptr<MonotonicClock> clock,
	std::shared_ptr<MonotonicScheduler> scheduler,
	std::shared_ptr<PttSafetyRecord> safetyRecord,
	PttCleanupPolicy cleanupPolicy)
	: supervisor_(std::move(supervisor)), clock_(std::move(clock)),
	  scheduler_(std::move(scheduler)), safetyRecord_(std::move(safetyRecord)),
	  cleanupPolicy_(cleanupPolicy), worker_([this](const std::stop_token token) {
		run(token);
	})
{
	std::unique_lock lock(startMutex_);
	startCondition_.wait(lock, [this] {
		return isOperational_.load(std::memory_order_acquire);
	});
}

PttWatchdog::~PttWatchdog()
{
	worker_.request_stop();
	startCondition_.notify_all();
	scheduler_->notify();
	if (worker_.joinable()) worker_.join();
}

bool
PttWatchdog::arm(const std::chrono::milliseconds leaseDuration) noexcept
{
	{
		std::lock_guard lock(startMutex_);
		if (!isOperational_.load(std::memory_order_acquire)
			|| leaseDuration.count() <= 0
			|| isArmed_.load(std::memory_order_acquire)) return false;
		hasExpired_.store(false, std::memory_order_release);
		leaseNanoseconds_.store(
			std::chrono::duration_cast<MonotonicTime>(leaseDuration).count(),
			std::memory_order_release);
		expiryNanoseconds_.store((clock_->now() + leaseDuration).count(),
			std::memory_order_release);
		isArmed_.store(true, std::memory_order_release);
	}
	startCondition_.notify_all();
	scheduler_->notify();
	return true;
}

bool
PttWatchdog::heartbeat() noexcept
{
	if (!isArmed_.load(std::memory_order_acquire)
		|| hasExpired_.load(std::memory_order_acquire)) return false;
	if (clock_->now() >= MonotonicTime(
		expiryNanoseconds_.load(std::memory_order_acquire))) return false;
	expiryNanoseconds_.store((clock_->now()
		+ MonotonicTime(leaseNanoseconds_.load(std::memory_order_acquire))).count(),
		std::memory_order_release);
	scheduler_->notify();
	return true;
}

void
PttWatchdog::finish(const PttCertainty certainty, const bool hasHazard) noexcept
{
	if (certainty == PttCertainty::definitelyUnkeyed || hasHazard) {
		isArmed_.store(false, std::memory_order_release);
		scheduler_->notify();
	}
}

bool
PttWatchdog::isArmed() const noexcept
{
	return isArmed_.load(std::memory_order_acquire);
}

bool
PttWatchdog::hasExpired() const noexcept
{
	return hasExpired_.load(std::memory_order_acquire);
}

void
PttWatchdog::run(const std::stop_token stopToken) noexcept
{
	{
		std::lock_guard lock(startMutex_);
		isOperational_.store(true, std::memory_order_release);
	}
	startCondition_.notify_all();
	while (!stopToken.stop_requested()) {
		if (!isArmed_.load(std::memory_order_acquire)) {
			std::unique_lock lock(startMutex_);
			startCondition_.wait(lock, [this, stopToken] {
				return stopToken.stop_requested()
					|| isArmed_.load(std::memory_order_acquire);
			});
			continue;
		}
		const MonotonicTime expiry(
			expiryNanoseconds_.load(std::memory_order_acquire));
		(void)scheduler_->waitUntil(expiry, stopToken);
		if (stopToken.stop_requested() || !isArmed_.load(std::memory_order_acquire)
			|| clock_->now() < MonotonicTime(
				expiryNanoseconds_.load(std::memory_order_acquire))) continue;
		hasExpired_.store(true, std::memory_order_release);
		const PttCleanupResult cleanup = supervisor_->unkey(
			cleanupPolicy_, *scheduler_);
		safetyRecord_->recordCleanup(cleanup, true);
		isArmed_.store(false, std::memory_order_release);
	}
	isOperational_.store(false, std::memory_order_release);
}

PttLease::PttLease(std::shared_ptr<PttSupervisor> supervisor,
	std::shared_ptr<MonotonicScheduler> scheduler,
	std::shared_ptr<PttSafetyRecord> safetyRecord,
	PttCleanupPolicy cleanupPolicy)
	: supervisor_(std::move(supervisor)), scheduler_(std::move(scheduler)),
	  safetyRecord_(std::move(safetyRecord)), cleanupPolicy_(cleanupPolicy) {}

PttLease::~PttLease() { releaseForDestruction(); }

PttLease::PttLease(PttLease&& other) noexcept
	: supervisor_(std::move(other.supervisor_)), scheduler_(std::move(other.scheduler_)),
	  safetyRecord_(std::move(other.safetyRecord_)), cleanupPolicy_(other.cleanupPolicy_),
	  isActive_(std::exchange(other.isActive_, false)),
	  hasReleased_(std::exchange(other.hasReleased_, true)) {}

PttLease&
PttLease::operator=(PttLease&& other) noexcept
{
	if (this == &other) return *this;
	releaseForDestruction();
	supervisor_ = std::move(other.supervisor_);
	scheduler_ = std::move(other.scheduler_);
	safetyRecord_ = std::move(other.safetyRecord_);
	cleanupPolicy_ = other.cleanupPolicy_;
	isActive_ = std::exchange(other.isActive_, false);
	hasReleased_ = std::exchange(other.hasReleased_, true);
	return *this;
}

void
PttLease::markKeyAttempt() noexcept
{
	isActive_ = true;
	hasReleased_ = false;
	safetyRecord_->recordKeyAttempt();
}

void
PttLease::recordKeyResult(const PttOperationResult& result) noexcept
{
	if (result.certainty == PttCertainty::definitelyUnkeyed && !result.mayHaveKeyed) {
		isActive_ = false;
		PttCleanupResult safe{PttCertainty::definitelyUnkeyed, 0, false};
		safetyRecord_->recordCleanup(safe, false);
	}
}

PttCleanupResult
PttLease::release() noexcept
{
	if (!isActive_) {
		return PttCleanupResult{PttCertainty::definitelyUnkeyed, 0, false};
	}
	if (hasReleased_) {
		const PttSafetySnapshot safety = safetyRecord_->snapshot();
		return PttCleanupResult{safety.certainty, 0, safety.hasHazard};
	}
	PttCleanupResult result = supervisor_->unkey(cleanupPolicy_, *scheduler_);
	safetyRecord_->recordCleanup(result, false);
	hasReleased_ = true;
	isActive_ = result.hasHazard;
	return result;
}

bool
PttLease::isActive() const noexcept { return isActive_; }

void
PttLease::releaseForDestruction() noexcept
{
	if (isActive_ && !hasReleased_) (void)release();
}

std::shared_ptr<MonotonicClock>
createSteadyMonotonicClock()
{
	return std::make_shared<SteadyClock>();
}

std::shared_ptr<MonotonicScheduler>
createSteadyMonotonicScheduler(std::shared_ptr<MonotonicClock> clock)
{
	return std::make_shared<SteadyScheduler>(std::move(clock));
}

} // namespace sstv::rig
