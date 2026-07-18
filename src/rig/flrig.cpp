// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/rig/flrig.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/rig/flrig.hpp>

#include "flrig_internal.hpp"

#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <exception>
#include <limits>
#include <utility>

namespace sstv::rig {
namespace {

class FileDescriptor final {
public:
	explicit FileDescriptor(const int descriptor = -1) noexcept : descriptor_(descriptor) {}
	~FileDescriptor() { if (descriptor_ >= 0) ::close(descriptor_); }
	FileDescriptor(const FileDescriptor&) = delete;
	FileDescriptor& operator=(const FileDescriptor&) = delete;
	[[nodiscard]] int get() const noexcept { return descriptor_; }
private:
	int descriptor_;
};

[[nodiscard]] std::string boundedMessage(std::string value)
{
	if (value.size() > maximumFlrigDiagnosticBytes) value.resize(maximumFlrigDiagnosticBytes);
	return value;
}

[[nodiscard]] MonotonicTime limitedDeadline(const MonotonicTime now,
	const MonotonicTime requested, const std::chrono::milliseconds limit)
{
	const MonotonicTime local = now + std::chrono::duration_cast<MonotonicTime>(limit);
	return std::min(requested, local);
}

[[nodiscard]] int remainingMilliseconds(const MonotonicTime now,
	const MonotonicTime deadline) noexcept
{
	if (now >= deadline) return 0;
	const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
	if (remaining.count() >= INT_MAX) return INT_MAX;
	return static_cast<int>(std::max<std::int64_t>(1, remaining.count()));
}

class PosixFlrigTransport final : public FlrigTransport {
public:
	PosixFlrigTransport(FlrigConfiguration configuration,
		std::shared_ptr<MonotonicClock> clock)
		: configuration_(std::move(configuration)), clock_(std::move(clock)) {}
	[[nodiscard]] FlrigTransportResult exchange(const std::string& request,
		const MonotonicTime deadline) noexcept override
	{
		try {
			return executeExchange(request, deadline);
		} catch (const std::exception&) {
			return {FlrigTransportError::ioFailure, {}, 0, "transport allocation failure"};
		} catch (...) {
			return {FlrigTransportError::ioFailure, {}, 0, "unknown transport failure"};
		}
	}
private:
	[[nodiscard]] FlrigTransportResult executeExchange(const std::string& request,
		const MonotonicTime deadline)
	{
		if (clock_->now() >= deadline) {
			return {FlrigTransportError::deadlineExpired, {}, 0, "PTT deadline already expired"};
		}
		const int family = configuration_.address == "127.0.0.1" ? AF_INET : AF_INET6;
		int socketType = SOCK_STREAM;
#ifdef SOCK_CLOEXEC
		socketType |= SOCK_CLOEXEC;
#endif
		FileDescriptor socket(::socket(family, socketType, 0));
		if (socket.get() < 0) return {FlrigTransportError::ioFailure, {}, 0, "socket creation failed"};
#ifndef SOCK_CLOEXEC
		if (::fcntl(socket.get(), F_SETFD, FD_CLOEXEC) != 0) {
			return {FlrigTransportError::ioFailure, {}, 0, "close-on-exec configuration failed"};
		}
#endif
#ifdef SO_NOSIGPIPE
		const int enabled = 1;
		if (::setsockopt(socket.get(), SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) != 0) {
			return {FlrigTransportError::ioFailure, {}, 0, "SIGPIPE protection failed"};
		}
#endif
		const int flags = ::fcntl(socket.get(), F_GETFL, 0);
		if (flags < 0 || ::fcntl(socket.get(), F_SETFL, flags | O_NONBLOCK) != 0) {
			return {FlrigTransportError::ioFailure, {}, 0, "nonblocking socket configuration failed"};
		}
		int connectionResult = -1;
		if (family == AF_INET) {
			sockaddr_in address{};
			address.sin_family = AF_INET;
			address.sin_port = htons(configuration_.port);
			if (::inet_pton(AF_INET, configuration_.address.c_str(), &address.sin_addr) != 1) {
				return {FlrigTransportError::invalidConfiguration, {}, 0, "invalid IPv4 literal"};
			}
			connectionResult = ::connect(socket.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address));
		} else {
			sockaddr_in6 address{};
			address.sin6_family = AF_INET6;
			address.sin6_port = htons(configuration_.port);
			if (::inet_pton(AF_INET6, configuration_.address.c_str(), &address.sin6_addr) != 1) {
				return {FlrigTransportError::invalidConfiguration, {}, 0, "invalid IPv6 literal"};
			}
			connectionResult = ::connect(socket.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address));
		}
		if (connectionResult != 0 && errno != EINPROGRESS) {
			return {errno == ECONNREFUSED ? FlrigTransportError::connectionRefused
				: FlrigTransportError::ioFailure, {}, 0, "loopback connection failed"};
		}
		const MonotonicTime connectDeadline = limitedDeadline(clock_->now(), deadline,
			configuration_.connectTimeout);
		if (connectionResult != 0) {
			const auto pollResult = wait(socket.get(), POLLOUT, connectDeadline);
			if (pollResult != FlrigTransportError::none) return {pollResult, {}, 0, "connect deadline or failure"};
			int socketError = 0;
			socklen_t errorSize = sizeof(socketError);
			if (::getsockopt(socket.get(), SOL_SOCKET, SO_ERROR, &socketError, &errorSize) != 0
				|| socketError != 0) {
				return {socketError == ECONNREFUSED ? FlrigTransportError::connectionRefused
					: FlrigTransportError::ioFailure, {}, 0, "loopback connection refused"};
			}
		}
		std::size_t sent = 0;
		const MonotonicTime writeDeadline = limitedDeadline(clock_->now(), deadline,
			configuration_.writeTimeout);
		while (sent < request.size()) {
			if (clock_->now() >= writeDeadline) {
				return {FlrigTransportError::deadlineExpired, {}, sent, "write deadline expired"};
			}
#ifdef MSG_NOSIGNAL
			constexpr int sendFlags = MSG_NOSIGNAL;
#else
			constexpr int sendFlags = 0;
#endif
			const ssize_t count = ::send(socket.get(), request.data() + sent, request.size() - sent, sendFlags);
			if (count > 0) {
				sent += static_cast<std::size_t>(count);
				continue;
			}
			if (count == 0) return {FlrigTransportError::disconnected, {}, sent, "peer closed during write"};
			if (errno == EINTR) continue;
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				return {errno == EPIPE || errno == ECONNRESET ? FlrigTransportError::disconnected
					: FlrigTransportError::ioFailure, {}, sent, "socket write failed"};
			}
			const auto pollResult = wait(socket.get(), POLLOUT, writeDeadline);
			if (pollResult != FlrigTransportError::none) return {pollResult, {}, sent, "write wait failed"};
		}
		std::string response;
		response.reserve(std::min<std::size_t>(configuration_.limits.maximumHeaderBytes
			+ configuration_.limits.maximumBodyBytes, 8 * 1024));
		std::size_t expectedBytes = 0;
		bool hasHeader = false;
		const MonotonicTime responseDeadline = limitedDeadline(clock_->now(), deadline,
			configuration_.responseTimeout);
		std::array<char, 4096> buffer{};
		while (true) {
			if (clock_->now() >= responseDeadline) {
				return {FlrigTransportError::deadlineExpired, {}, sent, "response deadline expired"};
			}
			const ssize_t count = ::recv(socket.get(), buffer.data(), buffer.size(), 0);
			if (count > 0) {
				response.append(buffer.data(), static_cast<std::size_t>(count));
				if (const auto error = flrigInternal::findHttpMessageLength(response,
					configuration_.limits, expectedBytes, hasHeader)) {
					return {FlrigTransportError::malformedHttp, {}, sent, boundedMessage(*error)};
				}
				if (hasHeader && response.size() == expectedBytes) return {FlrigTransportError::none, std::move(response), sent, {}};
				if (hasHeader && response.size() > expectedBytes) {
					return {FlrigTransportError::malformedHttp, {}, sent, "unexpected trailing HTTP bytes"};
				}
				continue;
			}
			if (count == 0) return {FlrigTransportError::disconnected, {}, sent, "truncated response"};
			if (errno == EINTR) continue;
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				return {FlrigTransportError::disconnected, {}, sent, "socket read failed"};
			}
			const auto pollResult = wait(socket.get(), POLLIN, responseDeadline);
			if (pollResult != FlrigTransportError::none) return {pollResult, {}, sent, "response wait failed"};
		}
	}
	[[nodiscard]] FlrigTransportError wait(const int descriptor, const short events,
		const MonotonicTime deadline) const noexcept
	{
		while (clock_->now() < deadline) {
			pollfd descriptorState{descriptor, events, 0};
			const int result = ::poll(&descriptorState, 1, remainingMilliseconds(clock_->now(), deadline));
			if (result > 0) {
				if ((descriptorState.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0
					&& (descriptorState.revents & events) == 0) return FlrigTransportError::disconnected;
				if ((descriptorState.revents & events) != 0) return FlrigTransportError::none;
			}
			if (result < 0 && errno != EINTR) return FlrigTransportError::ioFailure;
		}
		return FlrigTransportError::deadlineExpired;
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
			request.action == PttAction::key, request.attempt, {}, {}, "flrig provider allocation failure"};
	} catch (...) {
		return {request.action, PttObservedState::unknown, PttCertainty::indeterminate,
			PttReadback::unavailable, PttErrorCategory::providerFailure, false,
			request.action == PttAction::key, request.attempt, {}, {}, "unknown flrig provider failure"};
	}
}

} // namespace sstv::rig
