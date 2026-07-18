// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m2d_transmit_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/app/transmit_coordinator.hpp>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <deque>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace std::chrono_literals;

void
expect(const bool condition, const std::string& message)
{
	if (!condition) {
		std::cerr << "FAIL: " << message << '\n';
		std::exit(1);
	}
}

class VirtualTime final : public sstv::rig::MonotonicClock,
	public sstv::rig::MonotonicScheduler {
public:
	[[nodiscard]] sstv::rig::MonotonicTime now() const noexcept override
	{
		return sstv::rig::MonotonicTime(nowNanoseconds_.load(std::memory_order_acquire));
	}
	[[nodiscard]] sstv::rig::WaitResult waitUntil(
		const sstv::rig::MonotonicTime deadline,
		const std::stop_token stopToken) noexcept override
	{
		std::unique_lock lock(mutex_);
		const std::uint64_t generation = generation_;
		lock.unlock();
		if (onWait) onWait();
		lock.lock();
		condition_.wait(lock, [this, deadline, generation, stopToken] {
			return stopToken.stop_requested() || now() >= deadline
				|| generation_ != generation;
		});
		if (stopToken.stop_requested()) return sstv::rig::WaitResult::cancelled;
		return now() >= deadline ? sstv::rig::WaitResult::deadlineReached
			: sstv::rig::WaitResult::notified;
	}
	void notify() noexcept override
	{
		{
			std::lock_guard lock(mutex_);
			++generation_;
		}
		condition_.notify_all();
	}
	void advance(const std::chrono::milliseconds amount) noexcept
	{
		nowNanoseconds_.fetch_add(
			std::chrono::duration_cast<sstv::rig::MonotonicTime>(amount).count(),
			std::memory_order_acq_rel);
		notify();
	}
	std::function<void()> onWait;

private:
	std::atomic<std::int64_t> nowNanoseconds_{1};
	std::mutex mutex_;
	std::condition_variable_any condition_;
	std::uint64_t generation_ = 0;
};

[[nodiscard]] sstv::rig::PttOperationResult
keyedResult(const sstv::rig::PttReadback readback = sstv::rig::PttReadback::unavailable)
{
	return {sstv::rig::PttAction::key, sstv::rig::PttObservedState::keyed,
		sstv::rig::PttCertainty::definitelyKeyed, readback,
		sstv::rig::PttErrorCategory::none, false, true, 0, {}, {}, {}, {}, 0};
}

[[nodiscard]] sstv::rig::PttOperationResult
unkeyedResult()
{
	return {sstv::rig::PttAction::unkey, sstv::rig::PttObservedState::unkeyed,
		sstv::rig::PttCertainty::definitelyUnkeyed, sstv::rig::PttReadback::available,
		sstv::rig::PttErrorCategory::none, false, false, 0, {}, {}, {}, {}, 0};
}

[[nodiscard]] sstv::rig::PttOperationResult
failedUnkeyResult()
{
	return {sstv::rig::PttAction::unkey, sstv::rig::PttObservedState::unknown,
		sstv::rig::PttCertainty::indeterminate, sstv::rig::PttReadback::unavailable,
		sstv::rig::PttErrorCategory::timeout, true, false, 0, {}, {}, "unkey timeout", {}, 0};
}

class MockPttProvider final : public sstv::rig::PttProvider {
public:
	[[nodiscard]] sstv::rig::PttOperationResult execute(
		const sstv::rig::PttRequest& request) noexcept override
	{
		const int current = activeCalls_.fetch_add(1, std::memory_order_acq_rel) + 1;
		int previous = maximumActive_.load(std::memory_order_acquire);
		while (current > previous && !maximumActive_.compare_exchange_weak(previous, current)) {}
		if (onOperation) onOperation(request.action);
		sstv::rig::PttOperationResult result;
		{
			std::lock_guard lock(mutex_);
			operations.push_back(request.action);
			if (request.action == sstv::rig::PttAction::key) result = keyResult;
			else if (request.action == sstv::rig::PttAction::query) result = queryResult;
			else if (!unkeyResults.empty()) {
				result = unkeyResults.front();
				unkeyResults.pop_front();
			} else result = unkeyedResult();
		}
		operationCondition_.notify_all();
		if (request.action == sstv::rig::PttAction::key
			&& (result.certainty == sstv::rig::PttCertainty::definitelyKeyed
				|| result.mayHaveKeyed)) isKeyed.store(true, std::memory_order_release);
		if (request.action == sstv::rig::PttAction::unkey
			&& result.certainty == sstv::rig::PttCertainty::definitelyUnkeyed)
			isKeyed.store(false, std::memory_order_release);
		activeCalls_.fetch_sub(1, std::memory_order_acq_rel);
		return result;
	}
	[[nodiscard]] std::vector<sstv::rig::PttAction> operationSnapshot() const
	{
		std::lock_guard lock(mutex_);
		return operations;
	}
	[[nodiscard]] int maximumActive() const noexcept
	{
		return maximumActive_.load(std::memory_order_acquire);
	}
	void clearOperations()
	{
		std::lock_guard lock(mutex_);
		operations.clear();
	}
	void waitForOperationCount(const std::size_t count)
	{
		std::unique_lock lock(mutex_);
		operationCondition_.wait(lock, [this, count] { return operations.size() >= count; });
	}

	sstv::rig::PttOperationResult keyResult = keyedResult();
	sstv::rig::PttOperationResult queryResult = unkeyedResult();
	std::deque<sstv::rig::PttOperationResult> unkeyResults;
	std::atomic<bool> isKeyed{false};
	std::function<void(sstv::rig::PttAction)> onOperation;

private:
	mutable std::mutex mutex_;
	std::condition_variable operationCondition_;
	std::vector<sstv::rig::PttAction> operations;
	std::atomic<int> activeCalls_{0};
	std::atomic<int> maximumActive_{0};
};

class MockSource final : public sstv::app::FiniteSampleSource {
public:
	explicit MockSource(std::vector<float> samples, const bool fail = false)
		: samples_(std::move(samples)), shouldFail_(fail) {}
	[[nodiscard]] sstv::app::FiniteSourceFacts facts() const noexcept override
	{
		if (onFacts) onFacts();
		return {48'000, samples_.size(), std::ranges::any_of(samples_,
			[](const float sample) { return sample != 0.0F; })};
	}
	[[nodiscard]] sstv::app::SampleReadResult read(
		const std::span<float> destination) noexcept override
	{
		if (shouldFail_) return sstv::app::SampleSourceError{"scripted source failure"};
		const std::size_t count = std::min(destination.size(), samples_.size() - offset_);
		std::copy_n(samples_.begin() + static_cast<std::ptrdiff_t>(offset_), count,
			destination.begin());
		offset_ += count;
		return count;
	}
	[[nodiscard]] bool isExhausted() const noexcept override
	{
		return isCancelled_ || offset_ == samples_.size();
	}
	void cancel() noexcept override { isCancelled_ = true; }
	std::function<void()> onFacts;

private:
	std::vector<float> samples_;
	std::size_t offset_ = 0;
	bool shouldFail_ = false;
	bool isCancelled_ = false;
};

enum class AudioFailurePoint {
	none,
	open,
	prefill,
	prime,
	start,
	requestStop,
	stop,
	close,
};

struct MockAudioRecord {
	std::vector<std::string> operations;
	std::vector<float> output;
	bool releasedBeforeKey = false;
	bool gateCalled = false;
};

class MockAudioEndpoint final : public sstv::app::TransmitAudioEndpoint {
public:
	MockAudioEndpoint(std::shared_ptr<MockPttProvider> provider,
		std::shared_ptr<MockAudioRecord> record, const AudioFailurePoint failure,
		const sstv::app::TransmitAudioFault runningFault = sstv::app::TransmitAudioFault::none)
		: provider_(std::move(provider)), record_(std::move(record)), failure_(failure),
		  runningFault_(runningFault) {}
	[[nodiscard]] sstv::app::TransmitAudioResult open() noexcept override
	{
		record_->operations.push_back("open");
		if (onOpen) onOpen();
		if (failure_ == AudioFailurePoint::open) return error("open");
		isOpen_ = true;
		return std::nullopt;
	}
	[[nodiscard]] std::optional<sstv::audio::NegotiatedStreamFacts> negotiated() const noexcept override
	{
		return sstv::audio::NegotiatedStreamFacts{sstv::audio::AudioBackend::nullDiagnostic,
			48'000, 480, 3, sstv::audio::NegotiatedEndpointFacts{}, std::nullopt};
	}
	[[nodiscard]] sstv::app::TransmitAudioResult prefillSilence(
		const std::size_t frames) noexcept override
	{
		record_->operations.push_back("prefill");
		if (onPrefill) onPrefill();
		if (failure_ == AudioFailurePoint::prefill) return error("prefill");
		prefilledFrames_ = frames;
		return std::nullopt;
	}
	[[nodiscard]] sstv::app::TransmitAudioResult prime() noexcept override
	{
		record_->operations.push_back("prime");
		if (onPrime) onPrime();
		if (failure_ == AudioFailurePoint::prime) return error("prime");
		isPrimed_ = prefilledFrames_ > 0;
		return std::nullopt;
	}
	[[nodiscard]] sstv::app::TransmitAudioResult start() noexcept override
	{
		record_->operations.push_back("start");
		if (onStart) onStart();
		if (failure_ == AudioFailurePoint::start) return error("start");
		isRunning_ = true;
		return std::nullopt;
	}
	[[nodiscard]] std::size_t queue(const std::span<const float> samples) noexcept override
	{
		record_->operations.push_back("queue");
		if (onQueue) onQueue();
		if (!provider_->isKeyed.load(std::memory_order_acquire)
			&& std::ranges::any_of(samples, [](const float sample) { return sample != 0.0F; }))
			record_->releasedBeforeKey = true;
		record_->output.insert(record_->output.end(), samples.begin(), samples.end());
		hasQueued_ = true;
		return samples.size();
	}
	void gateSignal() noexcept override
	{
		record_->operations.push_back("gate");
		record_->gateCalled = true;
	}
	[[nodiscard]] sstv::app::TransmitAudioStatus status() const noexcept override
	{
		if (onStatus) onStatus();
		const sstv::app::TransmitAudioFault fault = hasQueued_ ? runningFault_
			: sstv::app::TransmitAudioFault::none;
		return {fault, 0, hasQueued_ && !neverDrain, isOpen_, isPrimed_, isRunning_};
	}
	[[nodiscard]] sstv::app::TransmitAudioResult requestStop() noexcept override
	{
		record_->operations.push_back("requestStop");
		if (failure_ == AudioFailurePoint::requestStop) return error("requestStop");
		return std::nullopt;
	}
	[[nodiscard]] sstv::app::TransmitAudioResult stop() noexcept override
	{
		record_->operations.push_back("stop");
		isRunning_ = false;
		if (failure_ == AudioFailurePoint::stop) return error("stop");
		return std::nullopt;
	}
	[[nodiscard]] sstv::app::TransmitAudioResult close() noexcept override
	{
		record_->operations.push_back("close");
		isOpen_ = false;
		if (failure_ == AudioFailurePoint::close) return error("close");
		return std::nullopt;
	}
	std::function<void()> onOpen;
	std::function<void()> onPrefill;
	std::function<void()> onPrime;
	std::function<void()> onQueue;
	std::function<void()> onStart;
	std::function<void()> onStatus;
	bool neverDrain = false;

private:
	[[nodiscard]] static sstv::app::TransmitAudioError error(const std::string& operation)
	{
		return {operation, "scripted " + operation + " failure"};
	}
	std::shared_ptr<MockPttProvider> provider_;
	std::shared_ptr<MockAudioRecord> record_;
	AudioFailurePoint failure_ = AudioFailurePoint::none;
	sstv::app::TransmitAudioFault runningFault_ = sstv::app::TransmitAudioFault::none;
	std::size_t prefilledFrames_ = 0;
	bool isOpen_ = false;
	bool isPrimed_ = false;
	bool isRunning_ = false;
	mutable bool hasQueued_ = false;
};

class FailedWatchdog final : public sstv::rig::PttWatchdogControl {
public:
	[[nodiscard]] bool arm(std::chrono::milliseconds) noexcept override { return false; }
	[[nodiscard]] bool heartbeat() noexcept override { return false; }
	void finish(sstv::rig::PttCertainty, bool) noexcept override {}
	[[nodiscard]] bool isArmed() const noexcept override { return false; }
	[[nodiscard]] bool hasExpired() const noexcept override { return false; }
};

class FailedWatchdogFactory final : public sstv::app::TransmitWatchdogFactory {
public:
	[[nodiscard]] std::unique_ptr<sstv::rig::PttWatchdogControl> create(
		std::shared_ptr<sstv::rig::PttSupervisor>,
		std::shared_ptr<sstv::rig::MonotonicClock>,
		std::shared_ptr<sstv::rig::MonotonicScheduler>,
		std::shared_ptr<sstv::rig::PttSafetyRecord>,
		sstv::rig::PttCleanupPolicy) override
	{
		return std::make_unique<FailedWatchdog>();
	}
};

class CancellingWatchdogFactory final : public sstv::app::TransmitWatchdogFactory {
public:
	explicit CancellingWatchdogFactory(std::function<void()> cancel)
		: cancel_(std::move(cancel)) {}
	[[nodiscard]] std::unique_ptr<sstv::rig::PttWatchdogControl> create(
		std::shared_ptr<sstv::rig::PttSupervisor> supervisor,
		std::shared_ptr<sstv::rig::MonotonicClock> clock,
		std::shared_ptr<sstv::rig::MonotonicScheduler> scheduler,
		std::shared_ptr<sstv::rig::PttSafetyRecord> safetyRecord,
		const sstv::rig::PttCleanupPolicy cleanupPolicy) override
	{
		auto watchdog = std::make_unique<sstv::rig::PttWatchdog>(
			std::move(supervisor), std::move(clock), std::move(scheduler),
			std::move(safetyRecord), cleanupPolicy);
		cancel_();
		return watchdog;
	}

private:
	std::function<void()> cancel_;
};

struct Harness {
	std::shared_ptr<VirtualTime> time = std::make_shared<VirtualTime>();
	std::shared_ptr<MockPttProvider> provider = std::make_shared<MockPttProvider>();
	std::shared_ptr<sstv::rig::PttSupervisor> supervisor
		= std::make_shared<sstv::rig::PttSupervisor>(provider, time);
	std::shared_ptr<MockAudioRecord> audio = std::make_shared<MockAudioRecord>();
	std::unique_ptr<sstv::app::TransmitCoordinator> coordinator;
	Harness()
		: coordinator(std::make_unique<sstv::app::TransmitCoordinator>(
			supervisor, time, time)) {}
};

[[nodiscard]] sstv::app::TransmitRequest
fastRequest()
{
	sstv::app::TransmitRequest request;
	request.policy.preKeyDelay = 0ms;
	request.policy.postAudioTail = 0ms;
	request.policy.retryDelay = 0ms;
	request.prefillFrames = 4;
	request.blockFrames = 3;
	return request;
}

[[nodiscard]] std::unique_ptr<MockSource>
makeSource(const bool fail = false)
{
	return std::make_unique<MockSource>(std::vector<float>{0.25F, -0.5F, 0.75F, 0.0F}, fail);
}

[[nodiscard]] std::unique_ptr<MockAudioEndpoint>
makeEndpoint(Harness& harness, const AudioFailurePoint failure = AudioFailurePoint::none,
	const sstv::app::TransmitAudioFault fault = sstv::app::TransmitAudioFault::none)
{
	return std::make_unique<MockAudioEndpoint>(harness.provider, harness.audio, failure, fault);
}

[[nodiscard]] std::vector<sstv::app::TransmitState>
states(const sstv::app::TransmitRunResult& result)
{
	std::vector<sstv::app::TransmitState> values;
	std::ranges::transform(result->trace, std::back_inserter(values),
		[](const sstv::app::TransmitTraceEntry& entry) { return entry.state; });
	return values;
}

[[nodiscard]] bool
hasState(const sstv::app::TransmitRunResult& result,
	const sstv::app::TransmitState state)
{
	const std::vector<sstv::app::TransmitState> values = states(result);
	return std::ranges::find(values, state) != values.end();
}

void
testNormalSequence()
{
	Harness harness;
	const auto result = harness.coordinator->run(fastRequest(), makeSource(), makeEndpoint(harness));
	const std::vector expected{
		sstv::app::TransmitState::preparing,
		sstv::app::TransmitState::checkingPtt,
		sstv::app::TransmitState::openingAudio,
		sstv::app::TransmitState::primingAudio,
		sstv::app::TransmitState::armingWatchdog,
		sstv::app::TransmitState::keying,
		sstv::app::TransmitState::preKeyDelay,
		sstv::app::TransmitState::transmitting,
		sstv::app::TransmitState::draining,
		sstv::app::TransmitState::postAudioTail,
		sstv::app::TransmitState::unkeying,
		sstv::app::TransmitState::completed,
		sstv::app::TransmitState::idle,
	};
	expect(states(result) == expected, "normal state sequence is exact");
	expect(result->outcome == sstv::app::TransmitOutcome::completed, "normal outcome completes");
	expect(result->submittedFrames == 4, "all finite frames are submitted");
	expect(!harness.audio->releasedBeforeKey, "no signal is released before keying");
	expect(result->audioStopWasAttempted && result->audioCloseWasAttempted,
		"normal completion stops and closes audio");
	expect(harness.provider->operationSnapshot() == std::vector{
		sstv::rig::PttAction::query, sstv::rig::PttAction::key,
		sstv::rig::PttAction::unkey}, "normal PTT sequence preflights, keys, then unkeys");
}

void
testReadbackAndSafeRejection()
{
	Harness initiallyKeyed;
	initiallyKeyed.provider->queryResult = keyedResult(sstv::rig::PttReadback::available);
	const auto completed = initiallyKeyed.coordinator->run(
		fastRequest(), makeSource(), makeEndpoint(initiallyKeyed));
	expect(completed->outcome == sstv::app::TransmitOutcome::completed,
		"initially keyed preflight cleans up before proceeding");
	expect(initiallyKeyed.provider->operationSnapshot() == std::vector{
		sstv::rig::PttAction::query, sstv::rig::PttAction::unkey,
		sstv::rig::PttAction::key, sstv::rig::PttAction::unkey},
		"preflight cleanup precedes audio keying");
	Harness mismatch;
	mismatch.provider->queryResult = {sstv::rig::PttAction::query,
		sstv::rig::PttObservedState::unknown, sstv::rig::PttCertainty::indeterminate,
		sstv::rig::PttReadback::unavailable, sstv::rig::PttErrorCategory::timeout,
		true, false, 0, {}, {}, "unknown preflight", {}, 0};
	mismatch.provider->unkeyResults = {
		failedUnkeyResult(), failedUnkeyResult(), failedUnkeyResult()};
	const auto mismatchResult = mismatch.coordinator->run(
		fastRequest(), makeSource(), makeEndpoint(mismatch));
	expect(mismatchResult->error == sstv::app::TransmitErrorCode::unresolvedPttHazard,
		"unknown preflight blocks the session");
	expect(mismatch.audio->operations.empty(), "failed preflight opens no audio");
	expect(!mismatchResult->nonSilentAudioWasReleased,
		"failed preflight releases no signal");
	Harness rejected;
	rejected.provider->keyResult = {sstv::rig::PttAction::key,
		sstv::rig::PttObservedState::unkeyed, sstv::rig::PttCertainty::definitelyUnkeyed,
		sstv::rig::PttReadback::available, sstv::rig::PttErrorCategory::rejected,
		false, false, 0, {}, {}, "rejected", {}, 0};
	const auto failed = rejected.coordinator->run(fastRequest(), makeSource(), makeEndpoint(rejected));
	expect(failed->outcome == sstv::app::TransmitOutcome::faulted,
		"definite-unkeyed key rejection faults safely");
	expect(rejected.provider->operationSnapshot() == std::vector{
		sstv::rig::PttAction::query, sstv::rig::PttAction::key},
		"definite-unkeyed rejection needs no redundant unkey");
	expect(!failed->nonSilentAudioWasReleased, "rejected key releases no signal");
}

void
testAmbiguousKeyAndRetries()
{
	Harness ambiguous;
	ambiguous.provider->keyResult = {sstv::rig::PttAction::key,
		sstv::rig::PttObservedState::unknown, sstv::rig::PttCertainty::indeterminate,
		sstv::rig::PttReadback::unavailable, sstv::rig::PttErrorCategory::timeout,
		true, true, 0, {}, {}, "ambiguous key timeout", {}, 0};
	ambiguous.provider->unkeyResults = {failedUnkeyResult(), failedUnkeyResult(), unkeyedResult()};
	const auto result = ambiguous.coordinator->run(
		fastRequest(), makeSource(), makeEndpoint(ambiguous));
	expect(result->outcome == sstv::app::TransmitOutcome::faulted,
		"ambiguous key remains a failed session after safe cleanup");
	expect(result->ptt.certainty == sstv::rig::PttCertainty::definitelyUnkeyed,
		"retry cleanup confirms unkeyed");
	expect(result->ptt.unkeyAttempts == 3, "all scripted unkey attempts are counted");
	expect(!result->nonSilentAudioWasReleased, "ambiguous key releases no signal");
}

void
testPermanentHazardBlocksNextSession()
{
	Harness harness;
	harness.provider->unkeyResults = {
		failedUnkeyResult(), failedUnkeyResult(), failedUnkeyResult()};
	const auto hazardous = harness.coordinator->run(
		fastRequest(), makeSource(), makeEndpoint(harness));
	expect(hazardous->outcome == sstv::app::TransmitOutcome::hazardous,
		"permanent unkey failure is hazardous");
	expect(hazardous->ptt.hasHazard, "hazard remains visible");
	const std::size_t operationCount = harness.provider->operationSnapshot().size();
	const auto rejected = harness.coordinator->run(
		fastRequest(), makeSource(), makeEndpoint(harness));
	expect(rejected->error == sstv::app::TransmitErrorCode::unresolvedPttHazard,
		"hazard blocks a second session");
	expect(harness.provider->operationSnapshot().size() == operationCount,
		"blocked session acquires no PTT resource");
	const auto cleanup = harness.coordinator->retryHazardCleanup(
		sstv::rig::PttCleanupPolicy{500ms, 1, 0ms});
	expect(!cleanup.hasHazard, "explicit cleanup can clear a resolved hazard");
}

void
testResourceAndWatchdogGates()
{
	for (const AudioFailurePoint point : {AudioFailurePoint::open,
		AudioFailurePoint::prefill, AudioFailurePoint::prime,
		AudioFailurePoint::start}) {
		Harness harness;
		const auto result = harness.coordinator->run(
			fastRequest(), makeSource(), makeEndpoint(harness, point));
		expect(result->outcome == sstv::app::TransmitOutcome::faulted,
			"pre-key audio failure faults");
		expect(!result->keyWasAttempted, "pre-key audio failure never requests key");
	}
	Harness failedWatchdog;
	failedWatchdog.coordinator = std::make_unique<sstv::app::TransmitCoordinator>(
		failedWatchdog.supervisor, failedWatchdog.time, failedWatchdog.time,
		std::make_shared<FailedWatchdogFactory>());
	const auto result = failedWatchdog.coordinator->run(
		fastRequest(), makeSource(), makeEndpoint(failedWatchdog));
	expect(result->error == sstv::app::TransmitErrorCode::watchdogFailure,
		"watchdog arm failure is typed");
	expect(!result->keyWasAttempted, "watchdog arm failure prevents keying");
}

void
testPostKeyFaultsAlwaysUnkey()
{
	for (const AudioFailurePoint point : {AudioFailurePoint::requestStop,
		AudioFailurePoint::stop, AudioFailurePoint::close}) {
		Harness harness;
		const auto result = harness.coordinator->run(
			fastRequest(), makeSource(), makeEndpoint(harness, point));
		expect(result->keyWasAttempted, "post-key path attempted key");
		expect(std::ranges::count(harness.provider->operationSnapshot(),
			sstv::rig::PttAction::unkey) >= 1, "post-key fault attempts unkey");
		expect(result->audioStopWasAttempted && result->audioCloseWasAttempted,
			"post-key fault independently attempts audio cleanup");
	}
	for (const sstv::app::TransmitAudioFault fault : {
		sstv::app::TransmitAudioFault::underrun,
		sstv::app::TransmitAudioFault::disconnected,
		sstv::app::TransmitAudioFault::deviceRemoved}) {
		Harness harness;
		const auto result = harness.coordinator->run(
			fastRequest(), makeSource(), makeEndpoint(harness, AudioFailurePoint::none, fault));
		expect(result->outcome == sstv::app::TransmitOutcome::faulted,
			"running audio fault fails the session");
		expect(harness.audio->gateCalled, "running fault gates further signal samples");
		expect(std::ranges::count(harness.provider->operationSnapshot(),
			sstv::rig::PttAction::unkey) >= 1, "running fault unkeys");
	}
	Harness sourceFailure;
	const auto sourceResult = sourceFailure.coordinator->run(
		fastRequest(), makeSource(true), makeEndpoint(sourceFailure));
	expect(sourceResult->error == sstv::app::TransmitErrorCode::sourceFailure,
		"source failure is typed");
	expect(sourceFailure.audio->gateCalled, "source failure gates signal");
	Harness sourceAndCleanupFailure;
	const auto combinedResult = sourceAndCleanupFailure.coordinator->run(
		fastRequest(), makeSource(true),
		makeEndpoint(sourceAndCleanupFailure, AudioFailurePoint::close));
	expect(combinedResult->error == sstv::app::TransmitErrorCode::sourceFailure,
		"cleanup failure replaced the primary source failure");
	expect(!combinedResult->secondaryErrors.empty(),
		"cleanup failure was not retained as a secondary diagnostic");
}

void
testDrainTimeoutAndCoordinatorStall()
{
	{
		Harness harness;
		sstv::app::TransmitRequest request = fastRequest();
		request.blockFrames = 4;
		request.policy.watchdogLease = 10s;
		auto endpoint = makeEndpoint(harness);
		endpoint->neverDrain = true;
		harness.time->onWait = [&harness] {
			if (harness.coordinator->snapshot()->state == sstv::app::TransmitState::draining)
				harness.time->advance(100ms);
		};
		const auto result = harness.coordinator->run(
			request, makeSource(), std::move(endpoint));
		expect(result->error == sstv::app::TransmitErrorCode::drainTimeout,
			"bounded drain timeout faults");
		expect(result->ptt.certainty == sstv::rig::PttCertainty::definitelyUnkeyed,
			"drain timeout completes mandatory unkey");
	}
	{
		Harness harness;
		std::mutex gateMutex;
		std::condition_variable gateCondition;
		bool hasEnteredQueue = false;
		bool canLeaveQueue = false;
		auto endpoint = makeEndpoint(harness);
		endpoint->onQueue = [&] {
			std::unique_lock lock(gateMutex);
			hasEnteredQueue = true;
			gateCondition.notify_all();
			gateCondition.wait(lock, [&] { return canLeaveQueue; });
		};
		sstv::app::TransmitRunResult result;
		std::jthread runThread([&] {
			result = harness.coordinator->run(
				fastRequest(), makeSource(), std::move(endpoint));
		});
		{
			std::unique_lock lock(gateMutex);
			gateCondition.wait(lock, [&] { return hasEnteredQueue; });
		}
		harness.time->advance(501ms);
		harness.provider->waitForOperationCount(2);
		{
			std::lock_guard lock(gateMutex);
			canLeaveQueue = true;
		}
		gateCondition.notify_all();
		runThread.join();
		expect(result->error == sstv::app::TransmitErrorCode::watchdogFailure,
			"coordinator stall is detected after watchdog expiry");
		expect(result->ptt.watchdogExpired, "stall snapshot records watchdog expiry");
		expect(harness.provider->maximumActive() == 1,
			"watchdog and coordinator provider operations stay serialized");
		expect(result->audioStopWasAttempted && result->audioCloseWasAttempted,
			"stalled coordinator still cleans audio after release");
	}
}

void
testCancellationBeforeKey()
{
	Harness harness;
	auto endpoint = makeEndpoint(harness);
	endpoint->onOpen = [&harness] { harness.coordinator->requestCancel(); };
	const auto result = harness.coordinator->run(fastRequest(), makeSource(), std::move(endpoint));
	expect(result->outcome == sstv::app::TransmitOutcome::cancelled,
		"opening cancellation reaches cancelled state");
	expect(!result->keyWasAttempted, "opening cancellation prevents keying");
	expect(result->audioStopWasAttempted && result->audioCloseWasAttempted,
		"opening cancellation cleans audio");
}

void
testCancellationMatrix()
{
	{
		Harness harness;
		auto source = makeSource();
		source->onFacts = [&harness] { harness.coordinator->requestCancel(); };
		const auto result = harness.coordinator->run(
			fastRequest(), std::move(source), makeEndpoint(harness));
		expect(result->outcome == sstv::app::TransmitOutcome::cancelled,
			"preparing cancellation is terminal");
		expect(harness.audio->operations.empty(),
			"preparing cancellation acquires no audio resource");
	}
	{
		Harness harness;
		auto endpoint = makeEndpoint(harness);
		endpoint->onPrime = [&harness] { harness.coordinator->requestCancel(); };
		const auto result = harness.coordinator->run(
			fastRequest(), makeSource(), std::move(endpoint));
		expect(result->outcome == sstv::app::TransmitOutcome::cancelled
			&& hasState(result, sstv::app::TransmitState::primingAudio),
			"priming cancellation cleans up without keying");
		expect(!result->keyWasAttempted, "priming cancellation prevents keying");
	}
	{
		Harness harness;
		harness.coordinator = std::make_unique<sstv::app::TransmitCoordinator>(
			harness.supervisor, harness.time, harness.time,
			std::make_shared<CancellingWatchdogFactory>(
				[&harness] { harness.coordinator->requestCancel(); }));
		const auto result = harness.coordinator->run(
			fastRequest(), makeSource(), makeEndpoint(harness));
		expect(result->outcome == sstv::app::TransmitOutcome::cancelled
			&& hasState(result, sstv::app::TransmitState::armingWatchdog),
			"watchdog-arming cancellation prevents keying");
		expect(!result->keyWasAttempted, "arming cancellation makes no key request");
	}
	{
		Harness harness;
		harness.provider->onOperation = [&harness](const sstv::rig::PttAction action) {
			if (action == sstv::rig::PttAction::key) harness.coordinator->requestCancel();
		};
		const auto result = harness.coordinator->run(
			fastRequest(), makeSource(), makeEndpoint(harness));
		expect(result->outcome == sstv::app::TransmitOutcome::cancelled
			&& hasState(result, sstv::app::TransmitState::keying),
			"keying cancellation passes through mandatory unkey");
		expect(std::ranges::count(harness.provider->operationSnapshot(),
			sstv::rig::PttAction::unkey) == 1, "keying cancellation unkeys exactly once");
		expect(!result->nonSilentAudioWasReleased, "keying cancellation releases no signal");
	}
	{
		Harness harness;
		auto endpoint = makeEndpoint(harness);
		endpoint->onStart = [&harness] { harness.coordinator->requestCancel(); };
		const auto result = harness.coordinator->run(
			fastRequest(), makeSource(), std::move(endpoint));
		expect(result->outcome == sstv::app::TransmitOutcome::cancelled
			&& hasState(result, sstv::app::TransmitState::primingAudio),
			"audio-start cancellation prevents keying and signal release");
		expect(!result->keyWasAttempted,
			"audio-start cancellation occurs before the key request");
		expect(!result->nonSilentAudioWasReleased, "pre-key cancellation releases no signal");
	}
	{
		Harness harness;
		auto endpoint = makeEndpoint(harness);
		endpoint->onQueue = [&harness] { harness.coordinator->requestCancel(); };
		const auto result = harness.coordinator->run(
			fastRequest(), makeSource(), std::move(endpoint));
		expect(result->outcome == sstv::app::TransmitOutcome::cancelled
			&& hasState(result, sstv::app::TransmitState::transmitting),
			"transmitting cancellation gates subsequent blocks");
		expect(result->submittedFrames == 3, "only the in-flight bounded block is submitted");
	}
	{
		Harness harness;
		sstv::app::TransmitRequest request = fastRequest();
		request.blockFrames = 4;
		auto endpoint = makeEndpoint(harness);
		endpoint->onStatus = [&harness] {
			if (harness.audio->output.size() == 4)
				harness.coordinator->requestCancel();
		};
		const auto result = harness.coordinator->run(
			request, makeSource(), std::move(endpoint));
		expect(result->outcome == sstv::app::TransmitOutcome::cancelled
			&& hasState(result, sstv::app::TransmitState::draining),
			"draining cancellation bypasses normal tail completion");
	}
	{
		Harness harness;
		sstv::app::TransmitRequest request = fastRequest();
		request.blockFrames = 4;
		request.policy.postAudioTail = 250ms;
		harness.time->onWait = [&harness] {
			if (harness.coordinator->snapshot()->state
				== sstv::app::TransmitState::postAudioTail)
				harness.coordinator->requestCancel();
		};
		const auto result = harness.coordinator->run(
			request, makeSource(), makeEndpoint(harness));
		expect(result->outcome == sstv::app::TransmitOutcome::cancelled
			&& hasState(result, sstv::app::TransmitState::postAudioTail),
			"post-tail cancellation proceeds directly to unkey cleanup");
	}
	{
		Harness harness;
		harness.provider->onOperation = [&harness](const sstv::rig::PttAction action) {
			if (action == sstv::rig::PttAction::unkey) harness.coordinator->requestCancel();
		};
		const auto result = harness.coordinator->run(
			fastRequest(), makeSource(), makeEndpoint(harness));
		expect(result->outcome == sstv::app::TransmitOutcome::cancelled
			&& hasState(result, sstv::app::TransmitState::unkeying),
			"unkeying cancellation cannot cancel the unkey operation");
		expect(result->ptt.certainty == sstv::rig::PttCertainty::definitelyUnkeyed,
			"unkeying cancellation retains confirmed safety");
	}
}

void
testConfigurationRejectedBeforeResources()
{
	Harness harness;
	sstv::app::TransmitRequest request = fastRequest();
	request.policy.heartbeatInterval = request.policy.watchdogLease;
	const auto result = harness.coordinator->run(request, makeSource(), makeEndpoint(harness));
	expect(result->error == sstv::app::TransmitErrorCode::invalidConfiguration,
		"invalid watchdog policy is rejected");
	expect(harness.audio->operations.empty(), "invalid configuration opens no audio");
	expect(harness.provider->operationSnapshot().empty(), "invalid configuration calls no PTT");
}

void
testConcurrentSessionAndShutdown()
{
	Harness harness;
	std::mutex gateMutex;
	std::condition_variable gateCondition;
	bool hasEnteredOpen = false;
	bool canLeaveOpen = false;
	auto endpoint = makeEndpoint(harness);
	endpoint->onOpen = [&] {
		std::unique_lock lock(gateMutex);
		hasEnteredOpen = true;
		gateCondition.notify_all();
		gateCondition.wait(lock, [&] { return canLeaveOpen; });
	};
	sstv::app::TransmitRunResult firstResult;
	std::jthread runThread([&] {
		firstResult = harness.coordinator->run(
			fastRequest(), makeSource(), std::move(endpoint));
	});
	{
		std::unique_lock lock(gateMutex);
		gateCondition.wait(lock, [&] { return hasEnteredOpen; });
	}
	const auto concurrent = harness.coordinator->run(
		fastRequest(), makeSource(), makeEndpoint(harness));
	expect(concurrent->error == sstv::app::TransmitErrorCode::concurrentSession,
		"concurrent session is rejected without replacing the active endpoint");
	sstv::app::TransmitRunResult shutdownResult;
	std::jthread shutdownThread([&] { shutdownResult = harness.coordinator->shutdown(); });
	harness.coordinator->requestCancel();
	{
		std::lock_guard lock(gateMutex);
		canLeaveOpen = true;
	}
	gateCondition.notify_all();
	runThread.join();
	shutdownThread.join();
	expect(firstResult->outcome == sstv::app::TransmitOutcome::cancelled,
		"shutdown cancels an active opening session");
	expect(shutdownResult->outcome == sstv::app::TransmitOutcome::cancelled,
		"shutdown returns the final safety result");
	expect(!firstResult->keyWasAttempted, "shutdown before prime prevents keying");
	expect(firstResult->audioStopWasAttempted && firstResult->audioCloseWasAttempted,
		"shutdown waits for audio cleanup");
}

void
testProviderDeadlineEnforcement()
{
	auto time = std::make_shared<VirtualTime>();
	auto provider = std::make_shared<MockPttProvider>();
	auto supervisor = std::make_shared<sstv::rig::PttSupervisor>(provider, time);
	provider->onOperation = [time](const sstv::rig::PttAction action) {
		if (action == sstv::rig::PttAction::key) time->advance(3s);
	};
	const auto result = supervisor->execute(
		sstv::rig::PttAction::key, time->now() + 2s);
	expect(result.error == sstv::rig::PttErrorCategory::timeout,
		"provider completion beyond deadline is a timeout");
	expect(result.certainty == sstv::rig::PttCertainty::indeterminate
		&& result.mayHaveKeyed, "late key result remains maybe-keyed");
}

void
testWatchdogExpiryAndLeaseDestruction()
{
	auto time = std::make_shared<VirtualTime>();
	auto provider = std::make_shared<MockPttProvider>();
	auto supervisor = std::make_shared<sstv::rig::PttSupervisor>(provider, time);
	auto safety = std::make_shared<sstv::rig::PttSafetyRecord>();
	{
		sstv::rig::PttWatchdog watchdog(supervisor, time, time, safety,
			sstv::rig::PttCleanupPolicy{500ms, 1, 0ms});
		expect(watchdog.arm(500ms), "watchdog arms");
		time->advance(501ms);
		provider->waitForOperationCount(1);
		expect(watchdog.hasExpired(), "watchdog expires independently");
		expect(safety->snapshot().certainty == sstv::rig::PttCertainty::definitelyUnkeyed,
			"watchdog confirms unkey");
	}
	provider->clearOperations();
	{
		sstv::rig::PttLease lease(supervisor, time, safety,
			sstv::rig::PttCleanupPolicy{500ms, 1, 0ms});
		lease.markKeyAttempt();
	}
	expect(std::ranges::count(provider->operationSnapshot(), sstv::rig::PttAction::unkey) == 1,
		"active lease destructor attempts unkey");
}

} // namespace

int
main()
{
	testConfigurationRejectedBeforeResources();
	testConcurrentSessionAndShutdown();
	testProviderDeadlineEnforcement();
	testNormalSequence();
	testReadbackAndSafeRejection();
	testAmbiguousKeyAndRetries();
	testPermanentHazardBlocksNextSession();
	testResourceAndWatchdogGates();
	testPostKeyFaultsAlwaysUnkey();
	testDrainTimeoutAndCoordinatorStall();
	testCancellationBeforeKey();
	testCancellationMatrix();
	testWatchdogExpiryAndLeaseDestruction();
	std::cout << "M2D transmit coordinator tests passed\n";
	return 0;
}
