// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/rig/rigctld.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/rig/rigctld.hpp>

#include "loopback_tcp.hpp"
#include "rigctld_internal.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace sstv::rig {
namespace {

[[nodiscard]] std::string boundedMessage(std::string value)
{
	if (value.size() > maximumRigctldDiagnosticBytes) {
		value.resize(maximumRigctldDiagnosticBytes);
	}
	return value;
}

class RigctldResponseFramer final : public loopbackTcp::ResponseFramer {
public:
	RigctldResponseFramer(const PttAction action, const RigctldLimits& limits)
		: action_(action), limits_(limits) {}
	[[nodiscard]] loopbackTcp::FrameStatus inspect(
		const std::string_view response) const override
	{
		const rigctldInternal::FrameStatus status =
			rigctldInternal::inspectResponse(response, action_, limits_);
		return {status.isComplete, status.error};
	}
private:
	PttAction action_;
	const RigctldLimits& limits_;
};

class PosixRigctldTransport final : public RigctldTransport {
public:
	PosixRigctldTransport(RigctldConfiguration configuration,
		std::shared_ptr<MonotonicClock> clock)
		: configuration_(std::move(configuration)), clock_(std::move(clock)) {}
	[[nodiscard]] RigctldTransportResult exchange(const std::string& request,
		const MonotonicTime deadline) noexcept override
	{
		PttAction action = PttAction::query;
		if (request == rigctldInternal::buildCommand(PttAction::key)) {
			action = PttAction::key;
		} else if (request == rigctldInternal::buildCommand(PttAction::unkey)) {
			action = PttAction::unkey;
		} else if (request != rigctldInternal::buildCommand(PttAction::query)) {
			return {RigctldTransportError::invalidConfiguration, {}, 0,
				"unsupported rigctld command"};
		}
		const loopbackTcp::Configuration transportConfiguration{
			configuration_.address,
			configuration_.port,
			configuration_.connectTimeout,
			configuration_.writeTimeout,
			configuration_.responseTimeout,
			configuration_.limits.maximumResponseBytes,
		};
		const RigctldResponseFramer framer(action, configuration_.limits);
		const loopbackTcp::Result result = loopbackTcp::exchange(
			transportConfiguration, request, deadline, framer, clock_);
		return {mapError(result.error), result.response, result.requestBytesSent,
			boundedMessage(result.message)};
	}
private:
	[[nodiscard]] static RigctldTransportError mapError(
		const loopbackTcp::Error error) noexcept
	{
		switch (error) {
		case loopbackTcp::Error::none: return RigctldTransportError::none;
		case loopbackTcp::Error::invalidConfiguration: return RigctldTransportError::invalidConfiguration;
		case loopbackTcp::Error::deadlineExpired: return RigctldTransportError::deadlineExpired;
		case loopbackTcp::Error::connectionRefused: return RigctldTransportError::connectionRefused;
		case loopbackTcp::Error::disconnected: return RigctldTransportError::disconnected;
		case loopbackTcp::Error::malformedResponse: return RigctldTransportError::malformedResponse;
		case loopbackTcp::Error::responseTooLarge: return RigctldTransportError::responseTooLarge;
		case loopbackTcp::Error::ioFailure: return RigctldTransportError::ioFailure;
		}
		return RigctldTransportError::ioFailure;
	}
	RigctldConfiguration configuration_;
	std::shared_ptr<MonotonicClock> clock_;
};

[[nodiscard]] PttErrorCategory mapTransportError(const RigctldTransportError error)
{
	switch (error) {
	case RigctldTransportError::deadlineExpired: return PttErrorCategory::timeout;
	case RigctldTransportError::connectionRefused:
	case RigctldTransportError::disconnected: return PttErrorCategory::disconnected;
	case RigctldTransportError::malformedResponse:
	case RigctldTransportError::responseTooLarge: return PttErrorCategory::malformedResponse;
	case RigctldTransportError::invalidConfiguration: return PttErrorCategory::invalidRequest;
	default: return PttErrorCategory::providerFailure;
	}
}

[[nodiscard]] PttErrorCategory mapResultCode(const std::int32_t code) noexcept
{
	if (code == -5) return PttErrorCategory::timeout;
	if (code == -8) return PttErrorCategory::protocolFault;
	return PttErrorCategory::rejected;
}

} // namespace

std::optional<RigctldConfigurationError> validateRigctldConfiguration(
	const RigctldConfiguration& configuration)
{
	if (!loopbackTcp::isLiteralLoopback(configuration.address)) {
		return RigctldConfigurationError{"address must be literal 127.0.0.1 or ::1"};
	}
	if (configuration.port == 0) {
		return RigctldConfigurationError{"port must be explicit and nonzero"};
	}
	constexpr auto maximumTimeout = std::chrono::seconds(60);
	if (configuration.connectTimeout <= std::chrono::milliseconds::zero()
		|| configuration.writeTimeout <= std::chrono::milliseconds::zero()
		|| configuration.responseTimeout <= std::chrono::milliseconds::zero()
		|| configuration.connectTimeout > maximumTimeout
		|| configuration.writeTimeout > maximumTimeout
		|| configuration.responseTimeout > maximumTimeout) {
		return RigctldConfigurationError{"invalid network timeout"};
	}
	const RigctldLimits& limits = configuration.limits;
	if (limits.maximumLineBytes == 0 || limits.maximumLineBytes > 64 * 1024
		|| limits.maximumResponseLines < 2 || limits.maximumResponseLines > 64
		|| limits.maximumResponseBytes == 0 || limits.maximumResponseBytes > 1024 * 1024
		|| limits.maximumLineBytes > limits.maximumResponseBytes) {
		return RigctldConfigurationError{"invalid rigctld resource limits"};
	}
	return std::nullopt;
}

std::unique_ptr<RigctldTransport> createRigctldPosixTransport(
	const RigctldConfiguration& configuration, std::shared_ptr<MonotonicClock> clock)
{
	return std::make_unique<PosixRigctldTransport>(configuration, std::move(clock));
}

class RigctldPttProvider::Implementation final {
public:
	Implementation(RigctldConfiguration configuration,
		std::shared_ptr<MonotonicClock> clock,
		std::unique_ptr<RigctldTransport> transport)
		: configuration_(std::move(configuration)), clock_(std::move(clock)),
		configurationError_(validateRigctldConfiguration(configuration_)),
		transport_(std::move(transport))
	{
		if (!configurationError_.has_value() && !transport_ && clock_) {
			transport_ = createRigctldPosixTransport(configuration_, clock_);
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
			result.message = configurationError_.has_value()
				? configurationError_->message : "missing rigctld dependency";
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
		const std::string command = rigctldInternal::buildCommand(PttAction::query);
		const RigctldTransportResult exchange = transport_->exchange(command, request.deadline);
		if (exchange.error != RigctldTransportError::none) {
			result.error = mapTransportError(exchange.error);
			result.isRecoverable = true;
			result.message = boundedMessage(exchange.message);
			result.completed = clock_->now();
			return result;
		}
		rigctldInternal::Response response;
		if (const auto error = rigctldInternal::parseResponse(exchange.response,
			PttAction::query, configuration_.limits, response)) {
			return malformed(std::move(result), *error);
		}
		result.providerCode = response.resultCode;
		if (response.resultCode != 0) {
			result.error = mapResultCode(response.resultCode);
			result.isRecoverable = true;
			result.message = "rigctld query returned RPRT error";
			result.completed = clock_->now();
			return result;
		}
		result.observed = response.state;
		result.readback = PttReadback::available;
		result.certainty = response.state == PttObservedState::keyed
			? PttCertainty::definitelyKeyed : PttCertainty::definitelyUnkeyed;
		result.completed = clock_->now();
		return result;
	}
	[[nodiscard]] PttOperationResult executeSet(const PttRequest& request,
		PttOperationResult result) noexcept
	{
		const std::string command = rigctldInternal::buildCommand(request.action);
		const RigctldTransportResult setExchange = transport_->exchange(command, request.deadline);
		result.mayHaveKeyed = request.action == PttAction::key
			&& setExchange.requestBytesSent == command.size();
		PttErrorCategory setError = PttErrorCategory::none;
		std::string setMessage;
		std::optional<std::int32_t> setCode;
		if (setExchange.error != RigctldTransportError::none) {
			setError = mapTransportError(setExchange.error);
			setMessage = setExchange.message;
		} else {
			rigctldInternal::Response response;
			if (const auto error = rigctldInternal::parseResponse(setExchange.response,
				request.action, configuration_.limits, response)) {
				setError = PttErrorCategory::malformedResponse;
				setMessage = *error;
			} else {
				setCode = response.resultCode;
				if (response.resultCode != 0) {
					setError = mapResultCode(response.resultCode);
					setMessage = "rigctld set PTT returned RPRT error";
				}
			}
		}
		if (clock_->now() >= request.deadline || (request.action == PttAction::key
			&& setExchange.requestBytesSent == 0 && setError != PttErrorCategory::none)) {
			result.error = setError;
			result.isRecoverable = true;
			result.message = boundedMessage(setMessage);
			result.providerCode = setCode;
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
		result.providerCode = setCode.has_value() ? setCode : query.providerCode;
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
			result.message = "rigctld PTT readback mismatch";
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
	RigctldConfiguration configuration_;
	std::shared_ptr<MonotonicClock> clock_;
	std::optional<RigctldConfigurationError> configurationError_;
	std::unique_ptr<RigctldTransport> transport_;
};

RigctldPttProvider::RigctldPttProvider(RigctldConfiguration configuration,
	std::shared_ptr<MonotonicClock> clock,
	std::unique_ptr<RigctldTransport> transport)
	: implementation_(std::make_unique<Implementation>(std::move(configuration),
		std::move(clock), std::move(transport))) {}

RigctldPttProvider::~RigctldPttProvider() = default;

PttOperationResult RigctldPttProvider::execute(const PttRequest& request) noexcept
{
	try {
		return implementation_->execute(request);
	} catch (const std::exception&) {
		PttOperationResult result{};
		result.action = request.action;
		result.error = PttErrorCategory::providerFailure;
		result.mayHaveKeyed = request.action == PttAction::key;
		result.attempt = request.attempt;
		result.operationId = request.operationId;
		result.message = "rigctld provider allocation failure";
		return result;
	} catch (...) {
		PttOperationResult result{};
		result.action = request.action;
		result.error = PttErrorCategory::providerFailure;
		result.mayHaveKeyed = request.action == PttAction::key;
		result.attempt = request.attempt;
		result.operationId = request.operationId;
		result.message = "unknown rigctld provider failure";
		return result;
	}
}

} // namespace sstv::rig
