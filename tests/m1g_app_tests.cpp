// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m1g_app_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/fsk_id.hpp>
#include <sstv/app/offline_tx_editor.hpp>
#include <sstv/core/timing.hpp>
#include <sstv/offline/wav_writer.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <tuple>
#include <variant>

namespace {

int failures = 0;

void
expect(const bool condition, const std::string& message)
{
	if (condition) return;
	std::cerr << "FAIL: " << message << '\n';
	++failures;
}

[[nodiscard]] std::filesystem::path
makeTemporaryDirectory()
{
	const std::filesystem::path base = std::filesystem::temp_directory_path();
	for (std::uint64_t index = 0; index < 1000U; ++index) {
		const auto path = base / ("scanline-sstv-m1g-" + std::to_string(index));
		std::error_code error;
		if (std::filesystem::create_directory(path, error)) return path;
	}
	throw std::runtime_error("cannot create M1G temporary directory");
}

[[nodiscard]] sstv::app::OfflineEditorRequest
makeRequest(const std::filesystem::path& input, const std::string& mode,
	const std::uint32_t sampleRate, const std::uint64_t revision,
	std::optional<sstv::analog::FskIdentifier> fsk = std::nullopt)
{
	return sstv::app::OfflineEditorRequest{revision, mode, input,
		sstv::image::FitMode::contain, std::nullopt, {0U, 0U, 0U},
		sampleRate, std::move(fsk)};
}

[[nodiscard]] std::shared_ptr<const sstv::app::OfflineEditorSnapshot>
expectSnapshot(const sstv::app::SnapshotResult& result, const std::string& context)
{
	if (const auto* error = std::get_if<sstv::app::EditorError>(&result)) {
		expect(false, context + ": " + error->message);
		return {};
	}
	return std::get<std::shared_ptr<const sstv::app::OfflineEditorSnapshot>>(result);
}

void
testModeRegistry()
{
	const auto result = sstv::app::offlineImageModes();
	expect(std::holds_alternative<std::vector<sstv::app::OfflineModeInfo>>(result),
		"offline mode list succeeds");
	if (!std::holds_alternative<std::vector<sstv::app::OfflineModeInfo>>(result)) return;
	const auto& modes = std::get<std::vector<sstv::app::OfflineModeInfo>>(result);
	expect(modes.size() == 4U, "four offline image modes are available");
	const std::array expected{
		std::tuple{"martin-m1", 320U, 256U},
		std::tuple{"scottie-s1", 320U, 256U},
		std::tuple{"robot-36", 320U, 240U},
		std::tuple{"pd-120", 640U, 496U},
	};
	for (const auto& [id, width, height] : expected) {
		const auto iterator = std::find_if(modes.begin(), modes.end(),
			[id](const auto& mode) { return mode.id == id; });
		expect(iterator != modes.end(), std::string(id) + " is listed");
		if (iterator == modes.end()) continue;
		expect(iterator->width == width && iterator->height == height,
			std::string(id) + " dimensions are registry-derived");
		expect(iterator->baseDuration.numerator() > 0U,
			std::string(id) + " exposes exact base duration");
	}
}

void
testGenerationGate()
{
	sstv::app::GenerationGate gate;
	const std::uint64_t first = gate.issue();
	const std::uint64_t second = gate.issue();
	expect(!gate.isCurrent(first), "older generation is stale");
	expect(gate.isCurrent(second), "newest generation is current");
}

void
testPreparationAndExport(const std::filesystem::path& directory)
{
	const std::filesystem::path input = SSTV_IMAGE_FIXTURE_DIR "/marker.png";
	for (const std::string mode : {"martin-m1", "scottie-s1", "robot-36", "pd-120"}) {
		for (const std::uint32_t rate : sstv::offline::supportedSampleRates()) {
			const auto snapshot = expectSnapshot(sstv::app::prepareOfflineEditor(
				makeRequest(input, mode, rate, rate)), mode + " preparation");
			if (!snapshot) continue;
			expect(snapshot->prepared.frame.view().width() == snapshot->mode.width
				&& snapshot->prepared.frame.view().height() == snapshot->mode.height,
				mode + " exact prepared dimensions");
			expect(snapshot->frameCount == sstv::core::sampleCount(
				snapshot->mode.baseDuration, rate), mode + " exact frame projection at "
					+ std::to_string(rate));
		}
	}
	const auto fskValue = sstv::analog::validateFskIdentifier("m6vpn");
	expect(std::holds_alternative<sstv::analog::FskIdentifier>(fskValue),
		"test FSK identifier validates");
	if (!std::holds_alternative<sstv::analog::FskIdentifier>(fskValue)) return;
	const auto base = expectSnapshot(sstv::app::prepareOfflineEditor(
		makeRequest(input, "martin-m1", 48'000U, 10U)), "base preparation");
	auto coverRequest = makeRequest(
		SSTV_IMAGE_FIXTURE_DIR "/marker.jpg", "martin-m1", 48'000U, 12U);
	coverRequest.fit = sstv::image::FitMode::cover;
	const auto cover = expectSnapshot(sstv::app::prepareOfflineEditor(coverRequest),
		"JPEG cover preparation");
	expect(cover && cover->prepared.fit == sstv::image::FitMode::cover,
		"JPEG cover recipe reaches the app service");
	auto cropRequest = makeRequest(input, "martin-m1", 48'000U, 13U);
	cropRequest.crop = sstv::image::CropRect{1U, 0U, 3U, 3U};
	cropRequest.background = {0x12U, 0x34U, 0x56U};
	const auto cropped = expectSnapshot(sstv::app::prepareOfflineEditor(cropRequest),
		"cropped background preparation");
	const auto repeated = expectSnapshot(sstv::app::prepareOfflineEditor(cropRequest),
		"repeated cropped preparation");
	if (cropped && repeated) {
		const auto& appliedCrop = cropped->prepared.appliedCrop;
		expect(appliedCrop.has_value() && appliedCrop->x == 1U
			&& appliedCrop->y == 0U && appliedCrop->width == 3U
			&& appliedCrop->height == 3U, "oriented-source crop is recorded");
		const auto pixels = cropped->prepared.frame.view().pixels();
		const auto repeatedPixels = repeated->prepared.frame.view().pixels();
		expect(pixels.size() == repeatedPixels.size(),
			"repeated preparation has the same pixel count");
		bool pixelsMatch = pixels.size() == repeatedPixels.size();
		for (std::size_t index = 0; pixelsMatch && index < pixels.size(); ++index) {
			pixelsMatch = pixels[index].red == repeatedPixels[index].red
				&& pixels[index].green == repeatedPixels[index].green
				&& pixels[index].blue == repeatedPixels[index].blue;
		}
		expect(pixelsMatch, "repeated preparation is pixel-deterministic");
		expect(!pixels.empty() && pixels.front().red == 0x12U
			&& pixels.front().green == 0x34U && pixels.front().blue == 0x56U,
			"contain preparation uses the requested background bars");
	}
	const auto withFsk = expectSnapshot(sstv::app::prepareOfflineEditor(
		makeRequest(input, "martin-m1", 48'000U, 11U,
			std::get<sstv::analog::FskIdentifier>(fskValue))), "FSK preparation");
	if (!base || !withFsk) return;
	expect(withFsk->fskIdentifier == std::optional<std::string>{"M6VPN"},
		"normalized FSK ID is exposed");
	expect(withFsk->frameCount > base->frameCount,
		"combined FSK duration increases projected frames");
	const std::filesystem::path png = directory / "prepared.png";
	const auto pngResult = sstv::app::exportPreparedPng(*base, png, false);
	expect(std::holds_alternative<sstv::image::PngMetadata>(pngResult),
		"prepared PNG exports atomically");
	const auto refusedPng = sstv::app::exportPreparedPng(*base, png, false);
	expect(std::holds_alternative<sstv::app::EditorError>(refusedPng)
		&& std::get<sstv::app::EditorError>(refusedPng).code
			== sstv::app::EditorErrorCode::destinationExists,
		"PNG overwrite requires confirmation");
	expect(std::holds_alternative<sstv::image::PngMetadata>(
		sstv::app::exportPreparedPng(*base, png, true)), "confirmed PNG replacement succeeds");
	const std::filesystem::path wav = directory / "offline.wav";
	const auto wavResult = sstv::app::exportOfflineWav(*base, wav, false);
	expect(std::holds_alternative<sstv::app::WavExportResult>(wavResult),
		"offline WAV exports atomically");
	if (const auto* result = std::get_if<sstv::app::WavExportResult>(&wavResult)) {
		expect(result->inspection.frameCount == base->frameCount,
			"export inspects the actual published WAV");
		expect(result->inspection.sampleRate == 48'000U,
			"published WAV inspection reports selected rate");
	}
	expect(std::holds_alternative<sstv::offline::WavInspection>(
		sstv::app::inspectOfflineWav(wav)), "standalone app inspection succeeds");
	const auto wrongInput = sstv::app::prepareOfflineEditor(
		makeRequest(directory / "missing.png", "martin-m1", 48'000U, 20U));
	expect(std::holds_alternative<sstv::app::EditorError>(wrongInput),
		"missing input returns typed error");
	const auto wrongMode = sstv::app::prepareOfflineEditor(
		makeRequest(input, "unknown", 48'000U, 21U));
	expect(std::holds_alternative<sstv::app::EditorError>(wrongMode),
		"invalid mode returns typed error");
	const auto wrongRate = sstv::app::prepareOfflineEditor(
		makeRequest(input, "martin-m1", 12'345U, 22U));
	expect(std::holds_alternative<sstv::app::EditorError>(wrongRate),
		"invalid sample rate returns typed error");
}

} // namespace

int
main()
{
	const std::filesystem::path directory = makeTemporaryDirectory();
	testModeRegistry();
	testGenerationGate();
	testPreparationAndExport(directory);
	std::error_code error;
	std::filesystem::remove_all(directory, error);
	if (failures != 0) return 1;
	std::cout << "M1G app service tests passed\n";
	return 0;
}
