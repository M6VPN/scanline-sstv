// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/app/hil_evidence.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/app/hil_evidence.hpp>

#include <sys/stat.h>
#include <sys/types.h>

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <system_error>

namespace sstv::app {
namespace {

constexpr std::size_t maximumEvidenceBytes = 4U * 1024U * 1024U;
constexpr std::array<std::uint32_t, 64> sha256Constants{
	0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU,
	0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U,
	0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U,
	0xc19bf174U, 0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
	0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU, 0x983e5152U,
	0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
	0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU,
	0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
	0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U,
	0xd6990624U, 0xf40e3585U, 0x106aa070U, 0x19a4c116U, 0x1e376c08U,
	0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU,
	0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
	0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};

[[nodiscard]] HilError
makeError(const HilErrorCode code, std::string operation, std::string message)
{
	return {code, std::move(operation), std::move(message)};
}

[[nodiscard]] bool
isBoundedText(const std::string& value)
{
	if (value.empty() || value.size() > maximumHilTextBytes) return false;
	for (const char valueCharacter : value) {
		const unsigned char character = static_cast<unsigned char>(valueCharacter);
		if (character == 0U || character < 0x20U) return false;
	}
	return true;
}

[[nodiscard]] bool
isSha256(const std::string& value)
{
	if (value.size() != 64U) return false;
	for (const char character : value) {
		if (!((character >= '0' && character <= '9')
				|| (character >= 'a' && character <= 'f'))) return false;
	}
	return true;
}

[[nodiscard]] std::string
escapeJson(const std::string_view value)
{
	std::ostringstream output;
	for (const char valueCharacter : value) {
		const unsigned char character = static_cast<unsigned char>(valueCharacter);
		switch (character) {
		case '\"': output << "\\\""; break;
		case '\\': output << "\\\\"; break;
		case '\b': output << "\\b"; break;
		case '\f': output << "\\f"; break;
		case '\n': output << "\\n"; break;
		case '\r': output << "\\r"; break;
		case '\t': output << "\\t"; break;
		default:
			if (character < 0x20U) {
				output << "\\u" << std::hex << std::setw(4)
					<< std::setfill('0') << static_cast<unsigned int>(character)
					<< std::dec;
			} else {
				output << static_cast<char>(character);
			}
		}
	}
	return output.str();
}

void
writeJsonString(std::ostringstream& output, const std::string_view value)
{
	output << '\"' << escapeJson(value) << '\"';
}

void
writeOptionalString(std::ostringstream& output,
	const std::optional<std::string>& value)
{
	if (value.has_value()) writeJsonString(output, *value);
	else output << "null";
}

void
writeStringMap(std::ostringstream& output,
	const std::map<std::string, std::string>& values)
{
	output << '{';
	bool isFirst = true;
	for (const auto& [key, value] : values) {
		if (!isFirst) output << ',';
		isFirst = false;
		writeJsonString(output, key);
		output << ':';
		writeJsonString(output, value);
	}
	output << '}';
}

void
writeOptionalStringMap(std::ostringstream& output,
	const std::map<std::string, std::optional<std::string>>& values)
{
	output << '{';
	bool isFirst = true;
	for (const auto& [key, value] : values) {
		if (!isFirst) output << ',';
		isFirst = false;
		writeJsonString(output, key);
		output << ':';
		writeOptionalString(output, value);
	}
	output << '}';
}

[[nodiscard]] bool
isKeyedStage(const HilStage stage)
{
	return stage == HilStage::keyedSilence || stage == HilStage::fullSstv
		|| stage == HilStage::controlledFault;
}

[[nodiscard]] const HilStageResult*
findStage(const HilEvidenceRecord& record, const HilStage stage)
{
	for (const HilStageResult& result : record.stages) {
		if (result.stage == stage) return &result;
	}
	return nullptr;
}

[[nodiscard]] bool
hasPassed(const HilEvidenceRecord& record, const HilStage stage)
{
	const HilStageResult* const result = findStage(record, stage);
	return result != nullptr && result->state == HilResultState::passed;
}

[[nodiscard]] std::array<std::uint32_t, 8>
calculateSha256Words(const std::string_view input)
{
	std::vector<std::uint8_t> bytes(input.begin(), input.end());
	const std::uint64_t bitCount = static_cast<std::uint64_t>(bytes.size()) * 8U;
	bytes.push_back(0x80U);
	while ((bytes.size() % 64U) != 56U) bytes.push_back(0U);
	for (int shift = 56; shift >= 0; shift -= 8) {
		bytes.push_back(static_cast<std::uint8_t>(bitCount
			>> static_cast<unsigned int>(shift)));
	}
	std::array<std::uint32_t, 8> hash{0x6a09e667U, 0xbb67ae85U,
		0x3c6ef372U, 0xa54ff53aU, 0x510e527fU, 0x9b05688cU,
		0x1f83d9abU, 0x5be0cd19U};
	for (std::size_t offset = 0U; offset < bytes.size(); offset += 64U) {
		std::array<std::uint32_t, 64> words{};
		for (std::size_t index = 0U; index < 16U; ++index) {
			const std::size_t position = offset + index * 4U;
			words[index] = (static_cast<std::uint32_t>(bytes[position]) << 24U)
				| (static_cast<std::uint32_t>(bytes[position + 1U]) << 16U)
				| (static_cast<std::uint32_t>(bytes[position + 2U]) << 8U)
				| static_cast<std::uint32_t>(bytes[position + 3U]);
		}
		for (std::size_t index = 16U; index < words.size(); ++index) {
			const std::uint32_t s0 = std::rotr(words[index - 15U], 7)
				^ std::rotr(words[index - 15U], 18)
				^ (words[index - 15U] >> 3U);
			const std::uint32_t s1 = std::rotr(words[index - 2U], 17)
				^ std::rotr(words[index - 2U], 19)
				^ (words[index - 2U] >> 10U);
			words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
		}
		auto [a, b, c, d, e, f, g, h] = hash;
		for (std::size_t index = 0U; index < words.size(); ++index) {
			const std::uint32_t sigma1 = std::rotr(e, 6) ^ std::rotr(e, 11)
				^ std::rotr(e, 25);
			const std::uint32_t choice = (e & f) ^ (~e & g);
			const std::uint32_t temporary1 = h + sigma1 + choice
				+ sha256Constants[index] + words[index];
			const std::uint32_t sigma0 = std::rotr(a, 2) ^ std::rotr(a, 13)
				^ std::rotr(a, 22);
			const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
			const std::uint32_t temporary2 = sigma0 + majority;
			h = g;
			g = f;
			f = e;
			e = d + temporary1;
			d = c;
			c = b;
			b = a;
			a = temporary1 + temporary2;
		}
		hash[0] += a;
		hash[1] += b;
		hash[2] += c;
		hash[3] += d;
		hash[4] += e;
		hash[5] += f;
		hash[6] += g;
		hash[7] += h;
	}
	return hash;
}

void
writeAll(const int descriptor, const std::string_view content)
{
	std::size_t offset = 0U;
	while (offset < content.size()) {
		const ssize_t count = ::write(descriptor, content.data() + offset,
			content.size() - offset);
		if (count < 0 && errno == EINTR) continue;
		if (count <= 0) throw std::system_error(errno, std::generic_category(),
			"write evidence temporary file");
		offset += static_cast<std::size_t>(count);
	}
}

void
publishAtomicText(const std::filesystem::path& destination,
	const std::string_view content, const bool force)
{
	std::string pattern = destination.string() + ".tmp.XXXXXX";
	std::vector<char> buffer(pattern.begin(), pattern.end());
	buffer.push_back('\0');
	const int descriptor = ::mkstemp(buffer.data());
	if (descriptor < 0) throw std::system_error(errno, std::generic_category(),
		"create evidence temporary file");
	const std::filesystem::path temporary(buffer.data());
	try {
		writeAll(descriptor, content);
		if (::fsync(descriptor) != 0) throw std::system_error(errno,
			std::generic_category(), "sync evidence temporary file");
		if (::close(descriptor) != 0) throw std::system_error(errno,
			std::generic_category(), "close evidence temporary file");
		const int result = force ? ::rename(temporary.c_str(), destination.c_str())
			: ::link(temporary.c_str(), destination.c_str());
		if (result != 0) throw std::system_error(errno, std::generic_category(),
			"publish evidence file");
		if (!force && ::unlink(temporary.c_str()) != 0) {
			throw std::system_error(errno, std::generic_category(),
				"remove evidence temporary link");
		}
	} catch (...) {
		::close(descriptor);
		::unlink(temporary.c_str());
		throw;
	}
}

} // namespace

std::string_view
hilResultStateName(const HilResultState state) noexcept
{
	switch (state) {
	case HilResultState::notRun: return "not-run";
	case HilResultState::passed: return "passed";
	case HilResultState::failed: return "failed";
	case HilResultState::inconclusive: return "inconclusive";
	case HilResultState::skipped: return "skipped";
	}
	return "invalid";
}

std::string_view
hilEvidenceSourceName(const HilEvidenceSource source) noexcept
{
	switch (source) {
	case HilEvidenceSource::notMeasured: return "not-measured";
	case HilEvidenceSource::operatorObserved: return "operator-observed";
	case HilEvidenceSource::automaticallyMeasured: return "automatically-measured";
	}
	return "invalid";
}

std::string_view
hilStageName(const HilStage stage) noexcept
{
	switch (stage) {
	case HilStage::manifest: return "manifest";
	case HilStage::discovery: return "discovery";
	case HilStage::audioCalibration: return "audio-calibration";
	case HilStage::pttUnkey: return "ptt-unkey";
	case HilStage::keyedSilence: return "keyed-silence";
	case HilStage::fullSstv: return "full-sstv";
	case HilStage::controlledFault: return "controlled-fault";
	case HilStage::guiCompositor: return "gui-compositor";
	}
	return "invalid";
}

HilResourceSet
hilStageResources(const HilStage stage) noexcept
{
	using enum HilResource;
	switch (stage) {
	case HilStage::manifest: return HilResourceSet{};
	case HilStage::discovery:
		return HilResourceSet(static_cast<std::uint32_t>(discovery));
	case HilStage::audioCalibration:
		return HilResourceSet(static_cast<std::uint32_t>(discovery)
			| static_cast<std::uint32_t>(audioInput)
			| static_cast<std::uint32_t>(audioOutput));
	case HilStage::pttUnkey:
		return HilResourceSet(static_cast<std::uint32_t>(pttQuery)
			| static_cast<std::uint32_t>(pttUnkey));
	case HilStage::keyedSilence:
		return HilResourceSet(static_cast<std::uint32_t>(discovery)
			| static_cast<std::uint32_t>(audioOutput)
			| static_cast<std::uint32_t>(pttQuery)
			| static_cast<std::uint32_t>(pttUnkey)
			| static_cast<std::uint32_t>(pttKey));
	case HilStage::fullSstv:
		return HilResourceSet(hilStageResources(HilStage::keyedSilence).bits()
			| static_cast<std::uint32_t>(sstvSignal));
	case HilStage::controlledFault:
		return hilStageResources(HilStage::fullSstv);
	case HilStage::guiCompositor:
		return HilResourceSet(static_cast<std::uint32_t>(gui));
	}
	return HilResourceSet{};
}

std::string
hilStageConfirmationPhrase(const HilStage stage, const std::string_view digest)
{
	const std::size_t prefixLength = std::min<std::size_t>(digest.size(), 12U);
	return "AUTHORIZE M2J " + std::string(hilStageName(stage)) + " "
		+ std::string(digest.substr(0U, prefixLength));
}

HilValidationResult
validateHilEvidence(const HilEvidenceRecord& record)
{
	if (record.schemaVersion != m2jEvidenceSchemaVersion) {
		return makeError(HilErrorCode::invalidRecord, "schema",
			"unsupported M2J evidence schema version");
	}
	const std::array<const std::string*, 16> requiredTexts{
		&record.build.utcStarted, &record.build.gitCommit, &record.build.compiler,
		&record.build.compilerVersion, &record.build.cmakePreset,
		&record.build.miniaudioVersion, &record.build.operatingSystem,
		&record.build.kernel, &record.build.architecture,
		&record.build.redactedHostIdentifier, &record.build.sessionPlatform,
		&record.configuration.audioBackend, &record.configuration.playbackIdentity,
		&record.configuration.identityPersistence, &record.configuration.sstvMode,
		&record.configuration.fixtureSha256};
	for (const std::string* const text : requiredTexts) {
		if (!isBoundedText(*text)) return makeError(HilErrorCode::invalidRecord,
			"metadata", "required evidence text is missing or invalid");
	}
	const std::array<const std::string*, 17> configurationTexts{
		&record.configuration.pttProvider, &record.configuration.pttAddress,
		&record.configuration.radioManufacturer, &record.configuration.radioModel,
		&record.configuration.audioInterface, &record.configuration.cabling,
		&record.configuration.testArrangement, &record.configuration.radioMode,
		&record.configuration.frequency, &record.configuration.power,
		&record.configuration.voxState, &record.configuration.compressorState,
		&record.configuration.timeoutState, &record.configuration.antennaState,
		&record.configuration.sstvMode, &record.configuration.playbackIdentity,
		&record.configuration.identityPersistence};
	for (const std::string* const text : configurationTexts) {
		if (!isBoundedText(*text)) return makeError(HilErrorCode::invalidRecord,
			"configuration", "required configuration text is missing or invalid");
	}
	const std::array<const std::optional<std::string>*, 8> optionalTexts{
		&record.build.utcEnded, &record.build.qtVersion,
		&record.configuration.negotiatedFormat, &record.configuration.flrigPath,
		&record.configuration.radioFirmware,
		&record.configuration.instrumentDescription,
		&record.configuration.calibrationMethod, &record.operatorNotes};
	for (const std::optional<std::string>* const text : optionalTexts) {
		if (text->has_value() && !isBoundedText(**text)) {
			return makeError(HilErrorCode::invalidRecord, "metadata",
				"optional evidence text is invalid or exceeds its bound");
		}
	}
	if (record.build.cmakeOptions.size() > 128U
			|| record.limitations.size() > 64U) {
		return makeError(HilErrorCode::resourceLimit, "metadata",
			"evidence metadata exceeds its configured count bound");
	}
	for (const auto& [key, value] : record.build.cmakeOptions) {
		if (!isBoundedText(key) || !isBoundedText(value)) {
			return makeError(HilErrorCode::invalidRecord, "cmake",
				"CMake evidence option is invalid");
		}
	}
	for (const std::string& limitation : record.limitations) {
		if (!isBoundedText(limitation)) return makeError(HilErrorCode::invalidRecord,
			"limitations", "evidence limitation is invalid");
	}
	if (!isSha256(record.configuration.fixtureSha256)) {
		return makeError(HilErrorCode::invalidRecord, "fixture",
			"fixture SHA-256 must be 64 lowercase hexadecimal characters");
	}
	if (record.build.gitCommit.size() != 40U && record.build.gitCommit.size() != 64U) {
		return makeError(HilErrorCode::invalidRecord, "git",
			"Git commit must be a complete 40 or 64 character object ID");
	}
	for (const char character : record.build.gitCommit) {
		if (!((character >= '0' && character <= '9')
				|| (character >= 'a' && character <= 'f'))) {
			return makeError(HilErrorCode::invalidRecord, "git",
				"Git commit must use lowercase hexadecimal characters");
		}
	}
	if (record.build.utcStarted.back() != 'Z') {
		return makeError(HilErrorCode::invalidRecord, "time",
			"evidence start time must be an explicit UTC timestamp");
	}
	if (record.build.sessionPlatform != "cli"
			&& record.build.sessionPlatform != "wayland"
			&& record.build.sessionPlatform != "xcb-xwayland"
			&& record.build.sessionPlatform != "offscreen-test") {
		return makeError(HilErrorCode::invalidRecord, "session",
			"session platform is not recognized");
	}
	if (record.configuration.pttProvider != "mock"
			&& record.configuration.pttProvider != "flrig"
			&& record.configuration.pttProvider != "rigctld") {
		return makeError(HilErrorCode::invalidRecord, "ptt",
			"PTT provider is not recognized");
	}
	if (record.configuration.pttAddress != "127.0.0.1"
			&& record.configuration.pttAddress != "::1") {
		return makeError(HilErrorCode::invalidRecord, "ptt",
			"PTT evidence endpoint must be literal loopback");
	}
	if (record.configuration.pttPort.has_value() && *record.configuration.pttPort == 0U) {
		return makeError(HilErrorCode::invalidRecord, "ptt",
			"PTT evidence port must be explicit and nonzero, or explicitly unknown");
	}
	if (record.configuration.flrigPath.has_value()
			&& !isBoundedText(*record.configuration.flrigPath)) {
		return makeError(HilErrorCode::invalidRecord, "ptt",
			"flrig XML-RPC path is invalid");
	}
	if (record.configuration.gainDbfs < -60.0
			|| record.configuration.gainDbfs > -6.0) {
		return makeError(HilErrorCode::invalidRecord, "gain",
			"HIL gain must remain within the accepted -60 to -6 dBFS range");
	}
	if (record.configuration.channelCount == 0U
			|| record.configuration.selectedChannel
				>= record.configuration.channelCount) {
		return makeError(HilErrorCode::invalidRecord, "audio",
			"selected channel and channel count are inconsistent");
	}
	if (!std::isfinite(record.configuration.gainDbfs)) {
		return makeError(HilErrorCode::invalidRecord, "gain",
			"gain must be finite");
	}
	if (record.configuration.durationNanoseconds == 0U
			|| record.configuration.frameCount == 0U) {
		return makeError(HilErrorCode::invalidRecord, "transmission",
			"reference duration and frame count must be nonzero");
	}
	if (record.stages.empty() || record.stages.size() > maximumHilStageCount) {
		return makeError(HilErrorCode::resourceLimit, "stages",
			"evidence must contain a bounded stage list");
	}
	if (record.artifacts.size() > maximumHilArtifactCount) {
		return makeError(HilErrorCode::resourceLimit, "artifacts",
			"too many retained artifacts");
	}
	std::uint32_t stageBits = 0U;
	for (const HilStageResult& stage : record.stages) {
		const std::uint32_t bit = 1U << static_cast<std::uint32_t>(stage.stage);
		if ((stageBits & bit) != 0U) return makeError(HilErrorCode::invalidRecord,
			"stages", "duplicate stage result");
		stageBits |= bit;
		if (stage.cleanupErrors.size() > 64U || stage.measurements.size() > 128U) {
			return makeError(HilErrorCode::resourceLimit, "stage",
				"stage evidence exceeds its configured count bound");
		}
		const std::array<const std::optional<std::string>*, 6> stageTexts{
			&stage.startedUtc, &stage.endedUtc, &stage.skippedReason,
			&stage.primaryError, &stage.finalPttCertainty,
			&stage.operatorRadioState};
		for (const std::optional<std::string>* const text : stageTexts) {
			if (text->has_value() && !isBoundedText(**text)) {
				return makeError(HilErrorCode::invalidRecord, "stage",
					"stage text is invalid or exceeds its bound");
			}
		}
		for (const std::string& cleanup : stage.cleanupErrors) {
			if (!isBoundedText(cleanup)) return makeError(
				HilErrorCode::invalidRecord, "stage", "cleanup error text is invalid");
		}
		for (const auto& [key, value] : stage.measurements) {
			if (!isBoundedText(key) || (value.has_value() && !isBoundedText(*value))) {
				return makeError(HilErrorCode::invalidRecord, "stage",
					"measurement evidence is invalid");
			}
		}
		if (stage.state == HilResultState::skipped
				&& (!stage.skippedReason.has_value()
					|| !isBoundedText(*stage.skippedReason))) {
			return makeError(HilErrorCode::invalidRecord, "stage",
				"a skipped stage requires a bounded reason");
		}
		if (isKeyedStage(stage.stage) && stage.state == HilResultState::passed
				&& stage.finalPttCertainty != "definitely-unkeyed") {
			return makeError(HilErrorCode::unsafePttState, "stage",
				"a keyed stage cannot pass without definite unkey confirmation");
		}
	}
	for (const HilArtifact& artifact : record.artifacts) {
		if (!isBoundedText(artifact.name) || !isSha256(artifact.sha256)) {
			return makeError(HilErrorCode::invalidRecord, "artifact",
				"artifact name or SHA-256 is invalid");
		}
		if (artifact.containsRawAudio
				&& artifact.bytes > maximumHilRawCaptureBytes) {
			return makeError(HilErrorCode::resourceLimit, "artifact",
				"raw audio artifact exceeds the configured bound");
		}
	}
	return std::nullopt;
}

HilValidationResult
validateHilStageStart(const HilEvidenceRecord& record, const HilStage stage)
{
	if (const HilValidationResult error = validateHilEvidence(record)) return error;
	if (stage != HilStage::manifest && record.configuration.hasIdentityCollision) {
		return makeError(HilErrorCode::invalidStage, "stage",
			"a colliding exact device identity blocks resource stages");
	}
	if (stage != HilStage::manifest && !hasPassed(record, HilStage::manifest)) {
		return makeError(HilErrorCode::missingPrerequisite, "stage",
			"manifest stage has not passed");
	}
	if (stage == HilStage::audioCalibration
			&& !hasPassed(record, HilStage::discovery)) {
		return makeError(HilErrorCode::missingPrerequisite, "stage",
			"discovery stage has not passed");
	}
	if (stage == HilStage::keyedSilence
			&& (!hasPassed(record, HilStage::audioCalibration)
				|| !hasPassed(record, HilStage::pttUnkey))) {
		return makeError(HilErrorCode::missingPrerequisite, "stage",
			"audio calibration and PTT unkey stages must pass first");
	}
	if (stage == HilStage::fullSstv
			&& !hasPassed(record, HilStage::keyedSilence)) {
		return makeError(HilErrorCode::missingPrerequisite, "stage",
			"keyed-silence stage has not passed");
	}
	if (stage == HilStage::controlledFault
			&& !hasPassed(record, HilStage::fullSstv)) {
		return makeError(HilErrorCode::missingPrerequisite, "stage",
			"full-SSTV stage has not passed");
	}
	for (const HilStageResult& result : record.stages) {
		if (isKeyedStage(result.stage)
				&& result.state != HilResultState::notRun
				&& result.finalPttCertainty != "definitely-unkeyed") {
			return makeError(HilErrorCode::unsafePttState, "stage",
				"an unresolved PTT hazard blocks later keyed stages");
		}
	}
	return std::nullopt;
}

std::string
serializeHilEvidenceJson(const HilEvidenceRecord& record)
{
	if (const HilValidationResult error = validateHilEvidence(record)) {
		throw std::invalid_argument(error->message);
	}
	std::ostringstream output;
	output.imbue(std::locale::classic());
	output << std::setprecision(17);
	output << "{\n  \"schema_version\":" << record.schemaVersion
		<< ",\n  \"build\":{";
	output << "\"utc_started\":"; writeJsonString(output, record.build.utcStarted);
	output << ",\"utc_ended\":"; writeOptionalString(output, record.build.utcEnded);
	output << ",\"git_commit\":"; writeJsonString(output, record.build.gitCommit);
	output << ",\"dirty_worktree\":"
		<< (record.build.isDirtyWorktree ? "true" : "false");
	output << ",\"compiler\":"; writeJsonString(output, record.build.compiler);
	output << ",\"compiler_version\":";
	writeJsonString(output, record.build.compilerVersion);
	output << ",\"cmake_preset\":"; writeJsonString(output, record.build.cmakePreset);
	output << ",\"cmake_options\":"; writeStringMap(output, record.build.cmakeOptions);
	output << ",\"miniaudio_version\":";
	writeJsonString(output, record.build.miniaudioVersion);
	output << ",\"qt_version\":"; writeOptionalString(output, record.build.qtVersion);
	output << ",\"operating_system\":";
	writeJsonString(output, record.build.operatingSystem);
	output << ",\"kernel\":"; writeJsonString(output, record.build.kernel);
	output << ",\"architecture\":"; writeJsonString(output, record.build.architecture);
	output << ",\"redacted_host_id\":";
	writeJsonString(output, record.build.redactedHostIdentifier);
	output << ",\"session_platform\":";
	writeJsonString(output, record.build.sessionPlatform);
	output << "},\n  \"configuration\":{";
	const HilConfiguration& value = record.configuration;
	output << "\"audio_backend\":"; writeJsonString(output, value.audioBackend);
	output << ",\"playback_identity\":";
	writeJsonString(output, value.playbackIdentity);
	output << ",\"identity_persistence\":";
	writeJsonString(output, value.identityPersistence);
	output << ",\"identity_collision\":"
		<< (value.hasIdentityCollision ? "true" : "false");
	output << ",\"selected_channel\":" << value.selectedChannel
		<< ",\"channel_count\":" << value.channelCount;
	output << ",\"negotiated_rate\":";
	if (value.negotiatedRate) output << *value.negotiatedRate; else output << "null";
	output << ",\"negotiated_format\":";
	writeOptionalString(output, value.negotiatedFormat);
	output << ",\"negotiated_channels\":";
	if (value.negotiatedChannels) output << *value.negotiatedChannels;
	else output << "null";
	output << ",\"negotiated_period_frames\":";
	if (value.negotiatedPeriodFrames) output << *value.negotiatedPeriodFrames;
	else output << "null";
	output << ",\"ptt_provider\":"; writeJsonString(output, value.pttProvider);
	output << ",\"ptt_provider_version\":";
	writeOptionalString(output, value.pttProviderVersion);
	output << ",\"ptt_address\":"; writeJsonString(output, value.pttAddress);
	output << ",\"ptt_port\":";
	if (value.pttPort) output << *value.pttPort; else output << "null";
	output << ",\"flrig_path\":";
	writeOptionalString(output, value.flrigPath);
	output << ",\"radio_manufacturer\":";
	writeJsonString(output, value.radioManufacturer);
	output << ",\"radio_model\":"; writeJsonString(output, value.radioModel);
	output << ",\"radio_firmware\":"; writeOptionalString(output, value.radioFirmware);
	output << ",\"audio_interface\":"; writeJsonString(output, value.audioInterface);
	output << ",\"cabling\":"; writeJsonString(output, value.cabling);
	output << ",\"test_arrangement\":";
	writeJsonString(output, value.testArrangement);
	output << ",\"radio_mode\":"; writeJsonString(output, value.radioMode);
	output << ",\"frequency\":"; writeJsonString(output, value.frequency);
	output << ",\"power\":"; writeJsonString(output, value.power);
	output << ",\"vox_state\":"; writeJsonString(output, value.voxState);
	output << ",\"compressor_state\":";
	writeJsonString(output, value.compressorState);
	output << ",\"timeout_state\":"; writeJsonString(output, value.timeoutState);
	output << ",\"antenna_state\":"; writeJsonString(output, value.antennaState);
	output << ",\"sstv_mode\":"; writeJsonString(output, value.sstvMode);
	output << ",\"fixture_sha256\":"; writeJsonString(output, value.fixtureSha256);
	output << ",\"fsk_id_enabled\":"
		<< (value.hasFskIdentifier ? "true" : "false")
		<< ",\"duration_nanoseconds\":" << value.durationNanoseconds
		<< ",\"frame_count\":" << value.frameCount
		<< ",\"gain_dbfs\":" << value.gainDbfs
		<< ",\"pre_key_ms\":" << value.preKeyMilliseconds
		<< ",\"post_audio_ms\":" << value.postAudioMilliseconds;
	output << ",\"instrument\":";
	writeOptionalString(output, value.instrumentDescription);
	output << ",\"calibration_method\":";
	writeOptionalString(output, value.calibrationMethod);
	output << "},\n  \"stages\":[";
	for (std::size_t index = 0U; index < record.stages.size(); ++index) {
		if (index != 0U) output << ',';
		const HilStageResult& stage = record.stages[index];
		output << "{\"stage\":"; writeJsonString(output, hilStageName(stage.stage));
		output << ",\"state\":"; writeJsonString(output,
			hilResultStateName(stage.state));
		output << ",\"source\":"; writeJsonString(output,
			hilEvidenceSourceName(stage.source));
		output << ",\"started_utc\":"; writeOptionalString(output, stage.startedUtc);
		output << ",\"ended_utc\":"; writeOptionalString(output, stage.endedUtc);
		output << ",\"skipped_reason\":";
		writeOptionalString(output, stage.skippedReason);
		output << ",\"primary_error\":";
		writeOptionalString(output, stage.primaryError);
		output << ",\"cleanup_errors\":[";
		for (std::size_t cleanup = 0U; cleanup < stage.cleanupErrors.size(); ++cleanup) {
			if (cleanup != 0U) output << ',';
			writeJsonString(output, stage.cleanupErrors[cleanup]);
		}
		output << "],\"final_ptt_certainty\":";
		writeOptionalString(output, stage.finalPttCertainty);
		output << ",\"measurements\":";
		writeOptionalStringMap(output, stage.measurements);
		output << ",\"operator_radio_state\":";
		writeOptionalString(output, stage.operatorRadioState);
		output << '}';
	}
	output << "],\n  \"artifacts\":[";
	for (std::size_t index = 0U; index < record.artifacts.size(); ++index) {
		if (index != 0U) output << ',';
		const HilArtifact& artifact = record.artifacts[index];
		output << "{\"name\":"; writeJsonString(output, artifact.name);
		output << ",\"sha256\":"; writeJsonString(output, artifact.sha256);
		output << ",\"bytes\":" << artifact.bytes
			<< ",\"raw_audio\":"
			<< (artifact.containsRawAudio ? "true" : "false") << '}';
	}
	output << "],\n  \"limitations\":[";
	for (std::size_t index = 0U; index < record.limitations.size(); ++index) {
		if (index != 0U) output << ',';
		writeJsonString(output, record.limitations[index]);
	}
	output << "],\n  \"operator_notes\":";
	writeOptionalString(output, record.operatorNotes);
	output << ",\n  \"configuration_digest\":";
	writeJsonString(output, calculateHilConfigurationDigest(record.configuration));
	output << "\n}\n";
	const std::string result = output.str();
	if (result.size() > maximumEvidenceBytes) {
		throw std::length_error("serialized evidence exceeds the configured bound");
	}
	return result;
}

std::string
serializeHilEvidenceMarkdown(const HilEvidenceRecord& record)
{
	if (const HilValidationResult error = validateHilEvidence(record)) {
		throw std::invalid_argument(error->message);
	}
	std::ostringstream output;
	output << "# M2J HIL Evidence\n\n"
		<< "Schema: " << record.schemaVersion << "  \n"
		<< "Commit: `" << record.build.gitCommit << "`  \n"
		<< "Dirty worktree: " << (record.build.isDirtyWorktree ? "yes" : "no")
		<< "  \nSession: " << record.build.sessionPlatform << "  \n"
		<< "Mode: " << record.configuration.sstvMode << "  \n"
		<< "Fixture SHA-256: `" << record.configuration.fixtureSha256 << "`\n\n"
		<< "## Configuration\n\n"
		<< "- Audio backend: " << record.configuration.audioBackend << '\n'
		<< "- Playback identity: `" << record.configuration.playbackIdentity << "`\n"
		<< "- Channel: " << record.configuration.selectedChannel << " of "
		<< record.configuration.channelCount << '\n'
		<< "- PTT provider: " << record.configuration.pttProvider << '\n'
		<< "- PTT provider version: "
		<< (record.configuration.pttProviderVersion
			? *record.configuration.pttProviderVersion : "unknown") << '\n'
		<< "- PTT endpoint: " << record.configuration.pttAddress << ':'
		<< (record.configuration.pttPort ? std::to_string(*record.configuration.pttPort) : "unknown") << '\n'
		<< "- Radio: " << record.configuration.radioManufacturer << ' '
		<< record.configuration.radioModel << '\n'
		<< "- Test arrangement: " << record.configuration.testArrangement << "\n\n"
		<< "## Stages\n\n"
		<< "| Stage | Result | Evidence | Final PTT certainty |\n"
		<< "| --- | --- | --- | --- |\n";
	for (const HilStageResult& stage : record.stages) {
		output << "| " << hilStageName(stage.stage) << " | "
			<< hilResultStateName(stage.state) << " | "
			<< hilEvidenceSourceName(stage.source) << " | "
			<< stage.finalPttCertainty.value_or("unknown") << " |\n";
	}
	output << "\n## Limitations\n\n";
	for (const std::string& limitation : record.limitations) {
		output << "- " << limitation << '\n';
	}
	return output.str();
}

std::string
calculateHilConfigurationDigest(const HilConfiguration& configuration)
{
	std::ostringstream output;
	output.imbue(std::locale::classic());
	output << std::setprecision(17) << configuration.audioBackend << '\0'
		<< configuration.playbackIdentity << '\0'
		<< configuration.identityPersistence << '\0'
		<< configuration.hasIdentityCollision << '\0'
		<< configuration.selectedChannel << '\0' << configuration.channelCount << '\0'
		<< configuration.pttProvider << '\0'
		<< configuration.pttProviderVersion.value_or("") << '\0'
		<< configuration.pttAddress << '\0'
		<< (configuration.pttPort ? std::to_string(*configuration.pttPort) : "unknown") << '\0'
		<< configuration.flrigPath.value_or("") << '\0'
		<< configuration.sstvMode << '\0' << configuration.fixtureSha256 << '\0'
		<< configuration.hasFskIdentifier << '\0'
		<< configuration.durationNanoseconds << '\0' << configuration.frameCount << '\0'
		<< configuration.gainDbfs << '\0' << configuration.preKeyMilliseconds << '\0'
		<< configuration.postAudioMilliseconds;
	return calculateHilSha256(output.str());
}

std::string
calculateHilSha256(const std::string_view input)
{
	const auto hash = calculateSha256Words(input);
	std::ostringstream output;
	output << std::hex << std::setfill('0');
	for (const std::uint32_t word : hash) output << std::setw(8) << word;
	return output.str();
}

HilPublicationResult
publishHilEvidence(const HilEvidenceRecord& record,
	const std::filesystem::path& directory, const bool force)
{
	if (directory.empty() || directory.string().size() > maximumHilTextBytes) {
		return makeError(HilErrorCode::invalidPath, "output",
			"evidence output directory is invalid");
	}
	struct stat status {};
	if (::lstat(directory.c_str(), &status) != 0 || !S_ISDIR(status.st_mode)
			|| S_ISLNK(status.st_mode)) {
		return makeError(HilErrorCode::invalidPath, "output",
			"evidence output must be an existing nonsymlink local directory");
	}
	try {
		const std::string json = serializeHilEvidenceJson(record);
		const std::string markdown = serializeHilEvidenceMarkdown(record);
		const std::filesystem::path jsonPath = directory / "m2j-evidence-v1.json";
		const std::filesystem::path markdownPath = directory / "m2j-evidence-v1.md";
		if (!force && (std::filesystem::exists(jsonPath)
				|| std::filesystem::exists(markdownPath))) {
			return makeError(HilErrorCode::destinationExists, "publish",
				"evidence destination already exists");
		}
		publishAtomicText(jsonPath, json, force);
		try {
			publishAtomicText(markdownPath, markdown, force);
		} catch (...) {
			if (!force) std::filesystem::remove(jsonPath);
			throw;
		}
		return HilPublication{jsonPath, markdownPath,
			calculateHilSha256(json), calculateHilSha256(markdown)};
	} catch (const std::exception& error) {
		return makeError(HilErrorCode::publicationFailure, "publish", error.what());
	}
}

namespace {

[[nodiscard]] std::string
readEvidenceText(const std::filesystem::path& path)
{
	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error) || error
			|| std::filesystem::file_size(path, error) > maximumEvidenceBytes || error) {
		throw std::invalid_argument("evidence file is invalid or exceeds the configured limit");
	}
	std::ifstream input(path);
	if (!input) throw std::runtime_error("cannot open evidence file");
	return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

[[nodiscard]] std::size_t
findJsonObjectEnd(const std::string& text, const std::size_t start)
{
	std::size_t depth = 0U;
	bool quoted = false;
	bool escaped = false;
	for (std::size_t index = start; index < text.size(); ++index) {
		const char character = text[index];
		if (quoted) {
			if (escaped) escaped = false;
			else if (character == '\\') escaped = true;
			else if (character == '"') quoted = false;
			continue;
		}
		if (character == '"') quoted = true;
		else if (character == '{') ++depth;
		else if (character == '}' && --depth == 0U) return index + 1U;
	}
	throw std::invalid_argument("evidence JSON stage object is truncated");
}

[[nodiscard]] std::string
serializeStagePatch(const HilStage stage, const HilResultState state,
	const HilEvidenceSource source,
	const std::map<std::string, std::optional<std::string>>& measurements,
	const std::optional<std::string>& error,
	const std::optional<std::string>& finalPttCertainty)
{
	std::ostringstream output;
	output << "{\"stage\":"; writeJsonString(output, hilStageName(stage));
	output << ",\"state\":"; writeJsonString(output, hilResultStateName(state));
	output << ",\"source\":"; writeJsonString(output, hilEvidenceSourceName(source));
	output << ",\"started_utc\":null,\"ended_utc\":null,\"skipped_reason\":null,\"primary_error\":";
	writeOptionalString(output, error);
	output << ",\"cleanup_errors\":[],\"final_ptt_certainty\":";
	writeOptionalString(output, finalPttCertainty);
	output << ",\"measurements\":";
	writeOptionalStringMap(output, measurements);
	output << ",\"operator_radio_state\":null}";
	return output.str();
}

} // namespace

HilPublicationResult
publishHilStageResult(const std::filesystem::path& directory, const HilStage stage,
	const HilResultState state, const HilEvidenceSource source,
	const std::map<std::string, std::optional<std::string>>& measurements,
	const std::optional<std::string> error,
	const std::optional<std::string> finalPttCertainty)
{
	if (stage == HilStage::manifest || stage == HilStage::keyedSilence
			|| stage == HilStage::fullSstv || stage == HilStage::controlledFault) {
		return makeError(HilErrorCode::invalidStage, "publish-stage",
			"this helper only records non-keyed readiness stages");
	}
	try {
		const std::filesystem::path jsonPath = directory / "m2j-evidence-v1.json";
		const std::filesystem::path markdownPath = directory / "m2j-evidence-v1.md";
		std::string json = readEvidenceText(jsonPath);
		const std::string marker = "{\"stage\":\"" + std::string(hilStageName(stage)) + "\"";
		const std::size_t markerPosition = json.find(marker);
		if (markerPosition == std::string::npos) {
			return makeError(HilErrorCode::invalidRecord, "publish-stage", "stage is missing");
		}
		const std::size_t objectStart = json.rfind('{', markerPosition);
		const std::size_t objectEnd = findJsonObjectEnd(json, objectStart);
		json.replace(objectStart, objectEnd - objectStart,
			serializeStagePatch(stage, state, source, measurements, error,
				finalPttCertainty));
		std::string markdown = readEvidenceText(markdownPath);
		const std::string rowMarker = "| " + std::string(hilStageName(stage)) + " |";
		const std::size_t rowStart = markdown.find(rowMarker);
		if (rowStart == std::string::npos) {
			return makeError(HilErrorCode::invalidRecord, "publish-stage", "stage row is missing");
		}
		const std::size_t rowEnd = markdown.find('\n', rowStart);
		const std::string replacement = "| " + std::string(hilStageName(stage)) + " | "
			+ std::string(hilResultStateName(state)) + " | "
			+ std::string(hilEvidenceSourceName(source)) + " | unknown |";
		markdown.replace(rowStart, rowEnd == std::string::npos ? markdown.size() - rowStart
			: rowEnd - rowStart, replacement);
		publishAtomicText(jsonPath, json, true);
		publishAtomicText(markdownPath, markdown, true);
		return HilPublication{jsonPath, markdownPath, calculateHilSha256(json),
			calculateHilSha256(markdown)};
	} catch (const std::exception& caught) {
		return makeError(HilErrorCode::publicationFailure, "publish-stage", caught.what());
	}
}

HilPermitResult
HilStageAuthorizer::authorize(const HilStage stage, const std::string_view digest,
	const std::string_view phrase, const bool hasChecklist)
{
	if (!isSha256(std::string(digest))) return makeError(HilErrorCode::invalidConfirmation,
		"authorize", "configuration digest is invalid");
	if (stage != HilStage::manifest && !hasChecklist) {
		return makeError(HilErrorCode::invalidConfirmation, "authorize",
			"the stage checklist was not affirmed");
	}
	if (stage != HilStage::manifest
			&& phrase != hilStageConfirmationPhrase(stage, digest)) {
		return makeError(HilErrorCode::invalidConfirmation, "authorize",
			"stage confirmation phrase does not match");
	}
	permit_ = HilStagePermit{stage, std::string(digest), nextNonce_++};
	return *permit_;
}

HilValidationResult
HilStageAuthorizer::consume(const HilStagePermit& permit, const HilStage stage,
	const std::string_view digest)
{
	if (!permit_.has_value()) return makeError(HilErrorCode::consumedPermit,
		"consume", "stage permit is absent or already consumed");
	if (permit.nonce != permit_->nonce || permit.stage != stage
			|| permit.configurationDigest != digest
			|| permit_->configurationDigest != digest) {
		return makeError(HilErrorCode::stalePermit, "consume",
			"stage permit does not match the current configuration");
	}
	permit_.reset();
	return std::nullopt;
}

void
HilStageAuthorizer::invalidate() noexcept
{
	permit_.reset();
}

} // namespace sstv::app
