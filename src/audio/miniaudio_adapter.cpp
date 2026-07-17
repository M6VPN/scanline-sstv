// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/src/audio/miniaudio_adapter.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "miniaudio_adapter.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace sstv::audio {
namespace {

constexpr std::size_t maximumDevicesPerBackend = 4'096;
constexpr std::size_t maximumNativeFormats = 64;

struct EnumeratedDevice {
	ma_device_id id{};
	ma_device_type type = ma_device_type_playback;
	std::string name;
	bool isDefault = false;
};

struct EnumerationData {
	std::vector<EnumeratedDevice> devices;
	std::stop_token stopToken;
	bool exceededLimit = false;
};

class MiniaudioContext {
public:
	MiniaudioContext() = default;
	MiniaudioContext(const MiniaudioContext&) = delete;
	MiniaudioContext& operator=(const MiniaudioContext&) = delete;
	~MiniaudioContext()
	{
		if (isInitialized_) {
			(void)ma_context_uninit(&context_);
		}
	}
	[[nodiscard]] ma_result initialize(const ma_backend backend)
	{
		ma_context_config config = ma_context_config_init();
		config.pulse.pApplicationName = "Scanline SSTV discovery";
		config.pulse.tryAutoSpawn = MA_FALSE;
		config.jack.pClientName = "scanline-sstv-discovery";
		config.jack.tryStartServer = MA_FALSE;
		const ma_result result = ma_context_init(&backend, 1, &config, &context_);
		isInitialized_ = result == MA_SUCCESS;
		return result;
	}
	[[nodiscard]] ma_context* get()
	{
		return &context_;
	}

private:
	ma_context context_{};
	bool isInitialized_ = false;
};

[[nodiscard]] std::size_t
boundedLength(const char* const value, const std::size_t capacity)
{
	const char* const end = std::find(value, value + capacity, '\0');
	return static_cast<std::size_t>(end - value);
}

[[nodiscard]] std::string
hexEncode(const std::span<const char> bytes)
{
	constexpr std::array digits{'0', '1', '2', '3', '4', '5', '6', '7',
	    '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	std::string result;
	result.reserve(bytes.size() * 2);
	for (const char byte : bytes) {
		const auto value = static_cast<unsigned char>(byte);
		result.push_back(digits[value >> 4U]);
		result.push_back(digits[value & 0x0FU]);
	}
	return result;
}

template<std::size_t Capacity>
[[nodiscard]] std::string
serializeStringId(const char (&value)[Capacity])
{
	return hexEncode(std::span<const char>(value, boundedLength(value, Capacity)));
}

[[nodiscard]] AudioDirection
toDirection(const ma_device_type type)
{
	return type == ma_device_type_capture
	    ? AudioDirection::capture : AudioDirection::playback;
}

[[nodiscard]] SampleFormat
toSampleFormat(const ma_format format)
{
	switch (format) {
	case ma_format_u8: return SampleFormat::unsigned8;
	case ma_format_s16: return SampleFormat::signed16;
	case ma_format_s24: return SampleFormat::signed24;
	case ma_format_s32: return SampleFormat::signed32;
	case ma_format_f32: return SampleFormat::float32;
	default: return SampleFormat::unknown;
	}
}

void
copyCapabilities(const ma_device_info& info, DeviceCapabilities& capabilities)
{
	const std::size_t count = std::min<std::size_t>(
	    info.nativeDataFormatCount, maximumNativeFormats);
	capabilities.nativeFormats.reserve(count);
	for (std::size_t index = 0; index < count; ++index) {
		const auto& source = info.nativeDataFormats[index];
		capabilities.nativeFormats.push_back(NativeDataFormat{
		    toSampleFormat(source.format),
		    source.channels == 0 ? std::nullopt
		        : std::optional<std::uint32_t>{source.channels},
		    source.sampleRate == 0 ? std::nullopt
		        : std::optional<std::uint32_t>{source.sampleRate},
		    (source.flags & MA_DATA_FORMAT_FLAG_EXCLUSIVE_MODE) != 0});
		if (source.channels != 0) {
			capabilities.minimumChannels = capabilities.minimumChannels
			    ? std::min(*capabilities.minimumChannels, source.channels) : source.channels;
			capabilities.maximumChannels = capabilities.maximumChannels
			    ? std::max(*capabilities.maximumChannels, source.channels) : source.channels;
		}
		if (source.sampleRate != 0) {
			capabilities.minimumSampleRate = capabilities.minimumSampleRate
			    ? std::min(*capabilities.minimumSampleRate, source.sampleRate)
			    : source.sampleRate;
			capabilities.maximumSampleRate = capabilities.maximumSampleRate
			    ? std::max(*capabilities.maximumSampleRate, source.sampleRate)
			    : source.sampleRate;
		}
	}
	capabilities.detailsKnown = true;
}

ma_bool32
enumerateCallback(ma_context*, const ma_device_type type,
    const ma_device_info* const info, void* const userData)
{
	auto& data = *static_cast<EnumerationData*>(userData);
	if (data.stopToken.stop_requested()) {
		return MA_FALSE;
	}
	if (data.devices.size() >= maximumDevicesPerBackend) {
		data.exceededLimit = true;
		return MA_FALSE;
	}
	const std::size_t nameLength = boundedLength(info->name, sizeof(info->name));
	data.devices.push_back(EnumeratedDevice{
	    info->id, type, std::string(info->name, nameLength), info->isDefault == MA_TRUE});
	return MA_TRUE;
}

[[nodiscard]] bool
canQueryDetails(const AudioBackend backend)
{
	return backend == AudioBackend::pulseAudio || backend == AudioBackend::jack;
}

[[nodiscard]] IdentityStability
identityStability(const AudioBackend backend, const std::string& opaque)
{
	if (backend == AudioBackend::pulseAudio && !opaque.empty()) {
		return IdentityStability::persistent;
	}
	return IdentityStability::sessionOnly;
}

class MiniaudioDiscoveryProvider final : public AudioDiscoveryProvider {
public:
	[[nodiscard]] bool isBackendCompiled(const AudioBackend backend) const override
	{
		const std::optional<ma_backend> mapped = detail::toMiniaudioBackend(backend);
		return mapped.has_value() && ma_is_backend_enabled(*mapped) == MA_TRUE;
	}

	[[nodiscard]] BackendDiscovery discoverBackend(
	    const AudioBackend backend, const std::stop_token stopToken) override
	{
		BackendDiscovery result{backend, std::string(audioBackendApiName(backend)),
		    isBackendCompiled(backend), BackendStatus::unavailable, {}, {}};
		const std::optional<ma_backend> mapped = detail::toMiniaudioBackend(backend);
		if (!mapped || !result.isCompiled) {
			result.status = BackendStatus::uncompiled;
			result.diagnostic = "backend is not compiled";
			return result;
		}
		MiniaudioContext context;
		const ma_result initializeResult = context.initialize(*mapped);
		if (initializeResult != MA_SUCCESS) {
			result.diagnostic = ma_result_description(initializeResult);
			return result;
		}
		if (backend == AudioBackend::sndio) {
			result.status = BackendStatus::safeEnumerationUnsupported;
			result.diagnostic = "pinned sndio enumeration opens audio endpoints";
			return result;
		}
		EnumerationData data{{}, stopToken, false};
		const ma_result enumerationResult = ma_context_enumerate_devices(
		    context.get(), enumerateCallback, &data);
		if (stopToken.stop_requested()) {
			result.status = BackendStatus::cancelled;
			result.diagnostic = "backend enumeration was cancelled";
			return result;
		}
		if (data.exceededLimit) {
			result.diagnostic = "backend exceeded the device enumeration limit";
			return result;
		}
		if (enumerationResult != MA_SUCCESS) {
			result.diagnostic = ma_result_description(enumerationResult);
			return result;
		}
		result.devices.reserve(data.devices.size());
		for (const EnumeratedDevice& source : data.devices) {
			const std::string opaque = detail::serializeDeviceId(backend, source.id);
			AudioDevice device{DeviceIdentity{backend, toDirection(source.type), opaque,
			    identityStability(backend, opaque)}, source.name, source.isDefault,
			    AudioTransport::unknown, {}, false, {}};
			if (canQueryDetails(backend)) {
				ma_device_info detailInfo{};
				const ma_result detailResult = ma_context_get_device_info(
				    context.get(), source.type, &source.id, &detailInfo);
				if (detailResult == MA_SUCCESS) {
					copyCapabilities(detailInfo, device.capabilities);
				} else {
					device.diagnostic = std::string("capabilities unavailable: ")
					    + ma_result_description(detailResult);
				}
			}
			result.devices.push_back(std::move(device));
		}
		result.status = BackendStatus::available;
		if (!canQueryDetails(backend)) {
			result.diagnostic = "basic enumeration only; detailed probing is not endpoint-safe";
		}
		return result;
	}
};

} // namespace

namespace detail {

std::optional<ma_backend>
toMiniaudioBackend(const AudioBackend backend)
{
	switch (backend) {
	case AudioBackend::pulseAudio: return ma_backend_pulseaudio;
	case AudioBackend::jack: return ma_backend_jack;
	case AudioBackend::alsa: return ma_backend_alsa;
	case AudioBackend::oss: return ma_backend_oss;
	case AudioBackend::sndio: return ma_backend_sndio;
	case AudioBackend::audio4: return ma_backend_audio4;
	case AudioBackend::nullDiagnostic: return ma_backend_null;
	}
	return std::nullopt;
}

std::string
serializeDeviceId(const AudioBackend backend, const ma_device_id& id)
{
	switch (backend) {
	case AudioBackend::pulseAudio: return serializeStringId(id.pulse);
	case AudioBackend::jack: return std::to_string(id.jack);
	case AudioBackend::alsa: return serializeStringId(id.alsa);
	case AudioBackend::oss: return serializeStringId(id.oss);
	case AudioBackend::sndio: return serializeStringId(id.sndio);
	case AudioBackend::audio4: return serializeStringId(id.audio4);
	case AudioBackend::nullDiagnostic: return std::to_string(id.nullbackend);
	}
	return {};
}

} // namespace detail

std::shared_ptr<AudioDiscoveryProvider>
createMiniaudioDiscoveryProvider()
{
	return std::make_shared<MiniaudioDiscoveryProvider>();
}

} // namespace sstv::audio
