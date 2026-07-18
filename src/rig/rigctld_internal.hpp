// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/rig/rigctld_internal.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/rig/rigctld.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace sstv::rig::rigctldInternal {

struct Response {
	PttObservedState state = PttObservedState::unknown;
	std::int32_t resultCode = 0;
};

struct FrameStatus {
	bool isComplete = false;
	std::string error;
};

[[nodiscard]] std::string buildCommand(PttAction action);
[[nodiscard]] FrameStatus inspectResponse(std::string_view response,
	PttAction action, const RigctldLimits& limits);
[[nodiscard]] std::optional<std::string> parseResponse(std::string_view response,
	PttAction action, const RigctldLimits& limits, Response& parsed);

} // namespace sstv::rig::rigctldInternal
