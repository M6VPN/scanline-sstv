// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/app/live_transmit.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/app/offline_tx_editor.hpp>
#include <sstv/app/rendered_transmit.hpp>
#include <sstv/audio/audio_discovery.hpp>
#include <sstv/rig/ptt.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

namespace sstv::app {

inline constexpr double minimumLiveTransmitGainDbfs = -60.0;
inline constexpr double maximumLiveTransmitGainDbfs = -6.0;
inline constexpr std::string_view liveTransmitConfirmationPhrase
	= "TRANSMIT LIVE SSTV";
inline constexpr std::uint64_t maximumLiveTransmitDelayMilliseconds = 10'000;

enum class LiveTransmitErrorCode {
	invalidPreparation,
	invalidGain,
	invalidDevice,
	deviceNotFound,
	identityCollision,
	invalidPtt,
	invalidDelay,
	staleRevision,
	notConfirmed,
	confirmationConsumed,
	concurrentOperation,
	runtimeFailure,
	unresolvedHazard,
};

struct LiveTransmitError {
	LiveTransmitErrorCode code;
	std::string operation;
	std::string message;
};

struct LiveTransmitGain {
	double decibelsFullScale;
	float scalar;
};

struct LiveTransmitPreparationRequest {
	OfflineEditorRequest editor;
	double gainDecibelsFullScale;
};

struct LiveTransmitPrepared {
	std::shared_ptr<const OfflineEditorSnapshot> snapshot;
	LiveTransmitGain gain;
};

struct LivePlaybackSelectionRequest {
	audio::AudioBackend backend;
	std::string opaqueIdentity;
	std::uint32_t playbackChannels;
	std::uint32_t selectedOutputChannel;
};

using LiveTransmitPreparationResult
	= std::variant<LiveTransmitPrepared, LiveTransmitError>;
using LivePlaybackSelectionResult
	= std::variant<audio::AudioStreamConfiguration, LiveTransmitError>;

enum class LivePttProviderKind {
	flrig,
	rigctld,
};

struct LivePttConfiguration {
	LivePttProviderKind provider = LivePttProviderKind::flrig;
	std::string address;
	std::uint16_t port = 0;
	std::optional<std::string> flrigPath;
};

struct LiveTransmitConfiguration {
	std::uint64_t revision = 0;
	LivePlaybackSelectionRequest playback;
	LivePttConfiguration ptt;
	std::chrono::milliseconds preKeyDelay{0};
	std::chrono::milliseconds postAudioTail{0};
};

struct LiveTransmitArming {
	std::uint64_t revision = 0;
	bool hasRealAudioArm = false;
	bool hasAutomaticPttArm = false;
	bool hasLiveTransmitArm = false;
	std::string confirmationPhrase;
};

struct LiveTransmitConfirmation {
	std::uint64_t revision = 0;
	std::uint64_t nonce = 0;
};

enum class LiveServiceState {
	idle,
	ready,
	confirmed,
	checkingPtt,
	running,
	stopping,
	completed,
	cancelled,
	faulted,
	hazardous,
};

struct LiveTransmitServiceSnapshot {
	std::uint64_t sequence = 0;
	std::uint64_t revision = 0;
	LiveServiceState state = LiveServiceState::idle;
	std::shared_ptr<const OfflineEditorSnapshot> prepared;
	std::shared_ptr<const TransmitSessionSnapshot> session;
	std::string primaryError;
	std::vector<std::string> cleanupErrors;
	bool hasUnresolvedPttHazard = false;
	bool isConfirmationAvailable = false;
};

using LiveTransmitServiceResult = std::optional<LiveTransmitError>;
using LivePttQueryResult = std::variant<rig::PttOperationResult, LiveTransmitError>;

/** Dependency injection boundary for exact audio and PTT resource construction. */
class LiveTransmitRuntime {
public:
	virtual ~LiveTransmitRuntime() = default;
	[[nodiscard]] virtual std::shared_ptr<audio::AudioDiscoveryProvider>
		createDiscoveryProvider() = 0;
	[[nodiscard]] virtual std::unique_ptr<audio::AudioStreamAdapter>
		createAudioAdapter() = 0;
	[[nodiscard]] virtual std::shared_ptr<rig::MonotonicClock> createClock() = 0;
	[[nodiscard]] virtual std::shared_ptr<rig::MonotonicScheduler> createScheduler(
		std::shared_ptr<rig::MonotonicClock>) = 0;
	[[nodiscard]] virtual std::variant<std::shared_ptr<rig::PttProvider>, LiveTransmitError>
		createPttProvider(const LivePttConfiguration&,
			std::shared_ptr<rig::MonotonicClock>) = 0;
};

/** Create the production runtime. Resource creation remains dormant until requested. */
[[nodiscard]] std::shared_ptr<LiveTransmitRuntime> createLiveTransmitRuntime();

/** Shared CLI/GUI owner for one explicitly confirmed live-transmit session. */
class LiveTransmitService final {
public:
	explicit LiveTransmitService(std::shared_ptr<LiveTransmitRuntime>
		= createLiveTransmitRuntime());
	~LiveTransmitService();
	LiveTransmitService(const LiveTransmitService&) = delete;
	LiveTransmitService& operator=(const LiveTransmitService&) = delete;
	[[nodiscard]] LiveTransmitServiceResult configure(
		LiveTransmitPrepared, LiveTransmitConfiguration);
	[[nodiscard]] std::variant<LiveTransmitConfirmation, LiveTransmitError> confirm(
		const LiveTransmitArming&);
	[[nodiscard]] LiveTransmitServiceResult start(const LiveTransmitConfirmation&);
	void requestCancel() noexcept;
	[[nodiscard]] std::shared_ptr<const LiveTransmitServiceSnapshot> snapshot() const;
	[[nodiscard]] audio::DiscoveryResult discover(audio::AudioBackend,
		std::stop_token = {});
	[[nodiscard]] LivePttQueryResult checkPtt(const LivePttConfiguration&);
	[[nodiscard]] rig::PttCleanupResult retryUnkey();
	[[nodiscard]] std::shared_ptr<const LiveTransmitServiceSnapshot> shutdown();

private:
	void run(std::uint64_t, LiveTransmitPrepared, LiveTransmitConfiguration) noexcept;
	void publish(LiveServiceState, std::string = {},
		std::shared_ptr<const TransmitSessionSnapshot> = nullptr);
	[[nodiscard]] LiveTransmitServiceResult validateConfiguration(
		const LiveTransmitPrepared&, const LiveTransmitConfiguration&) const;
	[[nodiscard]] std::variant<std::shared_ptr<rig::PttProvider>, LiveTransmitError>
		createProvider(const LivePttConfiguration&,
			const std::shared_ptr<rig::MonotonicClock>&) const;

	std::shared_ptr<LiveTransmitRuntime> runtime_;
	mutable std::mutex mutex_;
	std::shared_ptr<const LiveTransmitServiceSnapshot> snapshot_;
	std::optional<LiveTransmitPrepared> prepared_;
	std::optional<LiveTransmitConfiguration> configuration_;
	std::optional<LiveTransmitConfirmation> confirmation_;
	std::shared_ptr<TransmitCoordinator> coordinator_;
	std::shared_ptr<rig::PttSupervisor> supervisor_;
	std::shared_ptr<rig::MonotonicScheduler> scheduler_;
	std::jthread worker_;
	std::atomic<bool> isRunning_{false};
	std::uint64_t nextSequence_ = 1;
	std::uint64_t nextNonce_ = 1;
};

/** Prepare and validate one immutable live-transmit payload before acquisition. */
[[nodiscard]] LiveTransmitPreparationResult prepareLiveTransmit(
	const LiveTransmitPreparationRequest&);

/** Validate gain and live limits for an existing immutable editor snapshot. */
[[nodiscard]] LiveTransmitPreparationResult prepareLiveTransmitSnapshot(
	std::shared_ptr<const OfflineEditorSnapshot>, double);

/** Resolve one exact playback identity from one fresh discovery snapshot. */
[[nodiscard]] LivePlaybackSelectionResult selectLivePlaybackDevice(
	const audio::AudioDiscoverySnapshot&, const LivePlaybackSelectionRequest&);

/** Create the constant-gain float source for a validated prepared payload. */
[[nodiscard]] FiniteSampleSourceCreateResult createLiveTransmitSource(
	const LiveTransmitPrepared&);

} // namespace sstv::app
