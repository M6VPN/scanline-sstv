// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m2h_signal_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "live_tx_signals.hpp"

#include <sys/types.h>
#include <sys/wait.h>

#include <array>
#include <csignal>
#include <stdexcept>
#include <string>

#include <unistd.h>

namespace {

void
require(const bool condition, const std::string& message)
{
	if (!condition) throw std::runtime_error(message);
}

void
testSignal(const int signal)
{
	std::array<int, 2> ready{};
	require(pipe(ready.data()) == 0, "signal test pipe creation failed");
	const pid_t child = fork();
	require(child >= 0, "signal test fork failed");
	if (child == 0) {
		(void)close(ready[0]);
		LiveTransmitSignalScope scope;
		const char marker = 'R';
		if (write(ready[1], &marker, 1) != 1) _exit(3);
		(void)close(ready[1]);
		while (!scope.isCancellationRequested()) (void)usleep(1'000);
		_exit(0);
	}
	(void)close(ready[1]);
	char marker = 0;
	require(read(ready[0], &marker, 1) == 1 && marker == 'R',
		"signal child did not become ready");
	(void)close(ready[0]);
	require(kill(child, signal) == 0, "cannot signal live-transmit child");
	int status = 0;
	require(waitpid(child, &status, 0) == child,
		"cannot reap live-transmit signal child");
	require(WIFEXITED(status) && WEXITSTATUS(status) == 0,
		"live-transmit signal handler did not publish cancellation");
}

} // namespace

int
main()
{
	for (const int signal : {SIGINT, SIGTERM, SIGHUP}) testSignal(signal);
	return 0;
}
