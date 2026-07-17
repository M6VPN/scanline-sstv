// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/offline_tx.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/offline_tx.hpp>

#include <sstv/analog/martin_m1.hpp>
#include <sstv/analog/pd_120.hpp>
#include <sstv/analog/robot_36.hpp>
#include <sstv/analog/scottie_s1.hpp>

#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

namespace sstv::analog {
namespace {

[[nodiscard]] OfflineTxResult
encodeBaseTransmission(const std::string_view modeId,
	const core::ModeCapability requiredCapability, const core::Rgb8View frame,
	const float amplitude)
{
	const core::ModeDescriptor* const mode = core::find_mode(modeId);
	if (mode == nullptr) {
		return OfflineTxError{
			OfflineTxErrorCode::unknownMode,
			"unknown mode: " + std::string(modeId),
		};
	}
	if (!mode->capabilities.contains(requiredCapability)) {
		return OfflineTxError{
			OfflineTxErrorCode::missingCapability,
			"mode does not provide the requested offline TX capability: "
			    + std::string(modeId),
		};
	}
	if (frame.width() != mode->width || frame.height() != mode->height) {
		return OfflineTxError{
			OfflineTxErrorCode::invalidFrameDimensions,
			std::string(mode->display_name) + " requires a "
			    + std::to_string(mode->width) + " by "
			    + std::to_string(mode->height) + " RGB8 frame",
		};
	}
	switch (mode->offline_tx_strategy) {
	case core::OfflineTxStrategy::martinM1:
		return OfflineTransmission{
			encodeMartinM1(frame, amplitude),
			martinM1TransmissionDuration(),
		};
	case core::OfflineTxStrategy::scottieS1:
		return OfflineTransmission{
			encodeScottieS1(frame, amplitude),
			scottieS1TransmissionDuration(),
		};
	case core::OfflineTxStrategy::robot36:
		return OfflineTransmission{
			encodeRobot36(frame, amplitude),
			robot36TransmissionDuration(),
		};
	case core::OfflineTxStrategy::pd120:
		return OfflineTransmission{
			encodePd120(frame, amplitude),
			pd120TransmissionDuration(),
		};
	case core::OfflineTxStrategy::none:
		return OfflineTxError{
			OfflineTxErrorCode::unsupportedStrategy,
			"mode has no offline TX encoder: " + std::string(modeId),
		};
	}
	return OfflineTxError{
		OfflineTxErrorCode::unsupportedStrategy,
		"mode has an invalid offline TX encoder strategy: " + std::string(modeId),
	};
}

void
appendFskSuffix(OfflineTransmission& transmission, FskIdSuffix suffix)
{
	if (suffix.events.size() > transmission.events.max_size()
	    - transmission.events.size()) {
		throw std::overflow_error("combined transmission event count overflow");
	}
	transmission.events.reserve(transmission.events.size() + suffix.events.size());
	transmission.events.insert(transmission.events.end(),
		std::make_move_iterator(suffix.events.begin()),
		std::make_move_iterator(suffix.events.end()));
	transmission.duration = transmission.duration + suffix.duration;
}

} // namespace

OfflineTxResult
encodeOfflineTransmission(const std::string_view modeId,
	const core::ModeCapability requiredCapability, const core::Rgb8View frame,
	const OfflineTransmissionOptions& options)
{
	OfflineTxResult result = encodeBaseTransmission(
		modeId, requiredCapability, frame, options.amplitude);
	if (std::holds_alternative<OfflineTxError>(result)
	    || !options.fskIdentifier.has_value()) {
		return result;
	}
	OfflineTransmission transmission
		= std::get<OfflineTransmission>(std::move(result));
	appendFskSuffix(transmission,
		generateFskIdSuffix(*options.fskIdentifier, options.amplitude));
	return transmission;
}

OfflineTxResult
encodeOfflineTransmission(const std::string_view modeId,
	const core::ModeCapability requiredCapability, const core::Rgb8View frame,
	const float amplitude)
{
	return encodeOfflineTransmission(modeId, requiredCapability, frame,
		OfflineTransmissionOptions{amplitude, std::nullopt});
}

} // namespace sstv::analog
