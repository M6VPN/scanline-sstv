// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/rig/flrig_codec.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "flrig_internal.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace sstv::rig::flrigInternal {
namespace {

[[nodiscard]] std::string lowerAscii(std::string_view value)
{
	std::string result(value);
	std::ranges::transform(result, result.begin(), [](const unsigned char character) {
		return static_cast<char>(std::tolower(character));
	});
	return result;
}

[[nodiscard]] std::string_view trimAscii(std::string_view value)
{
	while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.remove_prefix(1);
	while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) value.remove_suffix(1);
	return value;
}

[[nodiscard]] std::optional<std::string> parseContentLength(
	std::string_view header, const FlrigLimits& limits, std::size_t& contentLength)
{
	std::optional<std::size_t> accepted;
	std::size_t position = header.find("\r\n") + 2;
	bool hasContentType = false;
	while (position < header.size()) {
		const std::size_t end = header.find("\r\n", position);
		if (end == std::string_view::npos) return "malformed HTTP header termination";
		if (end == position) break;
		const std::string_view line = header.substr(position, end - position);
		if (line.front() == ' ' || line.front() == '\t') return "obsolete folded HTTP header";
		const std::size_t colon = line.find(':');
		if (colon == std::string_view::npos || colon == 0) return "malformed HTTP header line";
		const std::string name = lowerAscii(line.substr(0, colon));
		const std::string_view value = trimAscii(line.substr(colon + 1));
		if (name == "content-length") {
			std::size_t parsed = 0;
			const auto conversion = std::from_chars(value.data(), value.data() + value.size(), parsed);
			if (conversion.ec != std::errc{} || conversion.ptr != value.data() + value.size()) {
				return "invalid Content-Length";
			}
			if (parsed > limits.maximumBodyBytes) return "HTTP body exceeds configured limit";
			if (accepted.has_value() && accepted.value() != parsed) return "conflicting Content-Length";
			accepted = parsed;
		} else if (name == "content-type") {
			hasContentType = lowerAscii(value) == "text/xml";
			if (!hasContentType) return "unsupported HTTP Content-Type";
		} else if (name == "transfer-encoding" || name == "content-encoding"
			|| name == "upgrade" || name == "location") {
			return "unsupported HTTP response feature";
		}
		position = end + 2;
	}
	if (!accepted.has_value()) return "missing Content-Length";
	if (!hasContentType) return "missing text/xml Content-Type";
	contentLength = accepted.value();
	return std::nullopt;
}

[[nodiscard]] std::optional<std::string> validateXmlEnvelope(
	std::string_view body, const FlrigLimits& limits, std::string& compact)
{
	if (!isValidUtf8(body)) return "XML response is not valid UTF-8";
	for (const char bodyCharacter : body) {
		const auto character = static_cast<unsigned char>(bodyCharacter);
		if (character == 0 || (character < 0x20 && character != '\r'
			&& character != '\n' && character != '\t')) return "XML response contains control data";
	}
	const std::string lowered = lowerAscii(body);
	if (lowered.find("<!doctype") != std::string::npos
		|| lowered.find("<!entity") != std::string::npos
		|| lowered.find("xi:include") != std::string::npos
		|| lowered.find("<![") != std::string::npos) return "unsafe XML construct";
	std::string_view remaining = body;
	while (!remaining.empty() && std::isspace(static_cast<unsigned char>(remaining.front()))) {
		remaining.remove_prefix(1);
	}
	if (remaining.starts_with("<?xml version=\"1.0\"?>")) {
		remaining.remove_prefix(21);
	} else if (remaining.starts_with("<?")) {
		return "unsupported XML processing instruction";
	}
	if (remaining.find("<?") != std::string_view::npos) return "unexpected XML processing instruction";
	std::size_t elements = 0;
	std::size_t depth = 0;
	bool insideTag = false;
	compact.clear();
	compact.reserve(remaining.size());
	for (std::size_t index = 0; index < remaining.size(); ++index) {
		const char character = remaining[index];
		if (character == '<') {
			insideTag = true;
			++elements;
			if (elements > limits.maximumXmlElements * 2) return "XML element limit exceeded";
			if (index + 1 < remaining.size() && remaining[index + 1] != '/') {
				++depth;
				if (depth > limits.maximumXmlNesting) return "XML nesting limit exceeded";
			} else if (depth > 0) {
				--depth;
			}
		}
		if (!(std::isspace(static_cast<unsigned char>(character)) && !insideTag)) compact.push_back(character);
		if (character == '>') insideTag = false;
	}
	if (compact.size() > limits.maximumXmlTextBytes + limits.maximumBodyBytes) return "XML text limit exceeded";
	return std::nullopt;
}

[[nodiscard]] bool hasValidXmlEntities(const std::string_view text)
{
	std::size_t position = 0;
	while ((position = text.find('&', position)) != std::string_view::npos) {
		const std::size_t end = text.find(';', position + 1);
		if (end == std::string_view::npos) return false;
		const std::string_view entity = text.substr(position, end - position + 1);
		if (entity != "&amp;" && entity != "&lt;" && entity != "&gt;"
			&& entity != "&apos;" && entity != "&quot;") return false;
		position = end + 1;
	}
	return true;
}

[[nodiscard]] bool isValidFault(const std::string& compact)
{
	const std::string prefix = "<methodResponse><fault><value><struct><member>"
		"<name>faultCode</name><value><i4>";
	const std::string middle = "</i4></value></member><member><name>faultString</name><value>";
	const std::string suffix = "</value></member></struct></value></fault></methodResponse>";
	if (!compact.starts_with(prefix) || !compact.ends_with(suffix)) return false;
	const std::size_t middlePosition = compact.find(middle, prefix.size());
	if (middlePosition == std::string::npos) return false;
	const std::string_view code(compact.data() + prefix.size(), middlePosition - prefix.size());
	std::int32_t parsedCode = 0;
	const auto conversion = std::from_chars(code.data(), code.data() + code.size(), parsedCode);
	if (conversion.ec != std::errc{} || conversion.ptr != code.data() + code.size()) return false;
	const std::size_t messageStart = middlePosition + middle.size();
	const std::size_t messageSize = compact.size() - messageStart - suffix.size();
	const std::string_view message(compact.data() + messageStart, messageSize);
	return message.find('<') == std::string_view::npos && hasValidXmlEntities(message);
}

} // namespace

std::string buildRequest(const FlrigConfiguration& configuration, const PttAction action)
{
	const bool isQuery = action == PttAction::query;
	const std::string method = isQuery ? "rig.get_ptt" : "rig.set_ptt";
	std::string body = "<?xml version=\"1.0\"?>\r\n<methodCall><methodName>" + method
		+ "</methodName>\r\n";
	if (!isQuery) {
		const char value = action == PttAction::key ? '1' : '0';
		body += "<params><param><value><i4>";
		body.push_back(value);
		body += "</i4></value></param></params>";
	}
	body += "</methodCall>\r\n";
	const std::string host = configuration.address == "::1"
		? "[::1]:" + std::to_string(configuration.port)
		: configuration.address + ":" + std::to_string(configuration.port);
	return "POST " + configuration.path + " HTTP/1.1\r\nHost: " + host
		+ "\r\nUser-Agent: scanline-sstv/0.1\r\nContent-Type: text/xml\r\nContent-Length: "
		+ std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
}

std::optional<std::string> findHttpMessageLength(std::string_view response,
	const FlrigLimits& limits, std::size_t& totalBytes, bool& hasCompleteHeader)
{
	hasCompleteHeader = false;
	totalBytes = 0;
	if (response.size() > limits.maximumHeaderBytes + limits.maximumBodyBytes) {
		return "HTTP response exceeds configured limits";
	}
	const std::size_t boundary = response.find("\r\n\r\n");
	if (boundary == std::string_view::npos) {
		if (response.size() > limits.maximumHeaderBytes) return "HTTP header exceeds configured limit";
		return std::nullopt;
	}
	hasCompleteHeader = true;
	const std::size_t headerBytes = boundary + 4;
	if (headerBytes > limits.maximumHeaderBytes) return "HTTP header exceeds configured limit";
	std::size_t bodyBytes = 0;
	if (const auto error = parseContentLength(response.substr(0, headerBytes), limits, bodyBytes)) {
		return error;
	}
	if (bodyBytes > std::numeric_limits<std::size_t>::max() - headerBytes) return "HTTP size overflow";
	totalBytes = headerBytes + bodyBytes;
	return std::nullopt;
}

std::optional<std::string> parseHttpResponse(std::string_view response,
	const FlrigLimits& limits, HttpResponse& parsed)
{
	std::size_t totalBytes = 0;
	bool hasHeader = false;
	if (const auto error = findHttpMessageLength(response, limits, totalBytes, hasHeader)) return error;
	if (!hasHeader || response.size() < totalBytes) return "truncated HTTP response";
	if (response.size() != totalBytes) return "unexpected trailing HTTP bytes";
	const std::size_t lineEnd = response.find("\r\n");
	if (lineEnd == std::string_view::npos) return "missing HTTP status line";
	for (const char headerCharacter : response.substr(0, response.find("\r\n\r\n") + 4)) {
		const auto character = static_cast<unsigned char>(headerCharacter);
		if (character == 0 || (character < 0x20 && character != '\r' && character != '\n'
			&& character != '\t')) return "HTTP header contains control data";
	}
	if (response.substr(0, lineEnd) != "HTTP/1.1 200 OK") return "non-success HTTP status";
	const std::size_t bodyStart = response.find("\r\n\r\n") + 4;
	parsed.body.assign(response.substr(bodyStart));
	return std::nullopt;
}

std::optional<std::string> parseXmlResponse(std::string_view body, const PttAction action,
	const FlrigLimits& limits, XmlResponse& parsed)
{
	std::string compact;
	if (const auto error = validateXmlEnvelope(body, limits, compact)) return error;
	const std::string prefix = "<methodResponse><params><param><value>";
	const std::string suffix = "</value></param></params></methodResponse>";
	if (compact.starts_with("<methodResponse><fault>")) {
		if (!isValidFault(compact)) return "malformed XML-RPC fault";
		parsed = {XmlResponseKind::fault, PttObservedState::unknown, "flrig XML-RPC fault"};
		return std::nullopt;
	}
	if (!compact.starts_with(prefix) || !compact.ends_with(suffix)) {
		return "unexpected XML-RPC response shape";
	}
	const std::string scalar = compact.substr(prefix.size(),
		compact.size() - prefix.size() - suffix.size());
	if (action == PttAction::query) {
		if (scalar == "<i4>0</i4>") {
			parsed = {XmlResponseKind::state, PttObservedState::unkeyed, {}};
			return std::nullopt;
		}
		if (scalar == "<i4>1</i4>") {
			parsed = {XmlResponseKind::state, PttObservedState::keyed, {}};
			return std::nullopt;
		}
		return "invalid flrig PTT readback value";
	}
	if (!scalar.empty() && scalar != "<string></string>") return "unexpected set-PTT result type";
	parsed = {XmlResponseKind::setAcknowledged, PttObservedState::unknown, {}};
	return std::nullopt;
}

bool isValidUtf8(const std::string_view value) noexcept
{
	std::size_t index = 0;
	while (index < value.size()) {
		const unsigned char first = static_cast<unsigned char>(value[index]);
		if (first < 0x80) {
			++index;
			continue;
		}
		std::size_t count = 0;
		std::uint32_t codepoint = 0;
		if ((first & 0xe0U) == 0xc0U) { count = 2; codepoint = first & 0x1fU; }
		else if ((first & 0xf0U) == 0xe0U) { count = 3; codepoint = first & 0x0fU; }
		else if ((first & 0xf8U) == 0xf0U) { count = 4; codepoint = first & 0x07U; }
		else return false;
		if (index + count > value.size()) return false;
		for (std::size_t offset = 1; offset < count; ++offset) {
			const unsigned char continuation = static_cast<unsigned char>(value[index + offset]);
			if ((continuation & 0xc0U) != 0x80U) return false;
			codepoint = (codepoint << 6U) | (continuation & 0x3fU);
		}
		if ((count == 2 && codepoint < 0x80U) || (count == 3 && codepoint < 0x800U)
			|| (count == 4 && codepoint < 0x10000U) || codepoint > 0x10ffffU
			|| (codepoint >= 0xd800U && codepoint <= 0xdfffU)) return false;
		index += count;
	}
	return true;
}

} // namespace sstv::rig::flrigInternal
