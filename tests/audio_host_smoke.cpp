// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/audio_host_smoke.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/audio/audio_discovery.hpp>

#include <algorithm>
#include <memory>
#include <variant>
#include <vector>

int
main()
{
	sstv::audio::AudioDiscoveryService service(
	    sstv::audio::createMiniaudioDiscoveryProvider());
	std::vector<sstv::audio::AudioBackend> backends = sstv::audio::defaultAudioBackends();
#if defined(__linux__)
	backends.erase(std::remove(backends.begin(), backends.end(),
	    sstv::audio::AudioBackend::jack), backends.end());
#endif
	const sstv::audio::DiscoveryResult result = service.refresh({backends, false});
	if (const auto* snapshot
	    = std::get_if<std::shared_ptr<const sstv::audio::AudioDiscoverySnapshot>>(&result)) {
		return (*snapshot)->backends.empty() ? 1 : 0;
	}
	const auto& error = std::get<sstv::audio::DiscoveryError>(result);
	return error.attemptedSnapshot && !error.attemptedSnapshot->backends.empty() ? 0 : 1;
}
