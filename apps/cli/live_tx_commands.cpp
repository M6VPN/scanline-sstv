// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/live_tx_commands.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "live_tx_commands.hpp"
#include "live_tx_signals.hpp"

#include <sstv/app/live_transmit.hpp>
#include <sstv/rig/flrig.hpp>
#include <sstv/rig/rigctld.hpp>

#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <exception>
#include <iomanip>
#include <iostream>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include <termios.h>
#include <unistd.h>

namespace {

constexpr std::uint32_t liveSampleRate = 48'000;
constexpr std::uint64_t maximumDelayMilliseconds = 10'000;
class PosixTerminal final : public LiveTransmitTerminal {
public:
	[[nodiscard]] bool isForegroundInteractive() const noexcept override
	{
		return isatty(STDIN_FILENO) == 1 && isatty(STDOUT_FILENO) == 1
			&& tcgetpgrp(STDIN_FILENO) == getpgrp();
	}
	void write(const std::string_view text) override
	{
		std::cout << text << std::flush;
	}
	[[nodiscard]] std::optional<std::string> readLine() override
	{
		std::string line;
		if (!std::getline(std::cin, line)) return std::nullopt;
		return line;
	}
};

[[nodiscard]] std::uint64_t
parseUnsigned(const std::string_view value, const std::string_view name)
{
	std::uint64_t parsed = 0;
	const auto [end, error] = std::from_chars(
		value.data(), value.data() + value.size(), parsed);
	if (value.empty() || error != std::errc{}
		|| end != value.data() + value.size()) {
		throw std::invalid_argument("invalid " + std::string(name) + ": "
			+ std::string(value));
	}
	return parsed;
}

[[nodiscard]] double
parseDouble(const std::string_view value, const std::string_view name)
{
	double parsed = 0.0;
	const auto [end, error] = std::from_chars(
		value.data(), value.data() + value.size(), parsed);
	if (value.empty() || error != std::errc{}
		|| end != value.data() + value.size() || !std::isfinite(parsed)) {
		throw std::invalid_argument("invalid " + std::string(name) + ": "
			+ std::string(value));
	}
	return parsed;
}

[[nodiscard]] sstv::image::CropRect
parseCrop(const std::string_view value)
{
	std::array<std::uint64_t, 4> values{};
	std::size_t start = 0;
	for (std::size_t index = 0; index < values.size(); ++index) {
		const std::size_t end = value.find(',', start);
		if ((index != values.size() - 1 && end == std::string_view::npos)
			|| (index == values.size() - 1 && end != std::string_view::npos)) {
			throw std::invalid_argument("crop must be X,Y,WIDTH,HEIGHT");
		}
		const std::size_t count = end == std::string_view::npos
			? value.size() - start : end - start;
		values[index] = parseUnsigned(value.substr(start, count), "crop");
		start = end == std::string_view::npos ? value.size() : end + 1;
	}
	if (values[2] == 0 || values[3] == 0) {
		throw std::invalid_argument("crop width and height must be nonzero");
	}
	return {values[0], values[1], values[2], values[3]};
}

[[nodiscard]] std::uint8_t
parseHexByte(const std::string_view value)
{
	unsigned int parsed = 0;
	const auto [end, error] = std::from_chars(
		value.data(), value.data() + value.size(), parsed, 16);
	if (error != std::errc{} || end != value.data() + value.size()
		|| parsed > 255U) {
		throw std::invalid_argument("background must be RRGGBB");
	}
	return static_cast<std::uint8_t>(parsed);
}

[[nodiscard]] sstv::core::Rgb8Pixel
parseBackground(const std::string_view value)
{
	if (value.size() != 6) {
		throw std::invalid_argument("background must be RRGGBB");
	}
	return {parseHexByte(value.substr(0, 2)), parseHexByte(value.substr(2, 2)),
		parseHexByte(value.substr(4, 2))};
}

struct SeenOptions {
	bool mode = false;
	bool input = false;
	bool fit = false;
	bool crop = false;
	bool background = false;
	bool fsk = false;
	bool backend = false;
	bool playbackIdentity = false;
	bool outputChannel = false;
	bool playbackChannels = false;
	bool provider = false;
	bool address = false;
	bool port = false;
	bool path = false;
	bool preKey = false;
	bool postAudio = false;
	bool gain = false;
	bool armAudio = false;
	bool armPtt = false;
	bool armLive = false;
};

void
markOnce(bool& seen, const std::string_view argument)
{
	if (seen) throw std::invalid_argument("duplicate option: " + std::string(argument));
	seen = true;
}

void
setFlag(const std::string_view argument, SeenOptions& seen)
{
	if (argument == "--arm-real-audio") markOnce(seen.armAudio, argument);
	else if (argument == "--arm-automatic-ptt") markOnce(seen.armPtt, argument);
	else if (argument == "--arm-live-tx") markOnce(seen.armLive, argument);
	else throw std::invalid_argument("unexpected flag: " + std::string(argument));
}

void
setValue(LiveTransmitOptions& options, SeenOptions& seen,
	const std::string_view argument, const std::string_view value)
{
	if (argument == "--mode") {
		markOnce(seen.mode, argument);
		options.mode = value;
	} else if (argument == "--input") {
		markOnce(seen.input, argument);
		options.input = value;
	} else if (argument == "--fit") {
		markOnce(seen.fit, argument);
		if (value == "contain") options.fit = sstv::image::FitMode::contain;
		else if (value == "cover") options.fit = sstv::image::FitMode::cover;
		else throw std::invalid_argument("fit must be contain or cover");
	} else if (argument == "--crop") {
		markOnce(seen.crop, argument);
		options.crop = parseCrop(value);
	} else if (argument == "--background") {
		markOnce(seen.background, argument);
		options.background = parseBackground(value);
	} else if (argument == "--fsk-id") {
		markOnce(seen.fsk, argument);
		auto result = sstv::analog::validateFskIdentifier(value);
		if (const auto* error = std::get_if<sstv::analog::FskIdError>(&result)) {
			throw std::invalid_argument(error->message);
		}
		options.fskIdentifier = std::get<sstv::analog::FskIdentifier>(result);
	} else if (argument == "--backend") {
		markOnce(seen.backend, argument);
		const auto backend = sstv::audio::parseAudioBackend(value);
		if (!backend || !sstv::audio::isRealAudioBackend(*backend)) {
			throw std::invalid_argument("invalid real audio backend: " + std::string(value));
		}
		options.backend = *backend;
	} else if (argument == "--playback-id") {
		markOnce(seen.playbackIdentity, argument);
		if (value.empty()) throw std::invalid_argument("playback identity must not be empty");
		options.playbackIdentity = value;
	} else if (argument == "--output-channel") {
		markOnce(seen.outputChannel, argument);
		const std::uint64_t parsed = parseUnsigned(value, "output channel");
		if (parsed > std::numeric_limits<std::uint32_t>::max()) {
			throw std::invalid_argument("output channel is out of range");
		}
		options.outputChannel = static_cast<std::uint32_t>(parsed);
	} else if (argument == "--playback-channels") {
		markOnce(seen.playbackChannels, argument);
		const std::uint64_t parsed = parseUnsigned(value, "playback channels");
		if (parsed == 0 || parsed > sstv::audio::maximumAudioChannels) {
			throw std::invalid_argument("playback channels are out of range");
		}
		options.playbackChannels = static_cast<std::uint32_t>(parsed);
	} else if (argument == "--ptt-provider") {
		markOnce(seen.provider, argument);
		if (value == "flrig") options.pttProvider = LivePttProvider::flrig;
		else if (value == "rigctld") options.pttProvider = LivePttProvider::rigctld;
		else throw std::invalid_argument("PTT provider must be flrig or rigctld");
	} else if (argument == "--ptt-address") {
		markOnce(seen.address, argument);
		options.pttAddress = value;
	} else if (argument == "--ptt-port") {
		markOnce(seen.port, argument);
		const std::uint64_t parsed = parseUnsigned(value, "PTT port");
		if (parsed == 0 || parsed > std::numeric_limits<std::uint16_t>::max()) {
			throw std::invalid_argument("PTT port is out of range");
		}
		options.pttPort = static_cast<std::uint16_t>(parsed);
	} else if (argument == "--flrig-path") {
		markOnce(seen.path, argument);
		options.flrigPath = std::string(value);
	} else if (argument == "--pre-key-ms" || argument == "--post-audio-ms") {
		bool& field = argument == "--pre-key-ms" ? seen.preKey : seen.postAudio;
		markOnce(field, argument);
		const std::uint64_t parsed = parseUnsigned(value, "delay");
		if (parsed > maximumDelayMilliseconds) {
			throw std::invalid_argument("live transmit delay exceeds 10000 ms");
		}
		const auto delay = std::chrono::milliseconds(parsed);
		if (argument == "--pre-key-ms") options.preKeyDelay = delay;
		else options.postAudioTail = delay;
	} else if (argument == "--gain-dbfs") {
		markOnce(seen.gain, argument);
		options.gainDecibelsFullScale = parseDouble(value, "transmit gain");
	} else {
		throw std::invalid_argument("unexpected option: " + std::string(argument));
	}
}

void
validateRequired(const LiveTransmitOptions& options, const SeenOptions& seen)
{
	if (!seen.mode || !seen.input || !seen.backend || !seen.playbackIdentity
		|| !seen.outputChannel || !seen.playbackChannels || !seen.provider
		|| !seen.address || !seen.port || !seen.preKey || !seen.postAudio
		|| !seen.gain) {
		throw std::invalid_argument("all live device, PTT, delay, and gain options are required");
	}
	if (!seen.armAudio || !seen.armPtt || !seen.armLive) {
		throw std::invalid_argument("--arm-real-audio, --arm-automatic-ptt, and --arm-live-tx are required");
	}
	if (options.outputChannel >= options.playbackChannels) {
		throw std::invalid_argument("output channel must be below playback channel count");
	}
	if (options.pttProvider == LivePttProvider::flrig && !seen.path) {
		throw std::invalid_argument("--flrig-path is required for flrig");
	}
	if (options.pttProvider == LivePttProvider::rigctld && seen.path) {
		throw std::invalid_argument("--flrig-path is not valid with rigctld");
	}
	if (options.input.empty()) throw std::invalid_argument("input path must not be empty");
}

[[nodiscard]] std::shared_ptr<sstv::rig::PttProvider>
createPttProvider(const LiveTransmitOptions& options,
	const std::shared_ptr<sstv::rig::MonotonicClock>& clock)
{
	if (options.pttProvider == LivePttProvider::flrig) {
		sstv::rig::FlrigConfiguration configuration;
		configuration.address = options.pttAddress;
		configuration.port = options.pttPort;
		configuration.path = *options.flrigPath;
		if (const auto error = sstv::rig::validateFlrigConfiguration(configuration)) {
			throw std::invalid_argument(error->message);
		}
		return std::make_shared<sstv::rig::FlrigPttProvider>(
			std::move(configuration), clock);
	}
	sstv::rig::RigctldConfiguration configuration;
	configuration.address = options.pttAddress;
	configuration.port = options.pttPort;
	if (const auto error = sstv::rig::validateRigctldConfiguration(configuration)) {
		throw std::invalid_argument(error->message);
	}
	return std::make_shared<sstv::rig::RigctldPttProvider>(
		std::move(configuration), clock);
}

[[nodiscard]] std::string_view
providerName(const LivePttProvider provider) noexcept
{
	return provider == LivePttProvider::flrig ? "flrig" : "rigctld";
}

} // namespace

LiveTransmitOptions
parseLiveTransmitOptions(const std::span<const std::string_view> arguments)
{
	LiveTransmitOptions options;
	SeenOptions seen;
	for (std::size_t index = 0; index < arguments.size(); ++index) {
		const std::string_view argument = arguments[index];
		if (argument == "--arm-real-audio" || argument == "--arm-automatic-ptt"
			|| argument == "--arm-live-tx") {
			setFlag(argument, seen);
			continue;
		}
		if (!argument.starts_with("--")) {
			throw std::invalid_argument("unexpected argument: " + std::string(argument));
		}
		if (++index >= arguments.size() || arguments[index].starts_with("--")) {
			throw std::invalid_argument("missing value for " + std::string(argument));
		}
		setValue(options, seen, argument, arguments[index]);
	}
	validateRequired(options, seen);
	if (options.pttProvider == LivePttProvider::flrig) {
		sstv::rig::FlrigConfiguration configuration{
			options.pttAddress, options.pttPort, *options.flrigPath};
		if (const auto error = sstv::rig::validateFlrigConfiguration(configuration)) {
			throw std::invalid_argument(error->message);
		}
	} else {
		sstv::rig::RigctldConfiguration configuration;
		configuration.address = options.pttAddress;
		configuration.port = options.pttPort;
		if (const auto error = sstv::rig::validateRigctldConfiguration(configuration)) {
			throw std::invalid_argument(error->message);
		}
	}
	return options;
}

bool
confirmLiveTransmit(const LiveTransmitOptions& options,
	const std::string_view duration, const std::uint64_t frameCount,
	LiveTransmitTerminal& terminal)
{
	if (!terminal.isForegroundInteractive()) return false;
	std::string summary;
	{
		std::ostringstream output;
		output.imbue(std::locale::classic());
		output << "LIVE SSTV TRANSMIT SAFETY CHECK\n"
			<< "Mode: " << options.mode << "  Duration: " << duration
			<< "  Frames: " << frameCount << '\n'
			<< "FSK ID: " << (options.fskIdentifier ? options.fskIdentifier->value() : "none") << '\n'
			<< "Audio: " << sstv::audio::audioBackendApiName(options.backend) << ':'
			<< options.playbackIdentity << " channel " << options.outputChannel
			<< " of " << options.playbackChannels << '\n'
			<< "Software gain: " << options.gainDecibelsFullScale << " dBFS\n"
			<< "PTT: " << providerName(options.pttProvider) << ' '
			<< options.pttAddress << ':' << options.pttPort << '\n'
			<< "Delays: pre-key " << options.preKeyDelay.count()
			<< " ms, post-audio " << options.postAudioTail.count() << " ms\n"
			<< "Disable VOX. Verify interface and frequency. Use minimum practical RF power.\n"
			<< "Keep an immediate hardware unkey method available.\n"
			<< "Type " << liveTransmitConfirmationPhrase << " exactly: ";
		summary = output.str();
	}
	terminal.write(summary);
	const std::optional<std::string> response = terminal.readLine();
	return response && *response == liveTransmitConfirmationPhrase;
}

void
printLiveTransmitCommandHelp()
{
	std::cout
		<< "  scanline-sstv-cli transmit-image --mode MODE --input INPUT\n"
		   "      [--fit contain|cover] [--crop X,Y,WIDTH,HEIGHT]\n"
		   "      [--background RRGGBB] [--fsk-id TEXT]\n"
		   "      --backend BACKEND --playback-id ID --output-channel N\n"
		   "      --playback-channels N --ptt-provider flrig|rigctld\n"
		   "      --ptt-address 127.0.0.1|::1 --ptt-port PORT\n"
		   "      [--flrig-path PATH] --pre-key-ms MS --post-audio-ms MS\n"
		   "      --gain-dbfs DB --arm-real-audio --arm-automatic-ptt\n"
		   "      --arm-live-tx\n"
		   "    Live TX is interactive-only and has no default device or PTT endpoint.\n";
}

int
runLiveTransmitCommand(const int argc, char* argv[])
{
	try {
		std::vector<std::string_view> arguments;
		arguments.reserve(static_cast<std::size_t>(argc - 2));
		for (int index = 2; index < argc; ++index) arguments.emplace_back(argv[index]);
		const LiveTransmitOptions options = parseLiveTransmitOptions(arguments);
		const sstv::app::LiveTransmitPreparationResult preparedResult
			= sstv::app::prepareLiveTransmit({sstv::app::OfflineEditorRequest{
				1, options.mode, options.input, options.fit, options.crop,
				options.background, liveSampleRate, options.fskIdentifier},
				options.gainDecibelsFullScale});
		if (const auto* error = std::get_if<sstv::app::LiveTransmitError>(&preparedResult)) {
			throw std::runtime_error(error->operation + ": " + error->message);
		}
		const sstv::app::LiveTransmitPrepared prepared
			= std::get<sstv::app::LiveTransmitPrepared>(preparedResult);
		const long double seconds
			= static_cast<long double>(prepared.snapshot->transmission.duration.numerator())
			/ static_cast<long double>(prepared.snapshot->transmission.duration.denominator());
		std::ostringstream duration;
		duration.imbue(std::locale::classic());
		duration << std::fixed << std::setprecision(6) << seconds << " seconds";
		PosixTerminal terminal;
		if (!confirmLiveTransmit(options, duration.str(),
			prepared.snapshot->frameCount, terminal)) {
			std::cerr << "Error: live transmission was not interactively confirmed\n";
			return 2;
		}
		auto clock = sstv::rig::createSteadyMonotonicClock();
		auto scheduler = sstv::rig::createSteadyMonotonicScheduler(clock);
		auto provider = createPttProvider(options, clock);
		sstv::audio::AudioDiscoveryService discovery(
			sstv::audio::createMiniaudioDiscoveryProvider());
		const auto discoveryResult = discovery.refresh({{options.backend}, false});
		if (const auto* error = std::get_if<sstv::audio::DiscoveryError>(&discoveryResult)) {
			throw std::runtime_error(error->message);
		}
		const auto snapshot = std::get<std::shared_ptr<const sstv::audio::AudioDiscoverySnapshot>>(
			discoveryResult);
		const auto selection = sstv::app::selectLivePlaybackDevice(*snapshot,
			{options.backend, options.playbackIdentity, options.playbackChannels,
				options.outputChannel});
		if (const auto* error = std::get_if<sstv::app::LiveTransmitError>(&selection)) {
			throw std::runtime_error(error->operation + ": " + error->message);
		}
		auto sourceResult = sstv::app::createLiveTransmitSource(prepared);
		if (const auto* error = std::get_if<sstv::app::SampleSourceError>(&sourceResult)) {
			throw std::runtime_error(error->message);
		}
		auto endpointResult = sstv::app::createAudioStreamTransmitEndpoint(
			{std::get<sstv::audio::AudioStreamConfiguration>(selection), snapshot},
			sstv::audio::createMiniaudioStreamAdapter());
		if (const auto* error = std::get_if<sstv::app::TransmitAudioError>(&endpointResult)) {
			throw std::runtime_error(error->operation + ": " + error->message);
		}
		auto supervisor = std::make_shared<sstv::rig::PttSupervisor>(provider, clock);
		sstv::app::TransmitCoordinator coordinator(supervisor, clock, scheduler);
		sstv::app::TransmitRequest request;
		request.policy.preKeyDelay = options.preKeyDelay;
		request.policy.postAudioTail = options.postAudioTail;
		LiveTransmitSignalScope signals;
		sstv::app::TransmitRunResult result;
		std::exception_ptr workerError;
		std::atomic<bool> isDone{false};
		std::jthread worker([&] {
			try {
				result = coordinator.run(request,
					std::get<std::unique_ptr<sstv::app::FiniteSampleSource>>(
						std::move(sourceResult)),
					std::get<std::unique_ptr<sstv::app::TransmitAudioEndpoint>>(
						std::move(endpointResult)));
			} catch (...) {
				workerError = std::current_exception();
				result = coordinator.shutdown();
			}
			isDone.store(true, std::memory_order_release);
		});
		bool cancellationSent = false;
		while (!isDone.load(std::memory_order_acquire)) {
			if (signals.isCancellationRequested() && !cancellationSent) {
				coordinator.requestCancel();
				cancellationSent = true;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		worker.join();
		if (workerError) std::rethrow_exception(workerError);
		if (result->outcome != sstv::app::TransmitOutcome::completed) {
			std::cerr << "Error: " << result->message << '\n';
			if (result->ptt.hasHazard) {
				std::cerr << "HAZARD: PTT could not be confirmed unkeyed\n";
			}
			return cancellationSent ? 130 : 1;
		}
		std::cout << "Live transmission completed; PTT confirmed unkeyed.\n";
		return 0;
	} catch (const std::invalid_argument& error) {
		std::cerr << "Error: " << error.what() << '\n';
		return 2;
	} catch (const std::exception& error) {
		std::cerr << "Error: " << error.what() << '\n';
		return 1;
	}
}
