// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/app/offline_tx_editor.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/app/offline_tx_editor.hpp>

#include <sstv/core/mode.hpp>
#include <sstv/core/timing.hpp>
#include <sstv/offline/wav_writer.hpp>

#include <limits>

namespace sstv::app {
namespace {

[[nodiscard]] std::string
colourName(const core::ColourEncoding value)
{
	switch (value) {
	case core::ColourEncoding::monochrome: return "monochrome";
	case core::ColourEncoding::rgb: return "RGB";
	case core::ColourEncoding::lumaColourDifference: return "luma / red-blue difference";
	case core::ColourEncoding::digital_payload: return "digital payload";
	}
	return "unknown";
}

[[nodiscard]] std::variant<OfflineModeInfo, EditorError>
describe(const core::ModeDescriptor& mode)
{
	const auto duration = analog::offlineTransmissionDuration(
	    mode.id, core::ModeCapability::offlineImageTx);
	if (const auto* error = std::get_if<analog::OfflineTxError>(&duration)) {
		return EditorError{EditorErrorCode::invalidMode, "describe mode", error->message};
	}
	return OfflineModeInfo{std::string(mode.id), std::string(mode.display_name),
		colourName(mode.colour_encoding), mode.width, mode.height, mode.vis_code,
		std::get<core::Duration>(duration)};
}

[[nodiscard]] std::optional<EditorError>
validateOutput(const OfflineEditorSnapshot& snapshot,
	const std::filesystem::path& destination, const bool replace)
{
	if (destination.empty() || destination.string().find("://") != std::string::npos) {
		return EditorError{EditorErrorCode::unsafeOutput, "validate output",
			"output must be a local filesystem path"};
	}
	std::error_code error;
	if (std::filesystem::exists(destination, error)) {
		if (std::filesystem::equivalent(snapshot.input, destination, error)) {
			return EditorError{EditorErrorCode::unsafeOutput, "validate output",
				"input and output must be different files"};
		}
		if (!replace) {
			return EditorError{EditorErrorCode::destinationExists, "validate output",
				"output already exists; confirmation is required"};
		}
		if (!std::filesystem::is_regular_file(destination, error)
		    || std::filesystem::is_symlink(destination, error)) {
			return EditorError{EditorErrorCode::unsafeOutput, "validate output",
				"existing output must be a nonsymlink regular file"};
		}
	}
	if (error) return EditorError{EditorErrorCode::unsafeOutput, "validate output", error.message()};
	const std::filesystem::path parent = destination.parent_path().empty()
	    ? std::filesystem::path(".") : destination.parent_path();
	if (!std::filesystem::is_directory(parent, error) || error) {
		return EditorError{EditorErrorCode::unsafeOutput, "validate output",
			"output parent must be an existing directory"};
	}
	return std::nullopt;
}

} // namespace

std::uint64_t
GenerationGate::issue() noexcept { return ++current; }

bool
GenerationGate::isCurrent(const std::uint64_t value) const noexcept
{
	return current.load() == value;
}

ModeListResult
offlineImageModes()
{
	std::vector<OfflineModeInfo> result;
	for (const core::ModeDescriptor& mode : core::built_in_modes()) {
		if (!mode.capabilities.contains(core::ModeCapability::offlineImageTx)) continue;
		auto value = describe(mode);
		if (const auto* error = std::get_if<EditorError>(&value)) return *error;
		result.push_back(std::get<OfflineModeInfo>(std::move(value)));
	}
	return result;
}

SnapshotResult
prepareOfflineEditor(const OfflineEditorRequest& request)
{
	const core::ModeDescriptor* mode = core::find_mode(request.modeId);
	if (mode == nullptr || !mode->capabilities.contains(core::ModeCapability::offlineImageTx)) {
		return EditorError{EditorErrorCode::invalidMode, "prepare editor",
			"mode does not support offline image TX"};
	}
	if (!offline::isSupportedSampleRate(request.sampleRate)) {
		return EditorError{EditorErrorCode::invalidRequest, "prepare editor", "unsupported sample rate"};
	}
	auto preparedResult = image::prepareRasterImage(request.input,
		image::ImageRecipe{mode->width, mode->height, request.fit, request.crop,
			request.background, true}, image::defaultImageLoadLimits());
	if (const auto* error = std::get_if<image::ImageError>(&preparedResult)) {
		return EditorError{EditorErrorCode::preparationFailed, error->operation, error->message};
	}
	image::PreparedImage prepared = std::get<image::PreparedImage>(std::move(preparedResult));
	auto txResult = analog::encodeOfflineTransmission(request.modeId,
		core::ModeCapability::offlineImageTx, prepared.frame.view(),
		analog::OfflineTransmissionOptions{0.8F, request.fskIdentifier});
	if (const auto* error = std::get_if<analog::OfflineTxError>(&txResult)) {
		return EditorError{EditorErrorCode::invalidRequest, "encode preview", error->message};
	}
	analog::OfflineTransmission transmission
	    = std::get<analog::OfflineTransmission>(std::move(txResult));
	if (transmission.events.size() > maximumRetainedToneEvents) {
		return EditorError{EditorErrorCode::resourceLimit, "retain preview",
			"transmission exceeds the retained event limit"};
	}
	const std::uint64_t frames = core::sampleCount(transmission.duration, request.sampleRate);
	constexpr std::uint64_t maximumFrames
	    = (std::numeric_limits<std::uint32_t>::max() - 36U) / 2U;
	if (frames > maximumFrames) {
		return EditorError{EditorErrorCode::resourceLimit, "project WAV",
			"projected WAV exceeds RIFF limits"};
	}
	auto modeInfo = describe(*mode);
	if (const auto* error = std::get_if<EditorError>(&modeInfo)) return *error;
	auto snapshot = std::make_shared<OfflineEditorSnapshot>(OfflineEditorSnapshot{
		request.revision, std::get<OfflineModeInfo>(std::move(modeInfo)), request.input,
		std::move(prepared), std::move(transmission), request.sampleRate, frames,
		44U + frames * 2U, request.fskIdentifier.has_value()
		    ? std::optional<std::string>(request.fskIdentifier->value()) : std::nullopt});
	return std::shared_ptr<const OfflineEditorSnapshot>(std::move(snapshot));
}

PngExportResult
exportPreparedPng(const OfflineEditorSnapshot& snapshot,
	const std::filesystem::path& destination, const bool replace)
{
	if (auto error = validateOutput(snapshot, destination, replace)) return *error;
	auto result = image::writeRgb8PngAtomic(destination, snapshot.prepared.frame.view(), replace);
	if (const auto* error = std::get_if<image::ImageError>(&result)) {
		return EditorError{EditorErrorCode::exportFailed, error->operation, error->message};
	}
	return std::get<image::PngMetadata>(result);
}

WavEditorExportResult
exportOfflineWav(const OfflineEditorSnapshot& snapshot,
	const std::filesystem::path& destination, const bool replace)
{
	if (auto error = validateOutput(snapshot, destination, replace)) return *error;
	try {
		const auto metadata = offline::writePcm16WavAtomic(destination,
			snapshot.transmission.events, snapshot.sampleRate, replace);
		auto inspected = offline::inspectPcm16Wav(destination,
			offline::defaultWavInspectionLimits());
		if (const auto* error = std::get_if<offline::WavInspectError>(&inspected)) {
			return EditorError{EditorErrorCode::inspectionFailed, error->operation, error->message};
		}
		return WavExportResult{destination, metadata.frameCount,
			snapshot.transmission.duration, snapshot.fskIdentifier,
			std::get<offline::WavInspection>(inspected)};
	} catch (const std::exception& error) {
		return EditorError{EditorErrorCode::exportFailed, "export WAV", error.what()};
	}
}

InspectionResult
inspectOfflineWav(const std::filesystem::path& path)
{
	auto result = offline::inspectPcm16Wav(path, offline::defaultWavInspectionLimits());
	if (const auto* error = std::get_if<offline::WavInspectError>(&result)) {
		return EditorError{EditorErrorCode::inspectionFailed, error->operation, error->message};
	}
	return std::get<offline::WavInspection>(result);
}

} // namespace sstv::app

