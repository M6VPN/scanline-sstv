// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/rig/loopback_tcp.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/rig/ptt.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace sstv::rig::loopbackTcp {

enum class Error {
	none,
	invalidConfiguration,
	deadlineExpired,
	connectionRefused,
	disconnected,
	ioFailure,
	malformedResponse,
	responseTooLarge,
};

struct Configuration {
	std::string address;
	std::uint16_t port = 0;
	std::chrono::milliseconds connectTimeout{500};
	std::chrono::milliseconds writeTimeout{500};
	std::chrono::milliseconds responseTimeout{1'000};
	std::size_t maximumResponseBytes = 0;
};

struct FrameStatus {
	bool isComplete = false;
	std::string error;
};

class ResponseFramer {
public:
	virtual ~ResponseFramer() = default;
	[[nodiscard]] virtual FrameStatus inspect(std::string_view response) const = 0;
};

struct Result {
	Error error = Error::none;
	std::string response;
	std::size_t requestBytesSent = 0;
	std::string message;
};

[[nodiscard]] bool isLiteralLoopback(std::string_view address) noexcept;
[[nodiscard]] Result exchange(const Configuration& configuration,
	std::string_view request, MonotonicTime deadline,
	const ResponseFramer& framer, const std::shared_ptr<MonotonicClock>& clock) noexcept;

} // namespace sstv::rig::loopbackTcp
