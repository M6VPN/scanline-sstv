// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/audio/miniaudio_adapter.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/audio/audio_discovery.hpp>

#include <miniaudio.h>

#include <optional>
#include <string>
#include <string_view>

namespace sstv::audio::detail {

[[nodiscard]] std::optional<ma_backend> toMiniaudioBackend(AudioBackend backend);
[[nodiscard]] std::string serializeDeviceId(
    AudioBackend backend, const ma_device_id& id);
[[nodiscard]] bool deserializeDeviceId(
    AudioBackend backend, std::string_view value, ma_device_id& id) noexcept;

} // namespace sstv::audio::detail
