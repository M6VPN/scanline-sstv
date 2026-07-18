// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/rig/rigctld_codec.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "rigctld_internal.hpp"

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sstv::rig::rigctldInternal {
namespace {

[[nodiscard]] std::optional<std::string> validateBytes(
	const std::string_view response, const RigctldLimits& limits)
{
	if (response.size() > limits.maximumResponseBytes) {
		return "rigctld response exceeds configured byte limit";
	}
	for (const char value : response) {
		const auto byte = static_cast<unsigned char>(value);
		if (byte == '\r') return "rigctld CRLF response is unsupported";
		if (byte == 0 || byte == 0x1b || (byte < 0x20 && byte != '\n') || byte > 0x7e) {
			return "rigctld response contains unsupported control or non-ASCII data";
		}
	}
	return std::nullopt;
}

[[nodiscard]] std::optional<std::string> splitLines(
	const std::string_view response, const RigctldLimits& limits,
	std::vector<std::string_view>& lines)
{
	std::size_t start = 0;
	while (start < response.size()) {
		const std::size_t end = response.find('\n', start);
		const std::size_t length = (end == std::string_view::npos ? response.size() : end) - start;
		if (length > limits.maximumLineBytes) return "rigctld response line exceeds configured limit";
		if (end == std::string_view::npos) break;
		lines.push_back(response.substr(start, length));
		if (lines.size() > limits.maximumResponseLines) {
			return "rigctld response exceeds configured line limit";
		}
		start = end + 1;
	}
	return std::nullopt;
}

[[nodiscard]] std::optional<std::string> parseInteger(
	const std::string_view text, std::int32_t& value)
{
	if (text.empty()) return "missing rigctld integer";
	const auto conversion = std::from_chars(text.data(), text.data() + text.size(), value);
	if (conversion.ec != std::errc{} || conversion.ptr != text.data() + text.size()) {
		return "invalid or overflowing rigctld integer";
	}
	return std::nullopt;
}

[[nodiscard]] std::size_t expectedLineCount(const PttAction action) noexcept
{
	return action == PttAction::query ? 3 : 2;
}

[[nodiscard]] std::string expectedHeader(const PttAction action)
{
	if (action == PttAction::query) return "get_ptt:";
	return action == PttAction::key ? "set_ptt: 1" : "set_ptt: 0";
}

} // namespace

std::string buildCommand(const PttAction action)
{
	if (action == PttAction::query) return "+t\n";
	return action == PttAction::key ? "+T 1\n" : "+T 0\n";
}

FrameStatus inspectResponse(const std::string_view response, const PttAction action,
	const RigctldLimits& limits)
{
	if (const auto error = validateBytes(response, limits)) return {false, *error};
	std::vector<std::string_view> lines;
	lines.reserve(expectedLineCount(action));
	if (const auto error = splitLines(response, limits, lines)) return {false, *error};
	if (!lines.empty() && lines.front() != expectedHeader(action)) {
		return {false, "unexpected rigctld response header"};
	}
	std::size_t expected = expectedLineCount(action);
	if (action == PttAction::query && lines.size() >= 2) {
		if (lines[1].starts_with("RPRT ")) {
			std::int32_t resultCode = 0;
			if (const auto error = parseInteger(lines[1].substr(5), resultCode)) return {false, *error};
			if (resultCode >= 0) return {false, "query success is missing rigctld PTT field"};
			expected = 2;
		} else {
			if (!lines[1].starts_with("PTT: ")) return {false, "missing rigctld PTT field"};
			std::int32_t value = 0;
			if (const auto error = parseInteger(lines[1].substr(5), value)) return {false, *error};
			if (value < 0 || value > 3) return {false, "unsupported rigctld PTT value"};
		}
	}
	if (lines.size() < expected) return {};
	if (lines.size() > expected || response.empty() || response.back() != '\n') {
		return {false, "unexpected trailing rigctld response data"};
	}
	if (!lines.back().starts_with("RPRT ")) return {false, "missing rigctld RPRT terminator"};
	std::int32_t resultCode = 0;
	if (const auto error = parseInteger(lines.back().substr(5), resultCode)) return {false, *error};
	if (resultCode > 0) return {false, "positive rigctld RPRT code is unsupported"};
	return {true, {}};
}

std::optional<std::string> parseResponse(const std::string_view response,
	const PttAction action, const RigctldLimits& limits, Response& parsed)
{
	const FrameStatus frame = inspectResponse(response, action, limits);
	if (!frame.error.empty()) return frame.error;
	if (!frame.isComplete) return "truncated rigctld response";
	std::vector<std::string_view> lines;
	lines.reserve(expectedLineCount(action));
	if (const auto error = splitLines(response, limits, lines)) return error;
	if (const auto error = parseInteger(lines.back().substr(5), parsed.resultCode)) return error;
	if (action != PttAction::query || parsed.resultCode != 0) return std::nullopt;
	std::int32_t ptt = 0;
	if (const auto error = parseInteger(lines[1].substr(5), ptt)) return error;
	parsed.state = ptt == 0 ? PttObservedState::unkeyed : PttObservedState::keyed;
	return std::nullopt;
}

} // namespace sstv::rig::rigctldInternal
