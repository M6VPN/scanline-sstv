// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/audio_diagnostic_commands.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_diagnostic_commands.hpp"
#include "audio_text.hpp"

#include <sstv/audio/audio_diagnostics.hpp>

#include <charconv>
#include <chrono>
#include <cmath>
#include <csignal>
#include <future>
#include <iomanip>
#include <iostream>
#include <locale>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <variant>

namespace {

volatile std::sig_atomic_t isInterrupted = 0;

void
handleInterrupt(const int) noexcept
{
	isInterrupted = 1;
}

struct Options {
	sstv::audio::DiagnosticRequest request;
	std::string playbackId;
	std::string captureId;
	bool hasBackend = false;
	bool hasPlaybackId = false;
	bool hasCaptureId = false;
	bool hasOutputChannel = false;
	bool hasInputChannel = false;
	bool hasDuration = false;
	bool hasLevel = false;
	bool hasPeriodFrames = false;
	bool hasPeriodCount = false;
	bool hasPlaybackChannels = false;
	bool hasCaptureChannels = false;
	bool hasArm = false;
};

template<typename Integer>
[[nodiscard]] Integer
parseInteger(const std::string_view value, const Integer minimum,
	const Integer maximum, const std::string& option)
{
	Integer parsed = 0;
	const auto [end, error] = std::from_chars(
	    value.data(), value.data() + value.size(), parsed);
	if (value.empty() || error != std::errc{}
	    || end != value.data() + value.size()
	    || parsed < minimum || parsed > maximum) {
		throw std::invalid_argument("invalid value for " + option + ": "
		    + std::string(value));
	}
	return parsed;
}

[[nodiscard]] double
parseLevel(const std::string_view value)
{
	double parsed = 0.0;
	const auto [end, error] = std::from_chars(
	    value.data(), value.data() + value.size(), parsed);
	if (value.empty() || error != std::errc{}
	    || end != value.data() + value.size()
	    || !std::isfinite(parsed)
	    || parsed < sstv::audio::minimumDiagnosticLevelDbfs
	    || parsed > sstv::audio::maximumDiagnosticLevelDbfs) {
		throw std::invalid_argument("invalid value for --level-dbfs: "
		    + std::string(value));
	}
	return parsed;
}

[[nodiscard]] Options
parseOptions(const int argc, char* argv[])
{
	Options options;
	const std::string_view command{argv[1]};
	options.request.operation = command == "audio-meter"
	    ? sstv::audio::DiagnosticOperation::inputMeter
	    : command == "audio-output-test"
	        ? sstv::audio::DiagnosticOperation::outputCalibration
	        : sstv::audio::DiagnosticOperation::loopback;
	for (int index = 2; index < argc; ++index) {
		const std::string_view argument{argv[index]};
		if (argument == "--arm-real-audio") {
			if (options.hasArm) throw std::invalid_argument("duplicate option: --arm-real-audio");
			options.hasArm = true;
			options.request.isRealAudioArmed = true;
			continue;
		}
		if (argument != "--backend" && argument != "--playback-id"
		    && argument != "--capture-id" && argument != "--channel"
		    && argument != "--output-channel" && argument != "--input-channel"
		    && argument != "--duration" && argument != "--level-dbfs"
		    && argument != "--period-frames" && argument != "--period-count"
		    && argument != "--playback-channels" && argument != "--capture-channels") {
			throw std::invalid_argument("unexpected argument: " + std::string(argument));
		}
		if (++index >= argc || std::string_view(argv[index]).starts_with("--")) {
			throw std::invalid_argument("missing value for " + std::string(argument));
		}
		const std::string_view value{argv[index]};
		if (argument == "--backend") {
			if (options.hasBackend) throw std::invalid_argument("duplicate option: --backend");
			const auto backend = sstv::audio::parseAudioBackend(value);
			if (!backend) throw std::invalid_argument("unknown audio backend: " + std::string(value));
			options.request.backend = *backend;
			options.hasBackend = true;
		} else if (argument == "--playback-id") {
			if (options.hasPlaybackId) throw std::invalid_argument("duplicate option: --playback-id");
			options.playbackId = value;
			options.hasPlaybackId = true;
		} else if (argument == "--capture-id") {
			if (options.hasCaptureId) throw std::invalid_argument("duplicate option: --capture-id");
			options.captureId = value;
			options.hasCaptureId = true;
		} else if (argument == "--channel" || argument == "--output-channel") {
			if (options.hasOutputChannel || (argument == "--channel"
			    && options.request.operation == sstv::audio::DiagnosticOperation::inputMeter
			    && options.hasInputChannel)) {
				throw std::invalid_argument("duplicate channel option");
			}
			const std::uint32_t channel = parseInteger<std::uint32_t>(value, 0, 63, std::string(argument));
			if (argument == "--channel"
			    && options.request.operation == sstv::audio::DiagnosticOperation::inputMeter) {
				options.request.captureChannel = channel;
				options.hasInputChannel = true;
			} else {
				options.request.playbackChannel = channel;
				options.hasOutputChannel = true;
			}
		} else if (argument == "--input-channel") {
			if (options.hasInputChannel) throw std::invalid_argument("duplicate option: --input-channel");
			options.request.captureChannel = parseInteger<std::uint32_t>(value, 0, 63, "--input-channel");
			options.hasInputChannel = true;
		} else if (argument == "--duration") {
			if (options.hasDuration) throw std::invalid_argument("duplicate option: --duration");
			const std::uint32_t seconds = parseInteger<std::uint32_t>(value, 1, 10, "--duration");
			options.request.durationMs = seconds * 1'000U;
			options.hasDuration = true;
		} else if (argument == "--level-dbfs") {
			if (options.hasLevel) throw std::invalid_argument("duplicate option: --level-dbfs");
			options.request.levelDbfs = parseLevel(value);
			options.hasLevel = true;
		} else if (argument == "--period-frames") {
			if (options.hasPeriodFrames) throw std::invalid_argument("duplicate option: --period-frames");
			options.request.periodFrames = parseInteger<std::uint32_t>(value, 16, 8'192, "--period-frames");
			options.hasPeriodFrames = true;
		} else if (argument == "--period-count") {
			if (options.hasPeriodCount) throw std::invalid_argument("duplicate option: --period-count");
			options.request.periodCount = parseInteger<std::uint32_t>(value, 2, 16, "--period-count");
			options.hasPeriodCount = true;
		} else if (argument == "--playback-channels") {
			if (options.hasPlaybackChannels) throw std::invalid_argument("duplicate option: --playback-channels");
			options.request.playbackChannels = parseInteger<std::uint32_t>(value, 1, 64, "--playback-channels");
			options.hasPlaybackChannels = true;
		} else {
			if (options.hasCaptureChannels) throw std::invalid_argument("duplicate option: --capture-channels");
			options.request.captureChannels = parseInteger<std::uint32_t>(value, 1, 64, "--capture-channels");
			options.hasCaptureChannels = true;
		}
	}
	if (!options.hasBackend) throw std::invalid_argument("missing required option: --backend");
	const bool playback = options.request.operation != sstv::audio::DiagnosticOperation::inputMeter;
	const bool capture = options.request.operation != sstv::audio::DiagnosticOperation::outputCalibration;
	if (playback && !options.hasPlaybackId) throw std::invalid_argument("missing required option: --playback-id");
	if (capture && !options.hasCaptureId) throw std::invalid_argument("missing required option: --capture-id");
	if ((playback && options.playbackId.empty())
	    || (capture && options.captureId.empty())) {
		throw std::invalid_argument("audio device identity must not be empty");
	}
	if (playback && !options.hasOutputChannel) throw std::invalid_argument("missing required output channel");
	if (capture && !options.hasInputChannel) throw std::invalid_argument("missing required input channel");
	if (playback && !options.hasArm) {
		throw std::invalid_argument("--arm-real-audio is required before any output diagnostic");
	}
	if (!playback && options.hasArm) throw std::invalid_argument("--arm-real-audio is not valid for audio-meter");
	if (!playback && (options.hasPlaybackId || options.hasLevel
	    || options.hasOutputChannel || options.hasPlaybackChannels)) {
		throw std::invalid_argument("playback options are not valid for audio-meter");
	}
	if (!capture && (options.hasCaptureId || options.hasInputChannel
	    || options.hasCaptureChannels)) {
		throw std::invalid_argument("capture options are not valid for audio-output-test");
	}
	if (options.request.playbackChannel >= options.request.playbackChannels
	    || options.request.captureChannel >= options.request.captureChannels) {
		throw std::invalid_argument("selected channel is outside the requested channel count");
	}
	if (playback) {
		options.request.playbackIdentity = sstv::audio::DeviceIdentity{
		    options.request.backend, sstv::audio::AudioDirection::playback,
		    options.playbackId, sstv::audio::IdentityStability::sessionOnly};
	}
	if (capture) {
		options.request.captureIdentity = sstv::audio::DeviceIdentity{
		    options.request.backend, sstv::audio::AudioDirection::capture,
		    options.captureId, sstv::audio::IdentityStability::sessionOnly};
	}
	return options;
}

void
printResult(const sstv::audio::DiagnosticSnapshot& result)
{
	std::cout.imbue(std::locale::classic());
	std::cout << "State: completed\n"
	    << "Discovery generation: " << result.discoveryGeneration << '\n';
	if (result.playbackIdentity) {
		std::cout << "Playback identity: "
		    << escapeAudioTerminalText(result.playbackIdentity->opaque) << '\n'
		    << "Selected playback channel: " << *result.playbackChannel << '\n';
	}
	if (result.captureIdentity) {
		std::cout << "Capture identity: "
		    << escapeAudioTerminalText(result.captureIdentity->opaque) << '\n'
		    << "Selected capture channel: " << *result.captureChannel << '\n';
	}
	if (result.negotiated) {
		std::cout << "Negotiated sample rate: "
		    << result.negotiated->callbackSampleRate << " Hz\n"
		    << "Negotiated period: " << result.negotiated->periodFrames
		    << " frames x " << result.negotiated->periodCount << '\n';
		if (result.negotiated->playback) {
			std::cout << "Negotiated playback channels: "
			    << result.negotiated->playback->callbackChannels << '\n';
		}
		if (result.negotiated->capture) {
			std::cout << "Negotiated capture channels: "
			    << result.negotiated->capture->callbackChannels << '\n';
		}
	}
	if (result.level) {
		std::cout << std::fixed << std::setprecision(6)
		    << "Peak: " << result.level->peak << " (" << result.level->peakDbfs << " dBFS)\n"
		    << "RMS: " << result.level->rms << " (" << result.level->rmsDbfs << " dBFS)\n"
		    << "DC mean: " << result.level->dcMean << '\n'
		    << "Clipped positive/negative: " << result.level->clippedPositive
		    << '/' << result.level->clippedNegative << '\n'
		    << "Captured frames: " << result.level->frames << '\n';
	}
	if (result.loopback) {
		std::cout << std::fixed << std::setprecision(6)
		    << "Correlation: " << result.loopback->correlation << '\n'
		    << "Round-trip latency: " << result.loopback->latencyFrames
		    << " frames (" << result.loopback->latencyMilliseconds << " ms)\n"
		    << "Gain: " << result.loopback->gainDb << " dB\n"
		    << "Polarity inverted: "
		    << (result.loopback->hasPolarityInversion ? "yes" : "no") << '\n';
	}
	std::cout << "Playback underrun frames: " << result.stream.underrunFrames << '\n'
	    << "Capture dropped frames: " << result.stream.captureFramesDropped << '\n';
}

} // namespace

bool
isAudioDiagnosticCommand(const std::string_view command) noexcept
{
	return command == "audio-meter" || command == "audio-output-test"
	    || command == "audio-loopback";
}

void
printAudioDiagnosticCommandHelp()
{
	std::cout
	    << "  scanline-sstv-cli audio-meter --backend BACKEND --capture-id ID\n"
	       "      --channel N --duration SECONDS [--capture-channels N]\n"
	       "      [--period-frames N] [--period-count N]\n"
	       "  scanline-sstv-cli audio-output-test --backend BACKEND --playback-id ID\n"
	       "      --channel N [--level-dbfs -30] [--duration 2]\n"
	       "      [--playback-channels N] --arm-real-audio\n"
	       "  scanline-sstv-cli audio-loopback --backend BACKEND --playback-id ID\n"
	       "      --capture-id ID --output-channel N --input-channel N\n"
	       "      [--level-dbfs -30] --arm-real-audio\n"
	       "    Calibration only. No SSTV audio, radio control, VOX, or PTT.\n";
}

int
runAudioDiagnosticCommand(const int argc, char* argv[])
{
	try {
		const Options options = parseOptions(argc, argv);
		auto provider = sstv::audio::createMiniaudioDiscoveryProvider();
		sstv::audio::AudioDiagnosticsService service(provider,
		    [] { return sstv::audio::createMiniaudioStreamAdapter(); });
		isInterrupted = 0;
		const auto previous = std::signal(SIGINT, handleInterrupt);
		std::promise<sstv::audio::DiagnosticResult> promise;
		std::future<sstv::audio::DiagnosticResult> future = promise.get_future();
		std::jthread worker([&service, &options, &promise](const std::stop_token token) {
			promise.set_value(service.run(options.request, token));
		});
		while (future.wait_for(std::chrono::milliseconds(20)) != std::future_status::ready) {
			if (isInterrupted != 0) service.requestStop();
		}
		worker.join();
		(void)std::signal(SIGINT, previous);
		const sstv::audio::DiagnosticResult result = future.get();
		if (const auto* error = std::get_if<sstv::audio::DiagnosticError>(&result)) {
			std::cerr << "Error: " << error->operation << ": " << error->message << '\n';
			return error->code == sstv::audio::DiagnosticErrorCode::inconclusive ? 3 : 1;
		}
		printResult(std::get<sstv::audio::DiagnosticSnapshot>(result));
		return 0;
	} catch (const std::invalid_argument& error) {
		std::cerr << "Error: " << error.what() << '\n';
		return 2;
	} catch (const std::exception& error) {
		std::cerr << "Error: " << error.what() << '\n';
		return 1;
	}
}
