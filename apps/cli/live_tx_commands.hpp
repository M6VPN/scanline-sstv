// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/cli/live_tx_commands.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/analog/fsk_id.hpp>
#include <sstv/audio/audio_discovery.hpp>
#include <sstv/core/rgb8_frame.hpp>
#include <sstv/image/image.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>

inline constexpr std::string_view liveTransmitConfirmationPhrase
	= "TRANSMIT LIVE SSTV";

enum class LivePttProvider {
	flrig,
	rigctld,
};

struct LiveTransmitOptions {
	std::string mode;
	std::filesystem::path input;
	sstv::image::FitMode fit = sstv::image::FitMode::contain;
	std::optional<sstv::image::CropRect> crop;
	sstv::core::Rgb8Pixel background{0, 0, 0};
	std::optional<sstv::analog::FskIdentifier> fskIdentifier;
	sstv::audio::AudioBackend backend = sstv::audio::AudioBackend::alsa;
	std::string playbackIdentity;
	std::uint32_t outputChannel = 0;
	std::uint32_t playbackChannels = 0;
	LivePttProvider pttProvider = LivePttProvider::flrig;
	std::string pttAddress;
	std::uint16_t pttPort = 0;
	std::optional<std::string> flrigPath;
	std::chrono::milliseconds preKeyDelay{0};
	std::chrono::milliseconds postAudioTail{0};
	double gainDecibelsFullScale = 0.0;
};

class LiveTransmitTerminal {
public:
	virtual ~LiveTransmitTerminal() = default;
	[[nodiscard]] virtual bool isForegroundInteractive() const noexcept = 0;
	virtual void write(std::string_view) = 0;
	[[nodiscard]] virtual std::optional<std::string> readLine() = 0;
};

[[nodiscard]] LiveTransmitOptions parseLiveTransmitOptions(
	std::span<const std::string_view>);
[[nodiscard]] bool confirmLiveTransmit(const LiveTransmitOptions&,
	std::string_view, std::uint64_t, LiveTransmitTerminal&);
void printLiveTransmitCommandHelp();
[[nodiscard]] int runLiveTransmitCommand(int, char*[]);
