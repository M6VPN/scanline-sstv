// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/app/hil_evidence.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sstv::app {

inline constexpr std::uint32_t m2jEvidenceSchemaVersion = 1;
inline constexpr std::size_t maximumHilTextBytes = 16'384U;
inline constexpr std::size_t maximumHilArtifactCount = 64U;
inline constexpr std::size_t maximumHilStageCount = 64U;
inline constexpr std::uintmax_t maximumHilRawCaptureBytes = 64U * 1024U * 1024U;

enum class HilResultState {
	notRun,
	passed,
	failed,
	inconclusive,
	skipped,
};

enum class HilEvidenceSource {
	notMeasured,
	operatorObserved,
	automaticallyMeasured,
};

enum class HilStage {
	manifest,
	discovery,
	audioCalibration,
	pttUnkey,
	keyedSilence,
	fullSstv,
	controlledFault,
	guiCompositor,
};

enum class HilResource : std::uint32_t {
	none = 0U,
	discovery = 1U << 0U,
	audioInput = 1U << 1U,
	audioOutput = 1U << 2U,
	pttQuery = 1U << 3U,
	pttUnkey = 1U << 4U,
	pttKey = 1U << 5U,
	sstvSignal = 1U << 6U,
	gui = 1U << 7U,
};

class HilResourceSet final {
public:
	constexpr HilResourceSet() noexcept = default;
	constexpr explicit HilResourceSet(const std::uint32_t bits) noexcept : bits_(bits) {}
	[[nodiscard]] constexpr bool contains(const HilResource resource) const noexcept
	{
		return (bits_ & static_cast<std::uint32_t>(resource)) != 0U;
	}
	[[nodiscard]] constexpr std::uint32_t bits() const noexcept { return bits_; }
private:
	std::uint32_t bits_ = 0U;
};

struct HilArtifact {
	std::string name;
	std::string sha256;
	std::uint64_t bytes = 0U;
	bool containsRawAudio = false;
};

struct HilStageResult {
	HilStage stage = HilStage::manifest;
	HilResultState state = HilResultState::notRun;
	HilEvidenceSource source = HilEvidenceSource::notMeasured;
	std::optional<std::string> startedUtc;
	std::optional<std::string> endedUtc;
	std::optional<std::string> skippedReason;
	std::optional<std::string> primaryError;
	std::vector<std::string> cleanupErrors;
	std::optional<std::string> finalPttCertainty;
	std::map<std::string, std::optional<std::string>> measurements;
	std::optional<std::string> operatorRadioState;
};

struct HilBuildMetadata {
	std::string utcStarted;
	std::optional<std::string> utcEnded;
	std::string gitCommit;
	bool isDirtyWorktree = false;
	std::string compiler;
	std::string compilerVersion;
	std::string cmakePreset;
	std::map<std::string, std::string> cmakeOptions;
	std::string miniaudioVersion;
	std::optional<std::string> qtVersion;
	std::string operatingSystem;
	std::string kernel;
	std::string architecture;
	std::string redactedHostIdentifier;
	std::string sessionPlatform;
};

struct HilConfiguration {
	std::string audioBackend;
	std::string playbackIdentity;
	std::string identityPersistence;
	bool hasIdentityCollision = false;
	std::uint32_t selectedChannel = 0U;
	std::uint32_t channelCount = 0U;
	std::optional<std::uint32_t> negotiatedRate;
	std::optional<std::string> negotiatedFormat;
	std::optional<std::uint32_t> negotiatedChannels;
	std::optional<std::uint32_t> negotiatedPeriodFrames;
	std::string pttProvider;
	std::string pttAddress;
	std::optional<std::uint16_t> pttPort;
	std::optional<std::string> flrigPath;
	std::string radioManufacturer;
	std::string radioModel;
	std::optional<std::string> radioFirmware;
	std::string audioInterface;
	std::string cabling;
	std::string testArrangement;
	std::string radioMode;
	std::string frequency;
	std::string power;
	std::string voxState;
	std::string compressorState;
	std::string timeoutState;
	std::string antennaState;
	std::string sstvMode;
	std::string fixtureSha256;
	bool hasFskIdentifier = false;
	std::uint64_t durationNanoseconds = 0U;
	std::uint64_t frameCount = 0U;
	double gainDbfs = 0.0;
	std::uint64_t preKeyMilliseconds = 0U;
	std::uint64_t postAudioMilliseconds = 0U;
	std::optional<std::string> instrumentDescription;
	std::optional<std::string> calibrationMethod;
	std::optional<std::string> pttProviderVersion;
};

struct HilEvidenceRecord {
	std::uint32_t schemaVersion = m2jEvidenceSchemaVersion;
	HilBuildMetadata build;
	HilConfiguration configuration;
	std::vector<HilStageResult> stages;
	std::vector<HilArtifact> artifacts;
	std::vector<std::string> limitations;
	std::optional<std::string> operatorNotes;
};

enum class HilErrorCode {
	invalidRecord,
	invalidPath,
	destinationExists,
	resourceLimit,
	serializationFailure,
	publicationFailure,
	invalidStage,
	missingPrerequisite,
	unsafePttState,
	invalidConfirmation,
	stalePermit,
	consumedPermit,
};

struct HilError {
	HilErrorCode code;
	std::string operation;
	std::string message;
};

struct HilPublication {
	std::filesystem::path jsonPath;
	std::filesystem::path markdownPath;
	std::string jsonSha256;
	std::string markdownSha256;
};

struct HilStagePermit {
	HilStage stage = HilStage::manifest;
	std::string configurationDigest;
	std::uint64_t nonce = 0U;
};

using HilValidationResult = std::optional<HilError>;
using HilPublicationResult = std::variant<HilPublication, HilError>;
using HilPermitResult = std::variant<HilStagePermit, HilError>;

/** Owns single-use, configuration-bound authorization without acquiring resources. */
class HilStageAuthorizer final {
public:
	[[nodiscard]] HilPermitResult authorize(HilStage, std::string_view,
		std::string_view, bool);
	[[nodiscard]] HilValidationResult consume(const HilStagePermit&,
		HilStage, std::string_view);
	void invalidate() noexcept;
private:
	std::optional<HilStagePermit> permit_;
	std::uint64_t nextNonce_ = 1U;
};

[[nodiscard]] std::string_view hilResultStateName(HilResultState) noexcept;
[[nodiscard]] std::string_view hilEvidenceSourceName(HilEvidenceSource) noexcept;
[[nodiscard]] std::string_view hilStageName(HilStage) noexcept;
[[nodiscard]] HilResourceSet hilStageResources(HilStage) noexcept;
[[nodiscard]] std::string hilStageConfirmationPhrase(HilStage, std::string_view);
[[nodiscard]] HilValidationResult validateHilEvidence(const HilEvidenceRecord&);
[[nodiscard]] HilValidationResult validateHilStageStart(
	const HilEvidenceRecord&, HilStage);
[[nodiscard]] std::string serializeHilEvidenceJson(const HilEvidenceRecord&);
[[nodiscard]] std::string serializeHilEvidenceMarkdown(const HilEvidenceRecord&);
[[nodiscard]] std::string calculateHilConfigurationDigest(const HilConfiguration&);
[[nodiscard]] std::string calculateHilSha256(std::string_view);
[[nodiscard]] HilPublicationResult publishHilEvidence(const HilEvidenceRecord&,
	const std::filesystem::path&, bool);

/** Atomically records one completed non-keyed stage in an existing evidence session. */
[[nodiscard]] HilPublicationResult publishHilStageResult(
	const std::filesystem::path&, HilStage, HilResultState, HilEvidenceSource,
	const std::map<std::string, std::optional<std::string>>&, std::optional<std::string>,
	std::optional<std::string> = std::nullopt);

} // namespace sstv::app
