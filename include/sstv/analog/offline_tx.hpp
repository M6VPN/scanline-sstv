// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/analog/offline_tx.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/analog/fsk_id.hpp>
#include <sstv/core/mode.hpp>
#include <sstv/core/rgb8_frame.hpp>
#include <sstv/core/timing.hpp>
#include <sstv/core/tone.hpp>

#include <optional>
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

struct OfflineTransmissionOptions {
	float amplitude = 0.8F;
	std::optional<FskIdentifier> fskIdentifier;
};

using OfflineTxResult = std::variant<OfflineTransmission, OfflineTxError>;

/** Resolve and encode one evidence-approved built-in offline transmission. */
[[nodiscard]] OfflineTxResult encodeOfflineTransmission(
	std::string_view, core::ModeCapability, core::Rgb8View,
	const OfflineTransmissionOptions&);

/** Preserve the M1E no-FSK API and exact base-mode event stream. */
[[nodiscard]] OfflineTxResult encodeOfflineTransmission(
	std::string_view, core::ModeCapability, core::Rgb8View, float);

} // namespace sstv::analog
