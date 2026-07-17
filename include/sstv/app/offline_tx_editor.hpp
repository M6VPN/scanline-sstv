// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/app/offline_tx_editor.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/analog/offline_tx.hpp>
#include <sstv/image/image.hpp>
#include <sstv/offline/wav_inspector.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace sstv::app {

inline constexpr std::size_t maximumRetainedToneEvents = 700'000U;

enum class EditorErrorCode {
	invalidMode, invalidRequest, preparationFailed, resourceLimit,
	destinationExists, unsafeOutput, exportFailed, inspectionFailed,
};

struct EditorError {
	EditorErrorCode code;
	std::string operation;
	std::string message;
};

struct OfflineModeInfo {
	std::string id;
	std::string displayName;
	std::string colourEncoding;
	std::uint16_t width;
	std::uint16_t height;
	std::optional<std::uint8_t> visCode;
	core::Duration baseDuration;
};

struct OfflineEditorRequest {
	std::uint64_t revision;
	std::string modeId;
	std::filesystem::path input;
	image::FitMode fit;
	std::optional<image::CropRect> crop;
	core::Rgb8Pixel background;
	std::uint32_t sampleRate;
	std::optional<analog::FskIdentifier> fskIdentifier;
};

struct OfflineEditorSnapshot {
	std::uint64_t revision;
	OfflineModeInfo mode;
	std::filesystem::path input;
	image::PreparedImage prepared;
	analog::OfflineTransmission transmission;
	std::uint32_t sampleRate;
	std::uint64_t frameCount;
	std::uint64_t projectedFileBytes;
	std::optional<std::string> fskIdentifier;
};

struct WavExportResult {
	std::filesystem::path path;
	std::uint64_t frameCount;
	core::Duration transmissionDuration;
	std::optional<std::string> fskIdentifier;
	offline::WavInspection inspection;
};

using ModeListResult = std::variant<std::vector<OfflineModeInfo>, EditorError>;
using SnapshotResult = std::variant<std::shared_ptr<const OfflineEditorSnapshot>, EditorError>;
using PngExportResult = std::variant<image::PngMetadata, EditorError>;
using WavEditorExportResult = std::variant<WavExportResult, EditorError>;
using InspectionResult = std::variant<offline::WavInspection, EditorError>;

class GenerationGate {
public:
	[[nodiscard]] std::uint64_t issue() noexcept;
	[[nodiscard]] bool isCurrent(std::uint64_t) const noexcept;
private:
	std::atomic<std::uint64_t> current{0U};
};

[[nodiscard]] ModeListResult offlineImageModes();
[[nodiscard]] SnapshotResult prepareOfflineEditor(const OfflineEditorRequest&);
[[nodiscard]] PngExportResult exportPreparedPng(
	const OfflineEditorSnapshot&, const std::filesystem::path&, bool);
[[nodiscard]] WavEditorExportResult exportOfflineWav(
	const OfflineEditorSnapshot&, const std::filesystem::path&, bool);
[[nodiscard]] InspectionResult inspectOfflineWav(const std::filesystem::path&);

} // namespace sstv::app
