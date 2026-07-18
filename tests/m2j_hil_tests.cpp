// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m2j_hil_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/offline_tx.hpp>
#include <sstv/app/hil_evidence.hpp>
#include <sstv/app/rendered_transmit.hpp>
#include <sstv/core/mode.hpp>
#include <sstv/core/rgb8_frame.hpp>
#include <sstv/dsp/tone_renderer.hpp>
#include <sstv/image/image.hpp>

#include <sys/stat.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace {

void
require(const bool condition, const std::string& message)
{
	if (!condition) throw std::runtime_error(message);
}

class TemporaryDirectory final {
public:
	TemporaryDirectory()
	{
		std::string pattern = (std::filesystem::temp_directory_path()
			/ "scanline-sstv-m2j-XXXXXX").string();
		std::vector<char> buffer(pattern.begin(), pattern.end());
		buffer.push_back('\0');
		const char* const result = ::mkdtemp(buffer.data());
		if (result == nullptr) throw std::runtime_error("cannot create M2J temporary directory");
		path_ = result;
	}
	~TemporaryDirectory() { std::filesystem::remove_all(path_); }
	[[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }
private:
	std::filesystem::path path_;
};

[[nodiscard]] sstv::app::HilStageResult
makePendingStage(const sstv::app::HilStage stage)
{
	sstv::app::HilStageResult result;
	result.stage = stage;
	return result;
}

[[nodiscard]] sstv::app::HilEvidenceRecord
makeRecord()
{
	using namespace sstv::app;
	HilEvidenceRecord record;
	record.build = {"2026-07-18T12:00:00Z", std::nullopt,
		"0123456789abcdef0123456789abcdef01234567", false, "Clang", "18.1.8",
		"live-tx-gui-compile", {{"SSTV_ENABLE_LIVE_TX", "ON"}}, "0.11.25",
		std::string("6.11.1"), "Linux", "6.12-test", "x86_64",
		"sha256:host-redacted", "offscreen-test"};
	record.configuration = {"mock", "mock:playback:fixture", "session-only", false,
		0U, 2U, std::nullopt, std::nullopt, std::nullopt, std::nullopt,
		"mock", "127.0.0.1", 12345U, std::nullopt, "operator-required",
		"operator-required", std::nullopt, "operator-required", "operator-required",
		"dummy-load-required", "operator-required", "operator-required",
		"operator-required", "disabled", "operator-required", "operator-required",
		"disconnected", "robot-36",
		"2ab0388b27325de68bdb3246cb4eaa043ba85bf27d65f3aebb5d0ba164cbc9d2",
		false, 36'910'000'000ULL, 1'771'680ULL, -60.0, 250U, 250U,
		std::nullopt, std::nullopt};
	record.stages = {
		{HilStage::manifest, HilResultState::passed,
			HilEvidenceSource::automaticallyMeasured, record.build.utcStarted,
			record.build.utcStarted, std::nullopt, std::nullopt, {}, std::nullopt,
			{{"resource_acquisitions", std::string("0")}}, std::nullopt},
		makePendingStage(HilStage::discovery),
		makePendingStage(HilStage::audioCalibration),
		makePendingStage(HilStage::pttUnkey),
		makePendingStage(HilStage::keyedSilence),
		makePendingStage(HilStage::fullSstv),
		makePendingStage(HilStage::controlledFault),
		makePendingStage(HilStage::guiCompositor),
	};
	record.limitations = {"No physical HIL was performed.",
		"SIGKILL and power loss are outside in-process cleanup guarantees."};
	return record;
}

void
testSchemaAndSerialization()
{
	using namespace sstv::app;
	HilEvidenceRecord record = makeRecord();
	require(!validateHilEvidence(record), "valid evidence record was rejected");
	const std::string first = serializeHilEvidenceJson(record);
	const std::string second = serializeHilEvidenceJson(record);
	require(first == second, "evidence serialization is not deterministic");
	require(first.find("\"negotiated_rate\":null") != std::string::npos,
		"unknown negotiated value was not explicit");
	require(first.find("\"operator-observed\"") == std::string::npos,
		"unrecorded operator evidence was invented");
	require(calculateHilSha256("abc")
		== "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
		"SHA-256 implementation does not match the standard vector");
	const std::array<HilResultState, 5> states{HilResultState::notRun,
		HilResultState::passed, HilResultState::failed,
		HilResultState::inconclusive, HilResultState::skipped};
	for (const HilResultState state : states) {
		require(hilResultStateName(state) != "invalid", "result state is not represented");
	}
	record.stages[1].state = HilResultState::skipped;
	record.stages[1].source = HilEvidenceSource::operatorObserved;
	record.stages[1].skippedReason = "operator did not run discovery";
	record.stages[2].state = HilResultState::failed;
	record.stages[2].source = HilEvidenceSource::automaticallyMeasured;
	record.stages[2].primaryError = "injected failure";
	record.stages[3].state = HilResultState::inconclusive;
	record.stages[3].source = HilEvidenceSource::operatorObserved;
	const std::string allStates = serializeHilEvidenceJson(record);
	for (const HilResultState state : states) {
		require(allStates.find(std::string(hilResultStateName(state)))
			!= std::string::npos, "serialized evidence omitted a result state");
	}
	require(allStates.find("operator-observed") != std::string::npos
		&& allStates.find("automatically-measured") != std::string::npos,
		"evidence source classes were not serialized separately");
	record.schemaVersion = 2U;
	require(validateHilEvidence(record).has_value(), "unknown schema version was accepted");
	record = makeRecord();
	record.stages[4].state = HilResultState::passed;
	record.stages[4].source = HilEvidenceSource::automaticallyMeasured;
	require(validateHilEvidence(record).has_value(),
		"keyed stage passed without definite unkey certainty");
	record.stages[4].finalPttCertainty = "definitely-unkeyed";
	require(!validateHilEvidence(record), "safe keyed stage evidence was rejected");
}

void
testStagePolicyAndAuthorization()
{
	using namespace sstv::app;
	HilEvidenceRecord record = makeRecord();
	require(hilStageResources(HilStage::manifest).bits() == 0U,
		"manifest stage can acquire a resource");
	require(hilStageResources(HilStage::discovery).contains(HilResource::discovery),
		"discovery stage lacks discovery permission");
	require(!hilStageResources(HilStage::discovery).contains(HilResource::audioOutput)
		&& !hilStageResources(HilStage::discovery).contains(HilResource::pttQuery),
		"discovery stage can acquire higher-risk resources");
	require(hilStageResources(HilStage::pttUnkey).contains(HilResource::pttUnkey)
		&& !hilStageResources(HilStage::pttUnkey).contains(HilResource::pttKey)
		&& !hilStageResources(HilStage::pttUnkey).contains(HilResource::audioOutput),
		"PTT-unkey resource boundary is unsafe");
	require(hilStageResources(HilStage::fullSstv).contains(HilResource::sstvSignal),
		"full-SSTV stage lacks its explicit signal permission");
	require(!validateHilStageStart(record, HilStage::discovery),
		"discovery did not accept the passed manifest prerequisite");
	require(validateHilStageStart(record, HilStage::audioCalibration).has_value(),
		"audio calibration skipped discovery");
	const std::string digest = calculateHilConfigurationDigest(record.configuration);
	HilStageAuthorizer authorizer;
	const HilPermitResult rejected = authorizer.authorize(HilStage::audioCalibration,
		digest, "wrong", true);
	require(std::holds_alternative<HilError>(rejected),
		"incorrect stage confirmation was accepted");
	const std::string phrase = hilStageConfirmationPhrase(
		HilStage::audioCalibration, digest);
	const HilPermitResult accepted = authorizer.authorize(
		HilStage::audioCalibration, digest, phrase, true);
	require(std::holds_alternative<HilStagePermit>(accepted),
		"valid stage confirmation was rejected");
	const HilStagePermit permit = std::get<HilStagePermit>(accepted);
	require(authorizer.consume(permit, HilStage::pttUnkey, digest).has_value(),
		"permit authorized a different stage");
	require(!authorizer.consume(permit, HilStage::audioCalibration, digest),
		"valid permit was not consumed");
	require(authorizer.consume(permit, HilStage::audioCalibration, digest).has_value(),
		"single-use permit was reused");
	const HilPermitResult stale = authorizer.authorize(HilStage::audioCalibration,
		digest, phrase, true);
	require(std::holds_alternative<HilStagePermit>(stale), "second permit failed");
	const HilStagePermit stalePermit = std::get<HilStagePermit>(stale);
	require(authorizer.consume(stalePermit, HilStage::audioCalibration,
		calculateHilSha256("changed")).has_value(),
		"configuration change did not invalidate authorization");
	record.stages[4].state = HilResultState::failed;
	record.stages[4].source = HilEvidenceSource::automaticallyMeasured;
	record.stages[4].finalPttCertainty = "indeterminate";
	require(validateHilStageStart(record, HilStage::fullSstv).has_value(),
		"unresolved PTT hazard did not block a later keyed stage");
}

void
testAtomicPublication()
{
	using namespace sstv::app;
	const TemporaryDirectory temporary;
	const HilEvidenceRecord record = makeRecord();
	const HilPublicationResult first = publishHilEvidence(record, temporary.path(), false);
	require(std::holds_alternative<HilPublication>(first),
		"initial evidence publication failed");
	const HilPublication& publication = std::get<HilPublication>(first);
	require(std::filesystem::is_regular_file(publication.jsonPath)
		&& std::filesystem::is_regular_file(publication.markdownPath),
		"published evidence files are missing");
	require(std::holds_alternative<HilError>(
		publishHilEvidence(record, temporary.path(), false)),
		"evidence overwrite was not refused");
	require(std::holds_alternative<HilPublication>(
		publishHilEvidence(record, temporary.path(), true)),
		"explicit evidence replacement failed");
	for (const auto& entry : std::filesystem::directory_iterator(temporary.path())) {
		require(entry.path().filename().string().find(".tmp.") == std::string::npos,
			"evidence publication abandoned a temporary file");
	}
	const std::filesystem::path link = temporary.path() / "output-link";
	std::filesystem::create_directory(temporary.path() / "real-output");
	std::filesystem::create_directory_symlink(temporary.path() / "real-output", link);
	require(std::holds_alternative<HilError>(publishHilEvidence(record, link, false)),
		"symlink evidence directory was accepted");
	HilEvidenceRecord oversized = record;
	oversized.artifacts.push_back({"raw-capture.wav",
		std::string(64U, 'a'), maximumHilRawCaptureBytes + 1U, true});
	require(validateHilEvidence(oversized).has_value(),
		"oversized raw capture metadata was accepted");
}

void
testReferenceWaveform()
{
	using namespace sstv;
	const core::ModeDescriptor* const mode = core::find_mode("robot-36");
	require(mode != nullptr, "Robot 36 mode is unavailable");
	const core::Rgb8Frame frame = core::makeDiagnosticPattern(mode->width, mode->height);
	const core::Rgb8View frameView = frame.view();
	const image::ImageRecipe recipe{mode->width, mode->height,
		image::FitMode::contain, std::nullopt, {0U, 0U, 0U}, true};
	const std::filesystem::path fixturePath
		= std::filesystem::path(SSTV_HIL_FIXTURE_DIR) / "robot-36-reference.png";
	std::ifstream fixtureStream(fixturePath, std::ios::binary);
	const std::string fixtureBytes((std::istreambuf_iterator<char>(fixtureStream)),
		std::istreambuf_iterator<char>());
	require(app::calculateHilSha256(fixtureBytes)
		== "1676228514e96b46f144f05875ec7c74e3e16dca4d98291ea7f5d93658c1a1b7",
		"M2J reference PNG hash changed");
	const image::ImageResult<image::PreparedImage> prepared
		= image::prepareRasterImage(fixturePath,
			recipe, image::defaultImageLoadLimits());
	require(std::holds_alternative<image::PreparedImage>(prepared),
		"M2J reference PNG preparation failed");
	const core::Rgb8View preparedView
		= std::get<image::PreparedImage>(prepared).frame.view();
	require(std::ranges::equal(frameView.pixels(), preparedView.pixels(),
		[](const core::Rgb8Pixel left, const core::Rgb8Pixel right) {
			return left.red == right.red && left.green == right.green
				&& left.blue == right.blue;
		}), "M2J reference PNG differs from the accepted diagnostic frame");
	std::string rgbBytes;
	rgbBytes.reserve(frameView.pixels().size() * 3U);
	for (const core::Rgb8Pixel pixel : frameView.pixels()) {
		rgbBytes.push_back(static_cast<char>(pixel.red));
		rgbBytes.push_back(static_cast<char>(pixel.green));
		rgbBytes.push_back(static_cast<char>(pixel.blue));
	}
	const std::string fixtureHash = app::calculateHilSha256(rgbBytes);
	require(fixtureHash
		== "2ab0388b27325de68bdb3246cb4eaa043ba85bf27d65f3aebb5d0ba164cbc9d2",
		"M2J reference image hash changed: " + fixtureHash);
	analog::OfflineTxResult encoded = analog::encodeOfflineTransmission("robot-36",
		core::ModeCapability::offlineTestPatternTx, frame.view(),
		analog::OfflineTransmissionOptions{0.8F, std::nullopt});
	require(std::holds_alternative<analog::OfflineTransmission>(encoded),
		"M2J Robot 36 reference encoding failed");
	const analog::OfflineTransmission transmission
		= std::get<analog::OfflineTransmission>(std::move(encoded));
	analog::OfflineTxResult preparedEncoded = analog::encodeOfflineTransmission("robot-36",
		core::ModeCapability::offlineImageTx, preparedView,
		analog::OfflineTransmissionOptions{0.8F, std::nullopt});
	require(std::holds_alternative<analog::OfflineTransmission>(preparedEncoded),
		"M2J prepared reference encoding failed");
	const analog::OfflineTransmission& preparedTransmission
		= std::get<analog::OfflineTransmission>(preparedEncoded);
	require(std::ranges::equal(preparedTransmission.events, transmission.events,
		[](const core::ToneEvent& left, const core::ToneEvent& right) {
			return left.duration() == right.duration()
				&& left.frequencyHz() == right.frequencyHz()
				&& left.amplitude() == right.amplitude();
		})
		&& preparedTransmission.duration == transmission.duration,
		"M2J prepared and direct event sources differ");
	dsp::ToneRenderer direct(transmission.events, 48'000U);
	app::FiniteSampleSourceCreateResult created
		= app::createToneEventSampleSource(transmission, 48'000U, 0.001F);
	require(std::holds_alternative<std::unique_ptr<app::FiniteSampleSource>>(created),
		"M2J reference streaming source failed");
	std::unique_ptr<app::FiniteSampleSource> source
		= std::move(std::get<std::unique_ptr<app::FiniteSampleSource>>(created));
	require(source->facts().frameCount == direct.frameCount(),
		"M2J reference frame projection differs from the accepted renderer");
	std::vector<float> directBlock(257U);
	std::vector<float> sourceBlock(257U);
	std::vector<float> expectedSamples;
	expectedSamples.reserve(static_cast<std::size_t>(source->facts().frameCount));
	std::uint64_t compared = 0U;
	while (!direct.finished()) {
		const std::size_t directCount = direct.render(directBlock);
		const app::SampleReadResult read = source->read(sourceBlock);
		require(std::holds_alternative<std::size_t>(read),
			"M2J reference source read failed");
		const std::size_t sourceCount = std::get<std::size_t>(read);
		require(sourceCount == directCount, "M2J source block size changed output");
		for (std::size_t index = 0U; index < directCount; ++index) {
			require(sourceBlock[index] == directBlock[index] * 0.001F,
				"M2J source sample is not exact accepted sample times gain");
		}
		expectedSamples.insert(expectedSamples.end(), sourceBlock.begin(),
			sourceBlock.begin() + static_cast<std::ptrdiff_t>(sourceCount));
		compared += directCount;
	}
	require(compared == source->facts().frameCount && source->isExhausted(),
		"M2J reference source omitted or duplicated frames");
	app::FiniteSampleSourceCreateResult blockCreated
		= app::createToneEventSampleSource(transmission, 48'000U, 0.001F);
	require(std::holds_alternative<std::unique_ptr<app::FiniteSampleSource>>(blockCreated),
		"M2J alternate-block source failed");
	std::unique_ptr<app::FiniteSampleSource> blockSource
		= std::move(std::get<std::unique_ptr<app::FiniteSampleSource>>(blockCreated));
	std::vector<float> largeBlock(4'096U);
	std::size_t expectedOffset = 0U;
	while (!blockSource->isExhausted()) {
		const app::SampleReadResult read = blockSource->read(largeBlock);
		require(std::holds_alternative<std::size_t>(read),
			"M2J alternate-block source read failed");
		const std::size_t count = std::get<std::size_t>(read);
		require(expectedOffset + count <= expectedSamples.size(),
			"M2J alternate block exceeded the expected frame count");
		require(std::equal(largeBlock.begin(),
			largeBlock.begin() + static_cast<std::ptrdiff_t>(count),
			expectedSamples.begin() + static_cast<std::ptrdiff_t>(expectedOffset)),
			"M2J source output changed with pull block size");
		expectedOffset += count;
	}
	require(expectedOffset == expectedSamples.size(),
		"M2J alternate block source omitted frames");
}

} // namespace

int
main()
{
	testSchemaAndSerialization();
	testStagePolicyAndAuthorization();
	testAtomicPublication();
	testReferenceWaveform();
	return 0;
}
