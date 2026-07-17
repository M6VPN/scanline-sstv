// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/core/fsk_id.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/analog/fsk_id.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace sstv::analog {
namespace {

using core::Duration;

/* Frozen by docs/protocols/analogue/fsk-id.md and its independent vector. */
constexpr double leaderFrequencyHz = 1'500.0;
constexpr double markFrequencyHz = 1'900.0;
constexpr double spaceFrequencyHz = 2'100.0;
constexpr std::uint8_t headerCode = 0x2aU;
constexpr std::uint8_t endCode = 0x01U;

[[nodiscard]] std::size_t
checkedAdd(const std::size_t left, const std::size_t right)
{
	if (left > std::numeric_limits<std::size_t>::max() - right) {
		throw std::overflow_error("FSK ID event count overflow");
	}
	return left + right;
}

[[nodiscard]] std::size_t
checkedMultiply(const std::size_t left, const std::size_t right)
{
	if (left != 0U && right > std::numeric_limits<std::size_t>::max() / left) {
		throw std::overflow_error("FSK ID event count overflow");
	}
	return left * right;
}

void
appendCode(std::vector<core::ToneEvent>& events, const std::uint8_t code,
	const float amplitude)
{
	const Duration bitDuration = Duration::fromMicroseconds(22'000);
	for (unsigned int bit = 0; bit < 6U; ++bit) {
		const bool isMark = (code & (1U << bit)) != 0U;
		events.emplace_back(bitDuration,
			isMark ? markFrequencyHz : spaceFrequencyHz, amplitude);
	}
}

[[nodiscard]] std::uint8_t
encodedCharacter(const char character)
{
	return static_cast<std::uint8_t>(static_cast<unsigned char>(character) - 0x20U);
}

} // namespace

FskIdentifier::FskIdentifier(std::string value)
	: normalizedValue(std::move(value))
{
}

std::string_view
FskIdentifier::value() const noexcept
{
	return normalizedValue;
}

FskIdentifierResult
validateFskIdentifier(const std::string_view value)
{
	if (value.empty()) {
		return FskIdError{FskIdErrorCode::empty,
			"FSK identifier must not be empty"};
	}
	if (value.size() > maximumFskIdentifierLength) {
		return FskIdError{FskIdErrorCode::tooLong,
			"FSK identifier must contain at most nine characters"};
	}
	std::string normalized;
	normalized.reserve(value.size());
	bool hasNonSpace = false;
	for (std::size_t index = 0; index < value.size(); ++index) {
		unsigned char character = static_cast<unsigned char>(value[index]);
		if (character >= static_cast<unsigned char>('a')
		    && character <= static_cast<unsigned char>('z')) {
			character = static_cast<unsigned char>(character - 'a' + 'A');
		}
		if (character < 0x20U || character > 0x5fU) {
			return FskIdError{FskIdErrorCode::unsupportedCharacter,
				"FSK identifier contains an unsupported character at position "
				    + std::to_string(index)};
		}
		hasNonSpace = hasNonSpace || character != static_cast<unsigned char>(' ');
		normalized.push_back(static_cast<char>(character));
	}
	if (!hasNonSpace) {
		return FskIdError{FskIdErrorCode::empty,
			"FSK identifier must contain a non-space character"};
	}
	return FskIdentifier(std::move(normalized));
}

FskIdSuffix
generateFskIdSuffix(const FskIdentifier& identifier, const float amplitude)
{
	const std::size_t characterEvents = checkedMultiply(identifier.value().size(), 6U);
	const std::size_t eventCount = checkedAdd(characterEvents, 22U);
	std::vector<core::ToneEvent> events;
	events.reserve(eventCount);
	events.emplace_back(Duration::fromMicroseconds(300'000), leaderFrequencyHz,
		amplitude);
	events.emplace_back(Duration::fromMicroseconds(100'000), spaceFrequencyHz,
		amplitude);
	events.emplace_back(Duration::fromMicroseconds(22'000), markFrequencyHz,
		amplitude);
	appendCode(events, headerCode, amplitude);
	std::uint8_t checksum = 0U;
	for (const char character : identifier.value()) {
		const std::uint8_t code = encodedCharacter(character);
		appendCode(events, code, amplitude);
		checksum = static_cast<std::uint8_t>(checksum ^ code);
	}
	appendCode(events, endCode, amplitude);
	appendCode(events, static_cast<std::uint8_t>(checksum & 0x3fU), amplitude);
	events.emplace_back(Duration::fromMicroseconds(100'000), markFrequencyHz,
		amplitude);
	if (events.size() != eventCount) {
		throw std::logic_error("FSK ID event count does not match the schedule");
	}
	Duration duration(0, 1);
	for (const core::ToneEvent& event : events) {
		duration = duration + event.duration();
	}
	return {std::move(events), duration};
}

} // namespace sstv::analog
