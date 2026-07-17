// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/analog/fsk_id.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/core/timing.hpp>
#include <sstv/core/tone.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace sstv::analog {

inline constexpr std::size_t maximumFskIdentifierLength = 9U;

enum class FskIdErrorCode {
	empty,
	tooLong,
	unsupportedCharacter,
};

struct FskIdError {
	FskIdErrorCode code;
	std::string message;
};

/** Validated, normalized identifier for the analogue SSTV FSK ID suffix. */
class FskIdentifier {
public:
	[[nodiscard]] std::string_view value() const noexcept;

private:
	explicit FskIdentifier(std::string);

	std::string normalizedValue;

	friend std::variant<FskIdentifier, FskIdError> validateFskIdentifier(
		std::string_view);
};

using FskIdentifierResult = std::variant<FskIdentifier, FskIdError>;

struct FskIdSuffix {
	std::vector<core::ToneEvent> events;
	core::Duration duration;
};

/** Validate and uppercase one evidence-compatible analogue FSK identifier. */
[[nodiscard]] FskIdentifierResult validateFskIdentifier(std::string_view);

/** Generate the complete evidence-backed analogue FSK ID suffix. */
[[nodiscard]] FskIdSuffix generateFskIdSuffix(const FskIdentifier&, float);

} // namespace sstv::analog
