// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/gui/audio_diagnostics_model_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_diagnostics_model.hpp"

#include <QTest>

#include <memory>

namespace {

class GuiProvider final : public sstv::audio::AudioDiscoveryProvider {
public:
	std::size_t calls = 0;
	[[nodiscard]] bool isBackendCompiled(sstv::audio::AudioBackend) const override
	{
		return true;
	}
	[[nodiscard]] sstv::audio::BackendDiscovery discoverBackend(
	    const sstv::audio::AudioBackend backend, std::stop_token) override
	{
		++calls;
		using namespace sstv::audio;
		return {backend, std::string(audioBackendApiName(backend)), true,
		    BackendStatus::available,
		    {{{backend, AudioDirection::playback, "play", IdentityStability::sessionOnly},
		         "Mock output", false, AudioTransport::virtualDevice, {}, false, {}},
		        {{backend, AudioDirection::capture, "capture", IdentityStability::sessionOnly},
		         "Mock input", false, AudioTransport::virtualDevice, {}, false, {}}}, {}};
	}
};

} // namespace

class AudioDiagnosticsModelTests final : public QObject {
	Q_OBJECT

private slots:
	void refreshesWithoutSelectingAndRejectsUnarmedOutput();
};

void
AudioDiagnosticsModelTests::refreshesWithoutSelectingAndRejectsUnarmedOutput()
{
	auto provider = std::make_shared<GuiProvider>();
	std::size_t adapterCreations = 0;
	AudioDiagnosticsModel model(provider, [&adapterCreations] {
		++adapterCreations;
		return std::unique_ptr<sstv::audio::AudioStreamAdapter>();
	});
	QVERIFY(model.backends().size() >= 3);
	QVERIFY(!model.hasDevices());
	model.refreshDevices(QStringLiteral("alsa"));
	QVERIFY(model.isRunning());
	model.refreshDevices(QStringLiteral("jack"));
	QTRY_COMPARE_WITH_TIMEOUT(model.state(), QStringLiteral("idle"), 5'000);
	QVERIFY(!model.isRunning());
	QCOMPARE(provider->calls, 1U);
	QCOMPARE(model.playbackDevices().size(), 1);
	QCOMPARE(model.captureDevices().size(), 1);
	QVERIFY(model.hasDevices());
	model.startOutput(QStringLiteral("alsa"), QStringLiteral("play"),
	    0, 2, 1, 480, 3, -30.0, false);
	QVERIFY(model.isRunning());
	QTRY_COMPARE_WITH_TIMEOUT(model.state(), QStringLiteral("faulted"), 5'000);
	QVERIFY(!model.isRunning());
	QVERIFY(model.statusText().contains(QStringLiteral("fresh per-run arm")));
	QCOMPARE(provider->calls, 1U);
	QCOMPARE(adapterCreations, 0U);
	model.stop();
}

QTEST_MAIN(AudioDiagnosticsModelTests)

#include "audio_diagnostics_model_tests.moc"
