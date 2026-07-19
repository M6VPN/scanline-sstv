// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/tests/m2j_b1_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include <sstv/app/hil_evidence.hpp>
#include <sstv/audio/audio_discovery.hpp>
#include <sstv/rig/ptt.hpp>

#include <cassert>
#include <memory>
#include <stop_token>

namespace {

class Provider final : public sstv::audio::AudioDiscoveryProvider {
public:
	[[nodiscard]] bool isBackendCompiled(sstv::audio::AudioBackend) const override { return true; }
	[[nodiscard]] sstv::audio::BackendDiscovery discoverBackend(
		sstv::audio::AudioBackend backend, std::stop_token) override
	{
		return {backend, "alsa", true, sstv::audio::BackendStatus::available,
			{{{backend, sstv::audio::AudioDirection::playback, "exact",
				sstv::audio::IdentityStability::sessionOnly}, "USB", false,
				sstv::audio::AudioTransport::unknown, {}, false, {}}}, {}};
	}
};

class Classifier final : public sstv::audio::AudioTransportClassifier {
public:
	[[nodiscard]] sstv::audio::AudioTransport classify(
		const sstv::audio::AudioDevice&) const override { return sstv::audio::AudioTransport::unknown; }
	void enrich(sstv::audio::AudioDevice& device) const override
	{
		device.transport = sstv::audio::AudioTransport::usb;
		device.usb = sstv::audio::UsbMetadata{"0d8c", "0012", "C-Media", "USB Audio Device", "1-1.1", false};
	}
};

class QueryUnkeyOnlyProvider final : public sstv::rig::PttProvider {
public:
	[[nodiscard]] sstv::rig::PttOperationResult execute(
		const sstv::rig::PttRequest& request) noexcept override
	{
		assert(request.action != sstv::rig::PttAction::key);
		return {request.action, sstv::rig::PttObservedState::unkeyed,
			sstv::rig::PttCertainty::definitelyUnkeyed,
			sstv::rig::PttReadback::available};
	}
};

} // namespace

int
main()
{
	/* The classifier metadata never changes the native selector. */
	sstv::audio::AudioDiscoveryService service(std::make_shared<Provider>(),
		std::make_shared<Classifier>());
	const auto result = service.refresh({{sstv::audio::AudioBackend::alsa}, false});
	assert(std::holds_alternative<std::shared_ptr<const sstv::audio::AudioDiscoverySnapshot>>(result));
	const auto snapshot = std::get<std::shared_ptr<const sstv::audio::AudioDiscoverySnapshot>>(result);
	assert(snapshot->backends.front().devices.front().identity.opaque == "exact");
	assert(snapshot->backends.front().devices.front().usb->vendorId == "0d8c");
	assert(snapshot->backends.front().devices.front().transport == sstv::audio::AudioTransport::usb);
	/* PulseAudio server names are not treated as proof of a USB transport. */
	auto pulse = sstv::audio::AudioDevice{
		{sstv::audio::AudioBackend::pulseAudio, sstv::audio::AudioDirection::playback,
			"616c73615f6f75747075742e757362", sstv::audio::IdentityStability::persistent},
		"USB Audio Device Analog Stereo", false, sstv::audio::AudioTransport::unknown,
		{}, false, {}};
	const auto classifier = sstv::audio::createSystemAudioTransportClassifier();
	pulse.transport = classifier->classify(pulse);
	classifier->enrich(pulse);
	assert(pulse.transport == sstv::audio::AudioTransport::unknown && !pulse.usb.has_value());
	const std::string digest(64U, 'a');
	sstv::app::HilStageAuthorizer authorizer;
	const auto permit = authorizer.authorize(sstv::app::HilStage::discovery, digest,
		sstv::app::hilStageConfirmationPhrase(sstv::app::HilStage::discovery, digest), true);
	assert(std::holds_alternative<sstv::app::HilStagePermit>(permit));
	assert(!authorizer.consume(std::get<sstv::app::HilStagePermit>(permit),
		sstv::app::HilStage::discovery, digest));
	assert(authorizer.consume(std::get<sstv::app::HilStagePermit>(permit),
		sstv::app::HilStage::discovery, digest));
	QueryUnkeyOnlyProvider provider;
	const auto result = provider.execute({sstv::rig::PttAction::query, {}, 1U, 1U});
	assert(result.certainty == sstv::rig::PttCertainty::definitelyUnkeyed);
	return 0;
}
