// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/rig/flrig_internal.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/rig/flrig.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace sstv::rig::flrigInternal {

enum class XmlResponseKind {
	setAcknowledged,
	state,
	fault,
};

struct XmlResponse {
	XmlResponseKind kind = XmlResponseKind::fault;
	PttObservedState state = PttObservedState::unknown;
	std::string message;
};

struct HttpResponse {
	std::string body;
};

[[nodiscard]] std::string buildRequest(const FlrigConfiguration& configuration,
	PttAction action);
[[nodiscard]] std::optional<std::string> findHttpMessageLength(
	std::string_view response, const FlrigLimits& limits, std::size_t& totalBytes,
	bool& hasCompleteHeader);
[[nodiscard]] std::optional<std::string> parseHttpResponse(
	std::string_view response, const FlrigLimits& limits, HttpResponse& parsed);
[[nodiscard]] std::optional<std::string> parseXmlResponse(
	std::string_view body, PttAction action, const FlrigLimits& limits,
	XmlResponse& parsed);
[[nodiscard]] bool isValidUtf8(std::string_view value) noexcept;

} // namespace sstv::rig::flrigInternal
