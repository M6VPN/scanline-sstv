// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/rig/ptt.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <thread>

namespace sstv::rig {

using MonotonicTime = std::chrono::nanoseconds;

enum class PttAction {
	key,
	unkey,
	query,
};

enum class PttObservedState {
	keyed,
	unkeyed,
	unknown,
};

enum class PttCertainty {
	definitelyKeyed,
	definitelyUnkeyed,
	indeterminate,
};

enum class PttReadback {
	available,
	unavailable,
};

enum class PttErrorCategory {
	none,
	rejected,
	timeout,
	disconnected,
	protocolFault,
	malformedResponse,
	providerFailure,
	invalidRequest,
};

struct PttRequest {
	PttAction action = PttAction::query;
	MonotonicTime deadline{};
	std::size_t attempt = 1;
	std::uint64_t operationId = 0;
};

struct PttOperationResult {
	PttAction action = PttAction::query;
	PttObservedState observed = PttObservedState::unknown;
	PttCertainty certainty = PttCertainty::indeterminate;
	PttReadback readback = PttReadback::unavailable;
	PttErrorCategory error = PttErrorCategory::none;
	bool isRecoverable = false;
	bool mayHaveKeyed = false;
	std::size_t attempt = 0;
	MonotonicTime started{};
	MonotonicTime completed{};
	std::string message;
	std::optional<std::int32_t> providerCode;
	std::uint64_t operationId = 0;
};

struct PttCleanupPolicy {
	std::chrono::milliseconds unkeyTimeout{500};
	std::size_t unkeyAttempts = 3;
	std::chrono::milliseconds retryDelay{100};
};

struct PttCleanupResult {
	PttCertainty certainty = PttCertainty::indeterminate;
	std::size_t attempts = 0;
	bool hasHazard = true;
};

/** Supplies monotonic time for all transmit safety decisions. */
class MonotonicClock {
public:
	virtual ~MonotonicClock() = default;
	[[nodiscard]] virtual MonotonicTime now() const noexcept = 0;
};

enum class WaitResult {
	deadlineReached,
	notified,
	cancelled,
};

/** Waits against the same monotonic time domain as MonotonicClock. */
class MonotonicScheduler {
public:
	virtual ~MonotonicScheduler() = default;
	[[nodiscard]] virtual WaitResult waitUntil(
		MonotonicTime deadline, std::stop_token stopToken) noexcept = 0;
	virtual void notify() noexcept = 0;
};

/** Control-thread PTT provider. Implementations must honor request deadlines. */
class PttProvider {
public:
	virtual ~PttProvider() = default;
	[[nodiscard]] virtual PttOperationResult execute(
		const PttRequest& request) noexcept = 0;
};

/** Serializes all normal and watchdog calls to one exact provider. */
class PttSupervisor {
public:
	PttSupervisor(std::shared_ptr<PttProvider> provider,
		std::shared_ptr<MonotonicClock> clock);
	[[nodiscard]] PttOperationResult execute(
		PttAction action, MonotonicTime deadline, std::size_t attempt = 1) noexcept;
	[[nodiscard]] PttCleanupResult unkey(const PttCleanupPolicy& policy,
		MonotonicScheduler& scheduler, std::stop_token stopToken = {}) noexcept;

private:
	std::shared_ptr<PttProvider> provider_;
	std::shared_ptr<MonotonicClock> clock_;
	std::mutex mutex_;
	std::atomic<std::uint64_t> nextOperationId_{1};
};

struct PttSafetySnapshot {
	PttCertainty certainty = PttCertainty::definitelyUnkeyed;
	std::size_t unkeyAttempts = 0;
	bool hasHazard = false;
	bool watchdogExpired = false;
};

/** Shared immutable-at-read safety state used by coordinator, lease, and watchdog. */
class PttSafetyRecord {
public:
	void recordKeyAttempt() noexcept;
	void recordCleanup(const PttCleanupResult& result, bool watchdogExpired) noexcept;
	[[nodiscard]] PttSafetySnapshot snapshot() const noexcept;

private:
	mutable std::mutex mutex_;
	PttSafetySnapshot snapshot_{};
};

/** Independent heartbeat watchdog that requests unkey after lease expiry. */
class PttWatchdogControl {
public:
	virtual ~PttWatchdogControl() = default;
	[[nodiscard]] virtual bool arm(std::chrono::milliseconds leaseDuration) noexcept = 0;
	[[nodiscard]] virtual bool heartbeat() noexcept = 0;
	virtual void finish(PttCertainty certainty, bool hasHazard) noexcept = 0;
	[[nodiscard]] virtual bool isArmed() const noexcept = 0;
	[[nodiscard]] virtual bool hasExpired() const noexcept = 0;
};

class PttWatchdog final : public PttWatchdogControl {
public:
	PttWatchdog(std::shared_ptr<PttSupervisor> supervisor,
		std::shared_ptr<MonotonicClock> clock,
		std::shared_ptr<MonotonicScheduler> scheduler,
		std::shared_ptr<PttSafetyRecord> safetyRecord,
		PttCleanupPolicy cleanupPolicy);
	~PttWatchdog();
	PttWatchdog(const PttWatchdog&) = delete;
	PttWatchdog& operator=(const PttWatchdog&) = delete;
	[[nodiscard]] bool arm(std::chrono::milliseconds leaseDuration) noexcept override;
	[[nodiscard]] bool heartbeat() noexcept override;
	void finish(PttCertainty certainty, bool hasHazard) noexcept override;
	[[nodiscard]] bool isArmed() const noexcept override;
	[[nodiscard]] bool hasExpired() const noexcept override;

private:
	void run(std::stop_token stopToken) noexcept;
	std::shared_ptr<PttSupervisor> supervisor_;
	std::shared_ptr<MonotonicClock> clock_;
	std::shared_ptr<MonotonicScheduler> scheduler_;
	std::shared_ptr<PttSafetyRecord> safetyRecord_;
	PttCleanupPolicy cleanupPolicy_;
	std::mutex startMutex_;
	std::condition_variable startCondition_;
	std::atomic<std::int64_t> expiryNanoseconds_{0};
	std::atomic<std::int64_t> leaseNanoseconds_{0};
	std::atomic<bool> isArmed_{false};
	std::atomic<bool> isOperational_{false};
	std::atomic<bool> hasExpired_{false};
	std::jthread worker_;
};

/** Non-copyable ownership marker for a possibly keyed transmitter. */
class PttLease {
public:
	PttLease(std::shared_ptr<PttSupervisor> supervisor,
		std::shared_ptr<MonotonicScheduler> scheduler,
		std::shared_ptr<PttSafetyRecord> safetyRecord,
		PttCleanupPolicy cleanupPolicy);
	~PttLease();
	PttLease(const PttLease&) = delete;
	PttLease& operator=(const PttLease&) = delete;
	PttLease(PttLease&& other) noexcept;
	PttLease& operator=(PttLease&& other) noexcept;
	void markKeyAttempt() noexcept;
	void recordKeyResult(const PttOperationResult& result) noexcept;
	[[nodiscard]] PttCleanupResult release() noexcept;
	[[nodiscard]] bool isActive() const noexcept;

private:
	void releaseForDestruction() noexcept;
	std::shared_ptr<PttSupervisor> supervisor_;
	std::shared_ptr<MonotonicScheduler> scheduler_;
	std::shared_ptr<PttSafetyRecord> safetyRecord_;
	PttCleanupPolicy cleanupPolicy_;
	bool isActive_ = false;
	bool hasReleased_ = false;
};

[[nodiscard]] std::shared_ptr<MonotonicClock> createSteadyMonotonicClock();
[[nodiscard]] std::shared_ptr<MonotonicScheduler> createSteadyMonotonicScheduler(
	std::shared_ptr<MonotonicClock> clock);

} // namespace sstv::rig
