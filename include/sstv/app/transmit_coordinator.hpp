// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/app/transmit_coordinator.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/audio/audio_stream.hpp>
#include <sstv/rig/ptt.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <variant>
#include <vector>

namespace sstv::app {

inline constexpr std::uint64_t maximumTransmitSourceFrames = 28'800'000;
inline constexpr std::size_t maximumTransmitBlockFrames = 4'096;
inline constexpr std::size_t maximumTransmitTraceEntries = 1'024;

enum class TransmitState {
	idle,
	preparing,
	openingAudio,
	primingAudio,
	armingWatchdog,
	keying,
	preKeyDelay,
	transmitting,
	draining,
	postAudioTail,
	unkeying,
	completed,
	faulting,
	faulted,
	cancelled,
};

enum class TransmitOutcome {
	completed,
	cancelled,
	faulted,
	hazardous,
	rejected,
};

enum class TransmitErrorCode {
	none,
	invalidConfiguration,
	concurrentSession,
	unresolvedPttHazard,
	audioFailure,
	watchdogFailure,
	keyFailure,
	sourceFailure,
	underrun,
	disconnect,
	drainTimeout,
	cancelled,
	cleanupFailure,
};

struct TransmitPolicy {
	std::chrono::milliseconds keyTimeout{2'000};
	std::chrono::milliseconds readbackTimeout{500};
	std::chrono::milliseconds preKeyDelay{250};
	std::chrono::milliseconds heartbeatInterval{100};
	std::chrono::milliseconds watchdogLease{500};
	std::chrono::milliseconds audioDrainTimeout{2'000};
	std::chrono::milliseconds postAudioTail{250};
	std::chrono::milliseconds unkeyTimeout{500};
	std::size_t unkeyAttempts = 3;
	std::chrono::milliseconds retryDelay{100};
	std::chrono::milliseconds shutdownDeadline{3'000};
};

struct FiniteSourceFacts {
	std::uint32_t sampleRate = audio::nominalAudioSampleRate;
	std::uint64_t frameCount = 0;
	bool containsNonSilence = false;
};

struct SampleSourceError {
	std::string message;
};

using SampleReadResult = std::variant<std::size_t, SampleSourceError>;

/** Finite mock/test sample source. M2D does not provide an SSTV implementation. */
class FiniteSampleSource {
public:
	virtual ~FiniteSampleSource() = default;
	[[nodiscard]] virtual FiniteSourceFacts facts() const noexcept = 0;
	[[nodiscard]] virtual SampleReadResult read(std::span<float> destination) noexcept = 0;
	[[nodiscard]] virtual bool isExhausted() const noexcept = 0;
	virtual void cancel() noexcept = 0;
};

enum class TransmitAudioFault {
	none,
	underrun,
	disconnected,
	deviceRemoved,
	adapterFailure,
};

struct TransmitAudioStatus {
	TransmitAudioFault fault = TransmitAudioFault::none;
	std::uint64_t queuedFrames = 0;
	bool isDrained = false;
	bool isOpen = false;
	bool isPrimed = false;
	bool isRunning = false;
};

struct TransmitAudioError {
	std::string operation;
	std::string message;
};

using TransmitAudioResult = std::optional<TransmitAudioError>;

/** Exact endpoint seam used only through injected implementations in M2D. */
class TransmitAudioEndpoint {
public:
	virtual ~TransmitAudioEndpoint() = default;
	[[nodiscard]] virtual TransmitAudioResult open() noexcept = 0;
	[[nodiscard]] virtual std::optional<audio::NegotiatedStreamFacts> negotiated() const noexcept = 0;
	[[nodiscard]] virtual TransmitAudioResult prefillSilence(std::size_t frames) noexcept = 0;
	[[nodiscard]] virtual TransmitAudioResult prime() noexcept = 0;
	[[nodiscard]] virtual TransmitAudioResult start() noexcept = 0;
	[[nodiscard]] virtual std::size_t queue(std::span<const float> samples) noexcept = 0;
	virtual void gateSignal() noexcept = 0;
	[[nodiscard]] virtual TransmitAudioStatus status() const noexcept = 0;
	[[nodiscard]] virtual TransmitAudioResult requestStop() noexcept = 0;
	[[nodiscard]] virtual TransmitAudioResult stop() noexcept = 0;
	[[nodiscard]] virtual TransmitAudioResult close() noexcept = 0;
};

/** Injection seam for watchdog startup failure and deterministic tests. */
class TransmitWatchdogFactory {
public:
	virtual ~TransmitWatchdogFactory() = default;
	[[nodiscard]] virtual std::unique_ptr<rig::PttWatchdogControl> create(
		std::shared_ptr<rig::PttSupervisor> supervisor,
		std::shared_ptr<rig::MonotonicClock> clock,
		std::shared_ptr<rig::MonotonicScheduler> scheduler,
		std::shared_ptr<rig::PttSafetyRecord> safetyRecord,
		rig::PttCleanupPolicy cleanupPolicy) = 0;
};

struct TransmitRequest {
	TransmitPolicy policy{};
	std::uint32_t sampleRate = audio::nominalAudioSampleRate;
	std::size_t prefillFrames = 1'440;
	std::size_t blockFrames = 480;
};

struct TransmitTraceEntry {
	TransmitState state = TransmitState::idle;
	rig::MonotonicTime timestamp{};
	std::string detail;
};

struct TransmitSessionSnapshot {
	TransmitState state = TransmitState::idle;
	TransmitOutcome outcome = TransmitOutcome::rejected;
	TransmitErrorCode error = TransmitErrorCode::none;
	std::string message;
	std::uint64_t sourceFrames = 0;
	std::uint64_t submittedFrames = 0;
	bool keyWasAttempted = false;
	bool nonSilentAudioWasReleased = false;
	bool audioStopWasAttempted = false;
	bool audioCloseWasAttempted = false;
	rig::PttSafetySnapshot ptt{};
	std::optional<audio::NegotiatedStreamFacts> negotiated;
	std::vector<TransmitTraceEntry> trace;
};

using TransmitRunResult = std::shared_ptr<const TransmitSessionSnapshot>;

/** Synchronous control-worker coordinator with independent watchdog protection. */
class TransmitCoordinator {
public:
	TransmitCoordinator(std::shared_ptr<rig::PttSupervisor> supervisor,
		std::shared_ptr<rig::MonotonicClock> clock,
		std::shared_ptr<rig::MonotonicScheduler> scheduler,
		std::shared_ptr<TransmitWatchdogFactory> watchdogFactory = nullptr);
	~TransmitCoordinator();
	TransmitCoordinator(const TransmitCoordinator&) = delete;
	TransmitCoordinator& operator=(const TransmitCoordinator&) = delete;
	[[nodiscard]] TransmitRunResult run(const TransmitRequest& request,
		std::unique_ptr<FiniteSampleSource> source,
		std::unique_ptr<TransmitAudioEndpoint> endpoint);
	void requestCancel() noexcept;
	[[nodiscard]] TransmitRunResult shutdown();
	[[nodiscard]] rig::PttCleanupResult retryHazardCleanup(
		const rig::PttCleanupPolicy& policy);
	[[nodiscard]] TransmitRunResult snapshot() const;
	[[nodiscard]] bool isActive() const noexcept;

private:
	struct RunContext;
	[[nodiscard]] std::optional<std::string> validate(
		const TransmitRequest& request, const FiniteSourceFacts& source) const;
	void publish(const RunContext& context);
	std::shared_ptr<rig::PttSupervisor> supervisor_;
	std::shared_ptr<rig::MonotonicClock> clock_;
	std::shared_ptr<rig::MonotonicScheduler> scheduler_;
	std::shared_ptr<rig::PttSafetyRecord> safetyRecord_;
	std::shared_ptr<TransmitWatchdogFactory> watchdogFactory_;
	mutable std::mutex mutex_;
	std::condition_variable condition_;
	std::shared_ptr<const TransmitSessionSnapshot> snapshot_;
	std::atomic<bool> isActive_{false};
	std::atomic<bool> isCancelled_{false};
	std::atomic<bool> isShuttingDown_{false};
};

} // namespace sstv::app
