// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m2h_live_transmit_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "live_tx_commands.hpp"

#include <sstv/app/live_transmit.hpp>
#include <sstv/dsp/tone_renderer.hpp>

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace {

#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
constexpr bool isThreadSanitizerBuild = true;
#else
constexpr bool isThreadSanitizerBuild = false;
#endif
#else
constexpr bool isThreadSanitizerBuild = false;
#endif

void
require(const bool condition, const std::string& message)
{
	if (!condition) throw std::runtime_error(message);
}

[[nodiscard]] std::vector<std::string_view>
validArguments()
{
	return {"--mode", "martin-m1", "--input", SSTV_IMAGE_FIXTURE_DIR "/marker.png",
		"--fit", "contain", "--background", "000000", "--backend", "alsa",
		"--playback-id", "hex:0102", "--output-channel", "0",
		"--playback-channels", "2", "--ptt-provider", "flrig",
		"--ptt-address", "127.0.0.1", "--ptt-port", "12345",
		"--flrig-path", "/RPC2", "--pre-key-ms", "250",
		"--post-audio-ms", "250", "--gain-dbfs", "-30",
		"--arm-real-audio", "--arm-automatic-ptt", "--arm-live-tx"};
}

class FakeTerminal final : public LiveTransmitTerminal {
public:
	bool isInteractive = true;
	std::optional<std::string> response{std::string(liveTransmitConfirmationPhrase)};
	std::string output;
	[[nodiscard]] bool isForegroundInteractive() const noexcept override
	{
		return isInteractive;
	}
	void write(const std::string_view text) override { output.append(text); }
	[[nodiscard]] std::optional<std::string> readLine() override { return response; }
};

void
testParserAndConfirmation()
{
	const LiveTransmitOptions options = parseLiveTransmitOptions(validArguments());
	require(options.mode == "martin-m1" && options.backend == sstv::audio::AudioBackend::alsa,
		"valid live options changed");
	require(options.outputChannel == 0 && options.playbackChannels == 2
		&& options.gainDecibelsFullScale == -30.0,
		"explicit channel or gain was not retained");
	for (const std::string_view arm : {"--arm-real-audio", "--arm-automatic-ptt",
		"--arm-live-tx"}) {
		auto arguments = validArguments();
		arguments.erase(std::find(arguments.begin(), arguments.end(), arm));
		bool rejected = false;
		try {
			(void)parseLiveTransmitOptions(arguments);
		} catch (const std::invalid_argument&) {
			rejected = true;
		}
		require(rejected, "missing arm flag was accepted");
	}
	auto duplicate = validArguments();
	duplicate.push_back("--arm-live-tx");
	require([&] {
		try { (void)parseLiveTransmitOptions(duplicate); }
		catch (const std::invalid_argument&) { return true; }
		return false;
	}(), "duplicate arm flag was accepted");
	auto remote = validArguments();
	*std::find(remote.begin(), remote.end(), "127.0.0.1") = "192.0.2.1";
	require([&] {
		try { (void)parseLiveTransmitOptions(remote); }
		catch (const std::invalid_argument&) { return true; }
		return false;
	}(), "non-loopback PTT address was accepted");
	auto rigctld = validArguments();
	*std::find(rigctld.begin(), rigctld.end(), "flrig") = "rigctld";
	const auto path = std::find(rigctld.begin(), rigctld.end(), "--flrig-path");
	rigctld.erase(path, path + 2);
	require(parseLiveTransmitOptions(rigctld).pttProvider == LivePttProvider::rigctld,
		"explicit rigctld provider was rejected");
	FakeTerminal terminal;
	require(confirmLiveTransmit(options, "115.000000 seconds", 5'520'000, terminal),
		"exact interactive confirmation was rejected");
	require(terminal.output.find("Disable VOX") != std::string::npos
		&& terminal.output.find("hardware unkey") != std::string::npos,
		"safety confirmation omitted required warnings");
	terminal.isInteractive = false;
	require(!confirmLiveTransmit(options, "115 seconds", 1, terminal),
		"noninteractive confirmation was accepted");
	terminal.isInteractive = true;
	terminal.response = "yes";
	require(!confirmLiveTransmit(options, "115 seconds", 1, terminal),
		"mismatched confirmation phrase was accepted");
}

[[nodiscard]] sstv::analog::OfflineTransmission
shortTransmission()
{
	return {{sstv::core::ToneEvent(
		sstv::core::Duration::fromMicroseconds(10'000), 1'000.0, 0.8F)},
		sstv::core::Duration::fromMicroseconds(10'000)};
}

void
testGainAndPreparation()
{
	const float gain = 0.5F;
	auto sourceResult = sstv::app::createToneEventSampleSource(
		shortTransmission(), 48'000, gain);
	require(std::holds_alternative<std::unique_ptr<sstv::app::FiniteSampleSource>>(
		sourceResult), "valid constant gain source was rejected");
	auto source = std::get<std::unique_ptr<sstv::app::FiniteSampleSource>>(
		std::move(sourceResult));
	std::vector<float> actual(480);
	const auto read = source->read(actual);
	require(std::get<std::size_t>(read) == actual.size(),
		"gain source returned the wrong frame count");
	sstv::dsp::ToneRenderer renderer(shortTransmission().events, 48'000);
	std::vector<float> expected(480);
	require(renderer.render(expected) == expected.size(), "direct renderer failed");
	for (std::size_t index = 0; index < actual.size(); ++index) {
		require(actual[index] == expected[index] * gain,
			"constant-gain source changed a rendered sample");
	}
	require(std::holds_alternative<sstv::app::SampleSourceError>(
		sstv::app::createToneEventSampleSource(shortTransmission(), 48'000, 2.0F)),
		"clipping gain was accepted");
	if (!isThreadSanitizerBuild) {
		const auto prepared = sstv::app::prepareLiveTransmit({
			{7, "martin-m1", SSTV_IMAGE_FIXTURE_DIR "/marker.png",
				sstv::image::FitMode::contain, std::nullopt, {0, 0, 0}, 48'000,
				std::nullopt}, -30.0});
		require(std::holds_alternative<sstv::app::LiveTransmitPrepared>(prepared),
			"valid immutable live preparation failed");
		const auto& value = std::get<sstv::app::LiveTransmitPrepared>(prepared);
		require(value.snapshot->revision == 7 && value.snapshot->mode.id == "martin-m1"
			&& value.snapshot->frameCount > 0,
			"prepared live snapshot facts are inconsistent");
		require(std::abs(value.gain.scalar - 0.031622775F) < 0.00000001F,
			"-30 dBFS scalar changed");
	}
}

void
testExactDeviceSelection()
{
	sstv::audio::DeviceIdentity identity{sstv::audio::AudioBackend::alsa,
		sstv::audio::AudioDirection::playback, "hex:0102",
		sstv::audio::IdentityStability::persistent};
	sstv::audio::AudioDevice device;
	device.identity = identity;
	sstv::audio::BackendDiscovery backend;
	backend.backend = sstv::audio::AudioBackend::alsa;
	backend.status = sstv::audio::BackendStatus::available;
	backend.devices.push_back(device);
	sstv::audio::AudioDiscoverySnapshot snapshot{41, {backend}, true};
	const auto selected = sstv::app::selectLivePlaybackDevice(snapshot,
		{sstv::audio::AudioBackend::alsa, "hex:0102", 2, 1});
	require(std::holds_alternative<sstv::audio::AudioStreamConfiguration>(selected),
		"exact playback identity was not selected");
	const auto& configuration
		= std::get<sstv::audio::AudioStreamConfiguration>(selected);
	require(configuration.playbackDevice->discoveryGeneration == 41
		&& configuration.selectedPlaybackChannel == 1,
		"fresh discovery generation or explicit channel was lost");
	backend.devices.front().hasIdentityCollision = true;
	snapshot.backends.front() = backend;
	require(std::holds_alternative<sstv::app::LiveTransmitError>(
		sstv::app::selectLivePlaybackDevice(snapshot,
			{sstv::audio::AudioBackend::alsa, "hex:0102", 2, 1})),
		"collision-marked identity was accepted");
	require(std::holds_alternative<sstv::app::LiveTransmitError>(
		sstv::app::selectLivePlaybackDevice(snapshot,
			{sstv::audio::AudioBackend::alsa, "missing", 2, 1})),
		"missing identity fell back to another device");
}

} // namespace

int
main()
{
	testParserAndConfirmation();
	testGainAndPreparation();
	testExactDeviceSelection();
	return 0;
}
