// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/audio_commands.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_commands.hpp"
#include "audio_text.hpp"

#include <sstv/audio/audio_discovery.hpp>

#include <iomanip>
#include <iostream>
#include <locale>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace {

struct ListAudioOptions {
	std::optional<sstv::audio::AudioBackend> backend;
	bool includeNull = false;
	bool hasBackend = false;
};

[[nodiscard]] ListAudioOptions
parseOptions(const int argc, char* argv[])
{
	ListAudioOptions options;
	for (int index = 2; index < argc; ++index) {
		const std::string_view argument{argv[index]};
		if (argument == "--include-null") {
			if (options.includeNull) {
				throw std::invalid_argument("duplicate option: --include-null");
			}
			options.includeNull = true;
			continue;
		}
		if (argument != "--backend") {
			throw std::invalid_argument("unexpected argument: " + std::string(argument));
		}
		if (options.hasBackend) {
			throw std::invalid_argument("duplicate option: --backend");
		}
		if (++index >= argc || std::string_view(argv[index]).starts_with("--")) {
			throw std::invalid_argument("missing value for --backend");
		}
		options.backend = sstv::audio::parseAudioBackend(argv[index]);
		if (!options.backend) {
			throw std::invalid_argument("unknown audio backend: "
			    + std::string(argv[index]));
		}
		options.hasBackend = true;
	}
	return options;
}

[[nodiscard]] std::string_view
statusName(const sstv::audio::BackendStatus status)
{
	switch (status) {
	case sstv::audio::BackendStatus::available: return "available";
	case sstv::audio::BackendStatus::unavailable: return "unavailable";
	case sstv::audio::BackendStatus::uncompiled: return "uncompiled";
	case sstv::audio::BackendStatus::safeEnumerationUnsupported:
		return "safe-enumeration-unsupported";
	case sstv::audio::BackendStatus::cancelled: return "cancelled";
	}
	return "unknown";
}

[[nodiscard]] std::string_view
directionName(const sstv::audio::AudioDirection direction)
{
	return direction == sstv::audio::AudioDirection::playback
	    ? "playback" : "capture";
}

[[nodiscard]] std::string_view
transportName(const sstv::audio::AudioTransport transport)
{
	switch (transport) {
	case sstv::audio::AudioTransport::usb: return "usb";
	case sstv::audio::AudioTransport::builtIn: return "built-in";
	case sstv::audio::AudioTransport::virtualDevice: return "virtual";
	case sstv::audio::AudioTransport::unknown: return "unknown";
	}
	return "unknown";
}

[[nodiscard]] std::string_view
sampleFormatName(const sstv::audio::SampleFormat format)
{
	switch (format) {
	case sstv::audio::SampleFormat::unsigned8: return "u8";
	case sstv::audio::SampleFormat::signed16: return "s16";
	case sstv::audio::SampleFormat::signed24: return "s24";
	case sstv::audio::SampleFormat::signed32: return "s32";
	case sstv::audio::SampleFormat::float32: return "f32";
	case sstv::audio::SampleFormat::unknown: return "unknown";
	}
	return "unknown";
}

void
printCapabilities(const sstv::audio::DeviceCapabilities& capabilities)
{
	if (!capabilities.detailsKnown) {
		std::cout << "    Capabilities: unknown\n";
		return;
	}
	std::cout << "    Native formats:";
	if (capabilities.nativeFormats.empty()) {
		std::cout << " none reported";
	}
	for (const auto& format : capabilities.nativeFormats) {
		std::cout << ' ' << sampleFormatName(format.format) << '/';
		if (format.channels) {
			std::cout << *format.channels << "ch";
		} else {
			std::cout << "anych";
		}
		std::cout << '/';
		if (format.sampleRate) {
			std::cout << *format.sampleRate << "Hz";
		} else {
			std::cout << "anyHz";
		}
	}
	std::cout << '\n';
}

void
printSnapshot(const sstv::audio::AudioDiscoverySnapshot& snapshot)
{
	std::cout.imbue(std::locale::classic());
	for (const auto& backend : snapshot.backends) {
		std::cout << "Backend: " << sstv::audio::audioBackendDisplayName(backend.backend)
		    << " (" << backend.apiName << ")\n"
		    << "  Status: " << statusName(backend.status) << '\n';
		if (!backend.diagnostic.empty()) {
			std::cout << "  Diagnostic: " << escapeAudioTerminalText(backend.diagnostic) << '\n';
		}
		for (const auto& device : backend.devices) {
			std::cout << "  Device: " << escapeAudioTerminalText(device.name) << '\n'
			    << "    Direction: " << directionName(device.identity.direction) << '\n'
			    << "    Default: " << (device.isDefault ? "yes" : "no") << '\n'
			    << "    Identity: " << backend.apiName << ':'
			    << directionName(device.identity.direction) << ':'
			    << device.identity.opaque << '\n'
			    << "    Persistence: "
			    << (device.identity.stability == sstv::audio::IdentityStability::persistent
			            ? "persistent" : "session-only") << '\n'
			    << "    Transport: " << transportName(device.transport) << '\n';
			if (device.hasIdentityCollision) {
				std::cout << "    Identity collision: yes\n";
			}
			printCapabilities(device.capabilities);
			if (!device.diagnostic.empty()) {
				std::cout << "    Diagnostic: "
				    << escapeAudioTerminalText(device.diagnostic) << '\n';
			}
		}
	}
}

} // namespace

bool
isAudioCommand(const std::string_view argument) noexcept
{
	return argument == "list-audio";
}

void
printAudioCommandHelp()
{
	std::cout
	    << "  scanline-sstv-cli list-audio [--backend BACKEND] [--include-null]\n"
	       "    BACKEND: pulseaudio, jack, alsa, oss, sndio, or audio4\n"
	       "    Read-only discovery only; no audio device is opened.\n";
}

int
runAudioCommand(const int argc, char* argv[])
{
	try {
		const ListAudioOptions options = parseOptions(argc, argv);
		sstv::audio::DiscoveryRequest request;
		if (options.backend) {
			request.backends.push_back(*options.backend);
		}
		request.includeNull = options.includeNull;
		sstv::audio::AudioDiscoveryService service(
		    sstv::audio::createMiniaudioDiscoveryProvider());
		const sstv::audio::DiscoveryResult result = service.refresh(request);
		if (const auto* error = std::get_if<sstv::audio::DiscoveryError>(&result)) {
			if (error->attemptedSnapshot) {
				printSnapshot(*error->attemptedSnapshot);
			}
			std::cerr << "Error: " << error->message << '\n';
			return 1;
		}
		printSnapshot(**std::get_if<std::shared_ptr<const sstv::audio::AudioDiscoverySnapshot>>(
		    &result));
		return 0;
	} catch (const std::invalid_argument& error) {
		std::cerr << "Error: " << error.what() << '\n';
		return 2;
	} catch (const std::exception& error) {
		std::cerr << "Error: " << error.what() << '\n';
		return 1;
	}
}
