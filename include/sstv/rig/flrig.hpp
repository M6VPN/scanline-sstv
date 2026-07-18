// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/rig/flrig.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/rig/ptt.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace sstv::rig {

inline constexpr std::size_t maximumFlrigPathBytes = 256;
inline constexpr std::size_t maximumFlrigDiagnosticBytes = 256;

struct FlrigLimits {
	std::size_t maximumHeaderBytes = 16 * 1024;
	std::size_t maximumBodyBytes = 64 * 1024;
	std::size_t maximumXmlNesting = 16;
	std::size_t maximumXmlElements = 128;
	std::size_t maximumXmlTextBytes = 4 * 1024;
};

struct FlrigConfiguration {
	std::string address;
	std::uint16_t port = 0;
	std::string path;
	std::chrono::milliseconds connectTimeout{500};
	std::chrono::milliseconds writeTimeout{500};
	std::chrono::milliseconds responseTimeout{1'000};
	FlrigLimits limits{};
};

enum class FlrigTransportError {
	none,
	invalidConfiguration,
	deadlineExpired,
	connectionRefused,
	disconnected,
	ioFailure,
	malformedHttp,
	responseTooLarge,
};

struct FlrigTransportResult {
	FlrigTransportError error = FlrigTransportError::none;
	std::string response;
	std::size_t requestBytesSent = 0;
	std::string message;
};

/** One bounded request/response exchange. Implementations retain no connection. */
class FlrigTransport {
public:
	virtual ~FlrigTransport() = default;
	[[nodiscard]] virtual FlrigTransportResult exchange(
		const std::string& request, MonotonicTime deadline) noexcept = 0;
};

struct FlrigConfigurationError {
	std::string message;
};

/** Validates the literal-loopback-only M2E endpoint and all resource limits. */
[[nodiscard]] std::optional<FlrigConfigurationError> validateFlrigConfiguration(
	const FlrigConfiguration& configuration);

/** Creates a POSIX loopback transport. Configuration must already be valid. */
[[nodiscard]] std::unique_ptr<FlrigTransport> createFlrigPosixTransport(
	const FlrigConfiguration& configuration,
	std::shared_ptr<MonotonicClock> clock);

/** Bounded flrig XML-RPC provider. Set operations require separate readback. */
class FlrigPttProvider final : public PttProvider {
public:
	FlrigPttProvider(FlrigConfiguration configuration,
		std::shared_ptr<MonotonicClock> clock,
		std::unique_ptr<FlrigTransport> transport = nullptr);
	~FlrigPttProvider() override;
	FlrigPttProvider(const FlrigPttProvider&) = delete;
	FlrigPttProvider& operator=(const FlrigPttProvider&) = delete;
	[[nodiscard]] PttOperationResult execute(
		const PttRequest& request) noexcept override;

private:
	class Implementation;
	std::unique_ptr<Implementation> implementation_;
};

} // namespace sstv::rig
