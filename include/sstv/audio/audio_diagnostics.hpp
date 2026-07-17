// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/audio/audio_diagnostics.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/audio/audio_discovery.hpp>
#include <sstv/audio/audio_stream.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <string>
#include <variant>
#include <vector>

namespace sstv::audio {

inline constexpr double defaultDiagnosticLevelDbfs = -30.0;
inline constexpr double minimumDiagnosticLevelDbfs = -60.0;
inline constexpr double maximumDiagnosticLevelDbfs = -6.0;
inline constexpr std::uint32_t minimumDiagnosticDurationMs = 250;
inline constexpr std::uint32_t maximumDiagnosticDurationMs = 10'000;
inline constexpr std::uint32_t calibrationFrequencyHz = 1'000;
inline constexpr std::uint32_t calibrationFadeMs = 10;
inline constexpr double loopbackCorrelationThreshold = 0.65;
inline constexpr double silenceDbfsFloor = -120.0;

enum class DiagnosticOperation {
	inputMeter,
	outputCalibration,
	loopback,
};

enum class DiagnosticState {
	idle,
	validating,
	opening,
	priming,
	running,
	stopping,
	analysing,
	completed,
	cancelled,
	faulted,
};

enum class DiagnosticErrorCode {
	invalidRequest,
	unarmed,
	busy,
	discoveryFailed,
	deviceSelection,
	streamFailure,
	underrun,
	overrun,
	nonFiniteCapture,
	timeout,
	cancelled,
	inconclusive,
};

enum class LevelState {
	valid,
	silence,
};

struct DiagnosticRequest {
	DiagnosticOperation operation = DiagnosticOperation::inputMeter;
	AudioBackend backend = AudioBackend::alsa;
	std::optional<DeviceIdentity> playbackIdentity;
	std::optional<DeviceIdentity> captureIdentity;
	std::optional<std::uint64_t> expectedDiscoveryGeneration;
	std::uint32_t playbackChannel = 0;
	std::uint32_t captureChannel = 0;
	std::uint32_t playbackChannels = 2;
	std::uint32_t captureChannels = 2;
	std::uint32_t sampleRate = nominalAudioSampleRate;
	std::uint32_t periodFrames = 480;
	std::uint32_t periodCount = 3;
	std::size_t playbackRingCapacity = 9'600;
	std::size_t captureRingCapacity = 9'600;
	std::size_t playbackPrefillFrames = 1'440;
	std::uint32_t durationMs = 2'000;
	double levelDbfs = defaultDiagnosticLevelDbfs;
	PlaybackMappingPolicy playbackMapping = PlaybackMappingPolicy::selectedChannel;
	bool isRealAudioArmed = false;
};

struct LevelMeasurement {
	LevelState state = LevelState::silence;
	double peak = 0.0;
	double peakDbfs = silenceDbfsFloor;
	double rms = 0.0;
	double rmsDbfs = silenceDbfsFloor;
	double dcMean = 0.0;
	std::uint64_t clippedPositive = 0;
	std::uint64_t clippedNegative = 0;
	std::uint64_t frames = 0;
};

struct LoopbackMeasurement {
	bool isConclusive = false;
	bool correlationPassed = false;
	double correlation = 0.0;
	std::uint64_t latencyFrames = 0;
	double latencyMilliseconds = 0.0;
	double gainDb = 0.0;
	bool hasPolarityInversion = false;
	std::string reason;
};

struct DiagnosticSnapshot {
	DiagnosticState state = DiagnosticState::idle;
	DiagnosticOperation operation = DiagnosticOperation::inputMeter;
	std::uint64_t discoveryGeneration = 0;
	std::optional<DeviceIdentity> playbackIdentity;
	std::optional<DeviceIdentity> captureIdentity;
	std::optional<std::uint32_t> playbackChannel;
	std::optional<std::uint32_t> captureChannel;
	std::optional<NegotiatedStreamFacts> negotiated;
	std::optional<LevelMeasurement> level;
	std::optional<LoopbackMeasurement> loopback;
	AudioStreamStatistics stream;
	std::uint64_t processedFrames = 0;
	std::uint64_t totalFrames = 0;
	std::uint32_t elapsedMs = 0;
	std::uint32_t remainingMs = 0;
	std::string message;
};

struct DiagnosticError {
	DiagnosticErrorCode code;
	DiagnosticState state;
	std::string operation;
	std::string message;
	std::optional<DeviceIdentity> failedIdentity;
	bool isRecoverable = false;
};

using DiagnosticResult = std::variant<DiagnosticSnapshot, DiagnosticError>;
using DiagnosticDiscoveryResult = std::variant<
	std::shared_ptr<const AudioDiscoverySnapshot>, DiagnosticError>;
using DiagnosticSnapshotCallback = std::function<void(
	std::shared_ptr<const DiagnosticSnapshot>)>;
using AudioStreamAdapterFactory = std::function<std::unique_ptr<AudioStreamAdapter>()>;

/** Runs one bounded hardware diagnostic at a time through injected discovery and streams. */
class AudioDiagnosticsService {
public:
	AudioDiagnosticsService(std::shared_ptr<AudioDiscoveryProvider>,
	    AudioStreamAdapterFactory);
	~AudioDiagnosticsService();
	AudioDiagnosticsService(const AudioDiagnosticsService&) = delete;
	AudioDiagnosticsService& operator=(const AudioDiagnosticsService&) = delete;
	[[nodiscard]] DiagnosticDiscoveryResult refresh(AudioBackend,
	    std::stop_token = {});
	[[nodiscard]] DiagnosticResult run(const DiagnosticRequest&,
	    std::stop_token = {}, DiagnosticSnapshotCallback = {});
	void requestStop() noexcept;
	[[nodiscard]] bool isRunning() const noexcept;

private:
	std::shared_ptr<AudioDiscoveryProvider> discoveryProvider_;
	AudioStreamAdapterFactory streamFactory_;
	std::mutex discoveryMutex_;
	std::mutex cancellationMutex_;
	std::stop_source cancellationSource_;
	std::atomic<bool> isRunning_{false};
	std::atomic<bool> isStopRequested_{false};
	std::atomic<std::uint64_t> publishedGeneration_{0};
	std::atomic<std::uint64_t> nextGeneration_{1};
};

[[nodiscard]] std::variant<double, DiagnosticError> convertDbfsToAmplitude(double);
[[nodiscard]] std::vector<float> generateCalibrationSignal(
	std::uint32_t, std::uint32_t, double);
[[nodiscard]] std::vector<float> generateLoopbackSequence(
	std::uint32_t, double);
[[nodiscard]] std::variant<LevelMeasurement, DiagnosticError> analyseAudioLevels(
	const std::vector<float>&);
[[nodiscard]] LoopbackMeasurement analyseLoopback(
	const std::vector<float>&, const std::vector<float>&, std::uint32_t);

} // namespace sstv::audio
