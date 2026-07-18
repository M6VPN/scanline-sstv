// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/rig/loopback_tcp.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "loopback_tcp.hpp"

#include <sys/socket.h>
#include <sys/types.h>

#include <arpa/inet.h>
#include <cerrno>
#include <climits>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <exception>
#include <utility>

namespace sstv::rig::loopbackTcp {
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

[[nodiscard]] MonotonicTime limitDeadline(const MonotonicTime now,
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

[[nodiscard]] Error waitFor(const int descriptor, const short events,
	const MonotonicTime deadline, const MonotonicClock& clock) noexcept
{
	while (clock.now() < deadline) {
		pollfd descriptorState{descriptor, events, 0};
		const int result = ::poll(&descriptorState, 1,
			remainingMilliseconds(clock.now(), deadline));
		if (result > 0) {
			if ((descriptorState.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0
				&& (descriptorState.revents & events) == 0) return Error::disconnected;
			if ((descriptorState.revents & events) != 0) return Error::none;
		}
		if (result < 0 && errno != EINTR) return Error::ioFailure;
	}
	return Error::deadlineExpired;
}

[[nodiscard]] Result execute(const Configuration& configuration,
	const std::string_view request, const MonotonicTime deadline,
	const ResponseFramer& framer, const std::shared_ptr<MonotonicClock>& clock)
{
	if (!clock || !isLiteralLoopback(configuration.address) || configuration.port == 0
		|| configuration.maximumResponseBytes == 0) {
		return {Error::invalidConfiguration, {}, 0, "invalid loopback transport configuration"};
	}
	if (clock->now() >= deadline) {
		return {Error::deadlineExpired, {}, 0, "PTT deadline already expired"};
	}
	const int family = configuration.address == "127.0.0.1" ? AF_INET : AF_INET6;
	int socketType = SOCK_STREAM;
#ifdef SOCK_CLOEXEC
	socketType |= SOCK_CLOEXEC;
#endif
	FileDescriptor socket(::socket(family, socketType, 0));
	if (socket.get() < 0) return {Error::ioFailure, {}, 0, "socket creation failed"};
#ifndef SOCK_CLOEXEC
	if (::fcntl(socket.get(), F_SETFD, FD_CLOEXEC) != 0) {
		return {Error::ioFailure, {}, 0, "close-on-exec configuration failed"};
	}
#endif
#ifdef SO_NOSIGPIPE
	const int enabled = 1;
	if (::setsockopt(socket.get(), SOL_SOCKET, SO_NOSIGPIPE, &enabled, sizeof(enabled)) != 0) {
		return {Error::ioFailure, {}, 0, "SIGPIPE protection failed"};
	}
#endif
	const int flags = ::fcntl(socket.get(), F_GETFL, 0);
	if (flags < 0 || ::fcntl(socket.get(), F_SETFL, flags | O_NONBLOCK) != 0) {
		return {Error::ioFailure, {}, 0, "nonblocking socket configuration failed"};
	}
	int connectionResult = -1;
	if (family == AF_INET) {
		sockaddr_in address{};
		address.sin_family = AF_INET;
		address.sin_port = htons(configuration.port);
		if (::inet_pton(AF_INET, configuration.address.c_str(), &address.sin_addr) != 1) {
			return {Error::invalidConfiguration, {}, 0, "invalid IPv4 literal"};
		}
		connectionResult = ::connect(socket.get(),
			reinterpret_cast<const sockaddr*>(&address), sizeof(address));
	} else {
		sockaddr_in6 address{};
		address.sin6_family = AF_INET6;
		address.sin6_port = htons(configuration.port);
		if (::inet_pton(AF_INET6, configuration.address.c_str(), &address.sin6_addr) != 1) {
			return {Error::invalidConfiguration, {}, 0, "invalid IPv6 literal"};
		}
		connectionResult = ::connect(socket.get(),
			reinterpret_cast<const sockaddr*>(&address), sizeof(address));
	}
	if (connectionResult != 0 && errno != EINPROGRESS) {
		return {errno == ECONNREFUSED ? Error::connectionRefused : Error::ioFailure,
			{}, 0, "loopback connection failed"};
	}
	const MonotonicTime connectDeadline = limitDeadline(clock->now(), deadline,
		configuration.connectTimeout);
	if (connectionResult != 0) {
		const Error waitResult = waitFor(socket.get(), POLLOUT, connectDeadline, *clock);
		if (waitResult != Error::none) return {waitResult, {}, 0, "connect deadline or failure"};
		int socketError = 0;
		socklen_t errorSize = sizeof(socketError);
		if (::getsockopt(socket.get(), SOL_SOCKET, SO_ERROR, &socketError, &errorSize) != 0
			|| socketError != 0) {
			return {socketError == ECONNREFUSED ? Error::connectionRefused : Error::ioFailure,
				{}, 0, "loopback connection refused"};
		}
	}
	std::size_t sent = 0;
	const MonotonicTime writeDeadline = limitDeadline(clock->now(), deadline,
		configuration.writeTimeout);
	while (sent < request.size()) {
		if (clock->now() >= writeDeadline) {
			return {Error::deadlineExpired, {}, sent, "write deadline expired"};
		}
#ifdef MSG_NOSIGNAL
		constexpr int sendFlags = MSG_NOSIGNAL;
#else
		constexpr int sendFlags = 0;
#endif
		const ssize_t count = ::send(socket.get(), request.data() + sent,
			request.size() - sent, sendFlags);
		if (count > 0) {
			sent += static_cast<std::size_t>(count);
			continue;
		}
		if (count == 0) return {Error::disconnected, {}, sent, "peer closed during write"};
		if (errno == EINTR) continue;
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			return {errno == EPIPE || errno == ECONNRESET ? Error::disconnected : Error::ioFailure,
				{}, sent, "socket write failed"};
		}
		const Error waitResult = waitFor(socket.get(), POLLOUT, writeDeadline, *clock);
		if (waitResult != Error::none) return {waitResult, {}, sent, "write wait failed"};
	}
	std::string response;
	response.reserve(std::min<std::size_t>(configuration.maximumResponseBytes, 8 * 1024));
	const MonotonicTime responseDeadline = limitDeadline(clock->now(), deadline,
		configuration.responseTimeout);
	std::array<char, 4096> buffer{};
	while (true) {
		if (clock->now() >= responseDeadline) {
			return {Error::deadlineExpired, {}, sent, "response deadline expired"};
		}
		const ssize_t count = ::recv(socket.get(), buffer.data(), buffer.size(), 0);
		if (count > 0) {
			const std::size_t received = static_cast<std::size_t>(count);
			if (received > configuration.maximumResponseBytes - response.size()) {
				return {Error::responseTooLarge, {}, sent, "response exceeds configured limit"};
			}
			response.append(buffer.data(), received);
			const FrameStatus status = framer.inspect(response);
			if (!status.error.empty()) {
				return {Error::malformedResponse, {}, sent, status.error};
			}
			if (status.isComplete) return {Error::none, std::move(response), sent, {}};
			continue;
		}
		if (count == 0) return {Error::disconnected, {}, sent, "truncated response"};
		if (errno == EINTR) continue;
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			return {Error::disconnected, {}, sent, "socket read failed"};
		}
		const Error waitResult = waitFor(socket.get(), POLLIN, responseDeadline, *clock);
		if (waitResult != Error::none) return {waitResult, {}, sent, "response wait failed"};
	}
}

} // namespace

bool isLiteralLoopback(const std::string_view address) noexcept
{
	return address == "127.0.0.1" || address == "::1";
}

Result exchange(const Configuration& configuration, const std::string_view request,
	const MonotonicTime deadline, const ResponseFramer& framer,
	const std::shared_ptr<MonotonicClock>& clock) noexcept
{
	try {
		return execute(configuration, request, deadline, framer, clock);
	} catch (const std::exception&) {
		return {Error::ioFailure, {}, 0, "transport allocation failure"};
	} catch (...) {
		return {Error::ioFailure, {}, 0, "unknown transport failure"};
	}
}

} // namespace sstv::rig::loopbackTcp
