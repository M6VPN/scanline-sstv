// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/rig/rigctld.hpp
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

inline constexpr std::size_t maximumRigctldDiagnosticBytes = 256;

struct RigctldLimits {
	std::size_t maximumLineBytes = 256;
	std::size_t maximumResponseLines = 3;
	std::size_t maximumResponseBytes = 1'024;
};

struct RigctldConfiguration {
	std::string address;
	std::uint16_t port = 0;
	std::chrono::milliseconds connectTimeout{500};
	std::chrono::milliseconds writeTimeout{500};
	std::chrono::milliseconds responseTimeout{1'000};
	RigctldLimits limits{};
};

enum class RigctldTransportError {
	none,
	invalidConfiguration,
	deadlineExpired,
	connectionRefused,
	disconnected,
	ioFailure,
	malformedResponse,
	responseTooLarge,
};

struct RigctldTransportResult {
	RigctldTransportError error = RigctldTransportError::none;
	std::string response;
	std::size_t requestBytesSent = 0;
	std::string message;
};

/** Provider-specific bounded exchange seam used by deterministic tests. */
class RigctldTransport {
public:
	virtual ~RigctldTransport() = default;
	[[nodiscard]] virtual RigctldTransportResult exchange(
		const std::string& request, MonotonicTime deadline) noexcept = 0;
};

struct RigctldConfigurationError {
	std::string message;
};

/** Validates the explicit literal-loopback endpoint and all parser limits. */
[[nodiscard]] std::optional<RigctldConfigurationError> validateRigctldConfiguration(
	const RigctldConfiguration& configuration);

/** Creates the private POSIX implementation of the rigctld exchange seam. */
[[nodiscard]] std::unique_ptr<RigctldTransport> createRigctldPosixTransport(
	const RigctldConfiguration& configuration,
	std::shared_ptr<MonotonicClock> clock);

/** Bounded extended-response rigctld provider with mandatory readback. */
class RigctldPttProvider final : public PttProvider {
public:
	RigctldPttProvider(RigctldConfiguration configuration,
		std::shared_ptr<MonotonicClock> clock,
		std::unique_ptr<RigctldTransport> transport = nullptr);
	~RigctldPttProvider() override;
	RigctldPttProvider(const RigctldPttProvider&) = delete;
	RigctldPttProvider& operator=(const RigctldPttProvider&) = delete;
	[[nodiscard]] PttOperationResult execute(
		const PttRequest& request) noexcept override;

private:
	class Implementation;
	std::unique_ptr<Implementation> implementation_;
};

} // namespace sstv::rig
