// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/app/live_transmit_signals.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/app/live_transmit_signals.hpp>

#include <stdexcept>

namespace {

volatile std::sig_atomic_t signalCancellation = 0;

extern "C" void
handleLiveTransmitSignal(const int) noexcept
{
	signalCancellation = 1;
}

} // namespace

namespace sstv::app {

LiveTransmitSignalScope::LiveTransmitSignalScope()
{
	signalCancellation = 0;
	struct sigaction action{};
	action.sa_handler = handleLiveTransmitSignal;
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;
	if (sigaction(SIGINT, &action, &oldInterrupt_) != 0) {
		throw std::runtime_error("cannot install SIGINT live-transmit handler");
	}
	hasInterrupt_ = true;
	if (sigaction(SIGTERM, &action, &oldTerminate_) != 0) {
		(void)sigaction(SIGINT, &oldInterrupt_, nullptr);
		hasInterrupt_ = false;
		throw std::runtime_error("cannot install SIGTERM live-transmit handler");
	}
	hasTerminate_ = true;
	if (sigaction(SIGHUP, &action, &oldHangup_) != 0) {
		(void)sigaction(SIGTERM, &oldTerminate_, nullptr);
		(void)sigaction(SIGINT, &oldInterrupt_, nullptr);
		hasTerminate_ = false;
		hasInterrupt_ = false;
		throw std::runtime_error("cannot install SIGHUP live-transmit handler");
	}
	hasHangup_ = true;
}

LiveTransmitSignalScope::~LiveTransmitSignalScope()
{
	if (hasHangup_) (void)sigaction(SIGHUP, &oldHangup_, nullptr);
	if (hasTerminate_) (void)sigaction(SIGTERM, &oldTerminate_, nullptr);
	if (hasInterrupt_) (void)sigaction(SIGINT, &oldInterrupt_, nullptr);
}

bool
LiveTransmitSignalScope::isCancellationRequested() const noexcept
{
	return signalCancellation != 0;
}

} // namespace sstv::app
