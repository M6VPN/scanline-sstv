// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/include/sstv/app/live_transmit_signals.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <csignal>

namespace sstv::app {

/** Signal-safe cancellation publisher for an explicitly armed live session. */
class LiveTransmitSignalScope final {
public:
	LiveTransmitSignalScope();
	~LiveTransmitSignalScope();
	LiveTransmitSignalScope(const LiveTransmitSignalScope&) = delete;
	LiveTransmitSignalScope& operator=(const LiveTransmitSignalScope&) = delete;
	[[nodiscard]] bool isCancellationRequested() const noexcept;

private:
	struct sigaction oldInterrupt_{};
	struct sigaction oldTerminate_{};
	struct sigaction oldHangup_{};
	bool hasInterrupt_ = false;
	bool hasTerminate_ = false;
	bool hasHangup_ = false;
};

} // namespace sstv::app
