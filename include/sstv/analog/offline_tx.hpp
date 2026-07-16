// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/analog/offline_tx.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/core/mode.hpp>
#include <sstv/core/rgb8_frame.hpp>
#include <sstv/core/timing.hpp>
#include <sstv/core/tone.hpp>

#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sstv::analog {

enum class OfflineTxErrorCode {
	unknownMode,
	missingCapability,
	invalidFrameDimensions,
	unsupportedStrategy,
};

struct OfflineTxError {
	OfflineTxErrorCode code;
	std::string message;
};

struct OfflineTransmission {
	std::vector<core::ToneEvent> events;
	core::Duration duration;
};

using OfflineTxResult = std::variant<OfflineTransmission, OfflineTxError>;

/** Resolve and encode one evidence-approved built-in offline transmission. */
[[nodiscard]] OfflineTxResult encodeOfflineTransmission(
	std::string_view, core::ModeCapability, core::Rgb8View, float);

} // namespace sstv::analog
