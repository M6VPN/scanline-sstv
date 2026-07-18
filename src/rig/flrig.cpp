// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/rig/flrig.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/rig/flrig.hpp>

#include "flrig_internal.hpp"
#include "loopback_tcp.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace sstv::rig {
namespace {

[[nodiscard]] std::string boundedMessage(std::string value)
{
	if (value.size() > maximumFlrigDiagnosticBytes) value.resize(maximumFlrigDiagnosticBytes);
	return value;
}

class HttpResponseFramer final : public loopbackTcp::ResponseFramer {
public:
	explicit HttpResponseFramer(const FlrigLimits& limits) : limits_(limits) {}
	[[nodiscard]] loopbackTcp::FrameStatus inspect(
		const std::string_view response) const override
	{
		std::size_t expectedBytes = 0;
		bool hasHeader = false;
		if (const auto error = flrigInternal::findHttpMessageLength(
			response, limits_, expectedBytes, hasHeader)) return {false, *error};
		if (hasHeader && response.size() > expectedBytes) {
			return {false, "unexpected trailing HTTP bytes"};
		}
		return {hasHeader && response.size() == expectedBytes, {}};
	}
private:
	const FlrigLimits& limits_;
};

class PosixFlrigTransport final : public FlrigTransport {
public:
	PosixFlrigTransport(FlrigConfiguration configuration,
		std::shared_ptr<MonotonicClock> clock)
		: configuration_(std::move(configuration)), clock_(std::move(clock)) {}
	[[nodiscard]] FlrigTransportResult exchange(const std::string& request,
		const MonotonicTime deadline) noexcept override
	{
		const loopbackTcp::Configuration transportConfiguration{
			configuration_.address,
			configuration_.port,
			configuration_.connectTimeout,
			configuration_.writeTimeout,
			configuration_.responseTimeout,
			configuration_.limits.maximumHeaderBytes + configuration_.limits.maximumBodyBytes,
		};
		const HttpResponseFramer framer(configuration_.limits);
		const loopbackTcp::Result result = loopbackTcp::exchange(
			transportConfiguration, request, deadline, framer, clock_);
		return {mapError(result.error), result.response, result.requestBytesSent,
			boundedMessage(result.message)};
	}
private:
	[[nodiscard]] static FlrigTransportError mapError(
		const loopbackTcp::Error error) noexcept
	{
		switch (error) {
		case loopbackTcp::Error::none: return FlrigTransportError::none;
		case loopbackTcp::Error::invalidConfiguration: return FlrigTransportError::invalidConfiguration;
		case loopbackTcp::Error::deadlineExpired: return FlrigTransportError::deadlineExpired;
		case loopbackTcp::Error::connectionRefused: return FlrigTransportError::connectionRefused;
		case loopbackTcp::Error::disconnected: return FlrigTransportError::disconnected;
		case loopbackTcp::Error::malformedResponse: return FlrigTransportError::malformedHttp;
		case loopbackTcp::Error::responseTooLarge: return FlrigTransportError::responseTooLarge;
		case loopbackTcp::Error::ioFailure: return FlrigTransportError::ioFailure;
		}
		return FlrigTransportError::ioFailure;
	}
	FlrigConfiguration configuration_;
	std::shared_ptr<MonotonicClock> clock_;
};

[[nodiscard]] PttErrorCategory mapTransportError(const FlrigTransportError error)
{
	switch (error) {
	case FlrigTransportError::deadlineExpired: return PttErrorCategory::timeout;
	case FlrigTransportError::connectionRefused:
	case FlrigTransportError::disconnected: return PttErrorCategory::disconnected;
	case FlrigTransportError::malformedHttp: return PttErrorCategory::malformedResponse;
	case FlrigTransportError::invalidConfiguration: return PttErrorCategory::invalidRequest;
	default: return PttErrorCategory::providerFailure;
	}
}

} // namespace

std::optional<FlrigConfigurationError> validateFlrigConfiguration(
	const FlrigConfiguration& configuration)
{
	if (configuration.address != "127.0.0.1" && configuration.address != "::1") {
		return FlrigConfigurationError{"address must be literal 127.0.0.1 or ::1"};
	}
	if (configuration.port == 0) return FlrigConfigurationError{"port must be explicit and nonzero"};
	if (configuration.path.empty() || configuration.path.front() != '/'
		|| configuration.path.size() > maximumFlrigPathBytes) return FlrigConfigurationError{"invalid XML-RPC path"};
	for (const char pathCharacter : configuration.path) {
		const auto character = static_cast<unsigned char>(pathCharacter);
		if (character < 0x21 || character > 0x7e || character == '?' || character == '#'
			|| character == '@' || character == '\\') return FlrigConfigurationError{"unsafe XML-RPC path"};
	}
	if (configuration.path.find("://") != std::string::npos) return FlrigConfigurationError{"XML-RPC path must not be a URL"};
	constexpr auto maximumTimeout = std::chrono::seconds(60);
	if (configuration.connectTimeout <= std::chrono::milliseconds::zero()
		|| configuration.writeTimeout <= std::chrono::milliseconds::zero()
		|| configuration.responseTimeout <= std::chrono::milliseconds::zero()
		|| configuration.connectTimeout > maximumTimeout || configuration.writeTimeout > maximumTimeout
		|| configuration.responseTimeout > maximumTimeout) return FlrigConfigurationError{"invalid network timeout"};
	const FlrigLimits& limits = configuration.limits;
	if (limits.maximumHeaderBytes == 0 || limits.maximumHeaderBytes > 1024 * 1024
		|| limits.maximumBodyBytes == 0 || limits.maximumBodyBytes > 1024 * 1024
		|| limits.maximumXmlNesting == 0 || limits.maximumXmlNesting > 64
		|| limits.maximumXmlElements == 0 || limits.maximumXmlElements > 4096
		|| limits.maximumXmlTextBytes == 0 || limits.maximumXmlTextBytes > limits.maximumBodyBytes) {
		return FlrigConfigurationError{"invalid flrig resource limits"};
	}
	return std::nullopt;
}

std::unique_ptr<FlrigTransport> createFlrigPosixTransport(
	const FlrigConfiguration& configuration, std::shared_ptr<MonotonicClock> clock)
{
	return std::make_unique<PosixFlrigTransport>(configuration, std::move(clock));
}

class FlrigPttProvider::Implementation final {
public:
	Implementation(FlrigConfiguration configuration, std::shared_ptr<MonotonicClock> clock,
		std::unique_ptr<FlrigTransport> transport)
		: configuration_(std::move(configuration)), clock_(std::move(clock)),
		configurationError_(validateFlrigConfiguration(configuration_)), transport_(std::move(transport))
	{
		if (!configurationError_.has_value() && !transport_ && clock_) {
			transport_ = createFlrigPosixTransport(configuration_, clock_);
		}
	}
	[[nodiscard]] PttOperationResult execute(const PttRequest& request) noexcept
	{
		PttOperationResult result{};
		result.action = request.action;
		result.attempt = request.attempt;
		result.operationId = request.operationId;
		result.started = clock_ ? clock_->now() : MonotonicTime{};
		if (configurationError_.has_value() || !clock_ || !transport_) {
			result.error = PttErrorCategory::invalidRequest;
			result.message = configurationError_.has_value() ? configurationError_->message : "missing flrig dependency";
			result.completed = result.started;
			return result;
		}
		if (result.started >= request.deadline) {
			result.error = PttErrorCategory::timeout;
			result.message = "PTT deadline already expired";
			result.completed = result.started;
			return result;
		}
		if (request.action == PttAction::query) return executeQuery(request, result);
		return executeSet(request, result);
	}
private:
	[[nodiscard]] PttOperationResult executeQuery(const PttRequest& request,
		PttOperationResult result) noexcept
	{
		const FlrigTransportResult exchange = transport_->exchange(
			flrigInternal::buildRequest(configuration_, PttAction::query), request.deadline);
		if (exchange.error != FlrigTransportError::none) {
			result.error = mapTransportError(exchange.error);
			result.isRecoverable = true;
			result.message = boundedMessage(exchange.message);
			result.completed = clock_->now();
			return result;
		}
		flrigInternal::HttpResponse http;
		if (const auto error = flrigInternal::parseHttpResponse(exchange.response,
			configuration_.limits, http)) return malformed(std::move(result), *error);
		flrigInternal::XmlResponse xml;
		if (const auto error = flrigInternal::parseXmlResponse(http.body, PttAction::query,
			configuration_.limits, xml)) return malformed(std::move(result), *error);
		if (xml.kind == flrigInternal::XmlResponseKind::fault) {
			result.error = PttErrorCategory::protocolFault;
			result.message = xml.message;
			result.completed = clock_->now();
			return result;
		}
		result.observed = xml.state;
		result.readback = PttReadback::available;
		result.certainty = xml.state == PttObservedState::keyed
			? PttCertainty::definitelyKeyed : PttCertainty::definitelyUnkeyed;
		result.completed = clock_->now();
		return result;
	}
	[[nodiscard]] PttOperationResult executeSet(const PttRequest& request,
		PttOperationResult result) noexcept
	{
		const FlrigTransportResult setExchange = transport_->exchange(
			flrigInternal::buildRequest(configuration_, request.action), request.deadline);
		result.mayHaveKeyed = request.action == PttAction::key && setExchange.requestBytesSent > 0;
		PttErrorCategory setError = PttErrorCategory::none;
		std::string setMessage;
		if (setExchange.error != FlrigTransportError::none) {
			setError = mapTransportError(setExchange.error);
			setMessage = setExchange.message;
		} else {
			flrigInternal::HttpResponse http;
			if (const auto error = flrigInternal::parseHttpResponse(setExchange.response,
				configuration_.limits, http)) {
				setError = PttErrorCategory::malformedResponse;
				setMessage = *error;
			} else {
				flrigInternal::XmlResponse xml;
				if (const auto xmlError = flrigInternal::parseXmlResponse(http.body, request.action,
					configuration_.limits, xml)) {
					setError = PttErrorCategory::malformedResponse;
					setMessage = *xmlError;
				} else if (xml.kind == flrigInternal::XmlResponseKind::fault) {
					setError = PttErrorCategory::protocolFault;
					setMessage = xml.message;
				}
			}
		}
		if (clock_->now() >= request.deadline || (setExchange.requestBytesSent == 0
			&& setError != PttErrorCategory::none)) {
			result.error = setError;
			result.isRecoverable = true;
			result.message = boundedMessage(setMessage);
			result.completed = clock_->now();
			return result;
		}
		PttRequest queryRequest = request;
		queryRequest.action = PttAction::query;
		PttOperationResult query = executeQuery(queryRequest, {});
		result.observed = query.observed;
		result.certainty = query.certainty;
		result.readback = query.readback;
		result.completed = query.completed;
		result.attempt = request.attempt;
		if (query.error != PttErrorCategory::none) {
			result.error = setError != PttErrorCategory::none ? setError : query.error;
			result.isRecoverable = true;
			result.message = boundedMessage(setMessage.empty() ? query.message : setMessage);
			return result;
		}
		const PttObservedState expected = request.action == PttAction::key
			? PttObservedState::keyed : PttObservedState::unkeyed;
		if (query.observed != expected) {
			result.error = PttErrorCategory::rejected;
			result.message = "flrig PTT readback mismatch";
			return result;
		}
		if (setError != PttErrorCategory::none) {
			result.error = setError;
			result.message = boundedMessage(setMessage);
		}
		return result;
	}
	[[nodiscard]] PttOperationResult malformed(PttOperationResult result,
		const std::string& message) const noexcept
	{
		result.error = PttErrorCategory::malformedResponse;
		result.message = boundedMessage(message);
		result.completed = clock_->now();
		return result;
	}
	FlrigConfiguration configuration_;
	std::shared_ptr<MonotonicClock> clock_;
	std::optional<FlrigConfigurationError> configurationError_;
	std::unique_ptr<FlrigTransport> transport_;
};

FlrigPttProvider::FlrigPttProvider(FlrigConfiguration configuration,
	std::shared_ptr<MonotonicClock> clock, std::unique_ptr<FlrigTransport> transport)
	: implementation_(std::make_unique<Implementation>(std::move(configuration),
		std::move(clock), std::move(transport))) {}

FlrigPttProvider::~FlrigPttProvider() = default;

PttOperationResult FlrigPttProvider::execute(const PttRequest& request) noexcept
{
	try {
		return implementation_->execute(request);
	} catch (const std::exception&) {
		return {request.action, PttObservedState::unknown, PttCertainty::indeterminate,
			PttReadback::unavailable, PttErrorCategory::providerFailure, false,
			request.action == PttAction::key, request.attempt, {}, {},
			"flrig provider allocation failure", {}, request.operationId};
	} catch (...) {
		return {request.action, PttObservedState::unknown, PttCertainty::indeterminate,
			PttReadback::unavailable, PttErrorCategory::providerFailure, false,
			request.action == PttAction::key, request.attempt, {}, {},
			"unknown flrig provider failure", {}, request.operationId};
	}
}

} // namespace sstv::rig
