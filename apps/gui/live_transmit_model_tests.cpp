// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/gui/live_transmit_model_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "live_transmit_model.hpp"
#include "prepared_image_provider.hpp"
#include "tx_editor_model.hpp"

#include <QTest>

#include <atomic>
#include <memory>

namespace {

class DeviceProvider final : public sstv::audio::AudioDiscoveryProvider {
public:
	[[nodiscard]] bool isBackendCompiled(const sstv::audio::AudioBackend) const override
	{
		return true;
	}
	[[nodiscard]] sstv::audio::BackendDiscovery discoverBackend(
		const sstv::audio::AudioBackend backend, const std::stop_token) override
	{
		sstv::audio::AudioDevice device;
		device.identity = {backend, sstv::audio::AudioDirection::playback,
			"hex:0102", sstv::audio::IdentityStability::persistent};
		device.name = "Injected playback\nname";
		return {backend, std::string(sstv::audio::audioBackendApiName(backend)), true,
			sstv::audio::BackendStatus::available, {device}, {}};
	}
};

class NoopPttProvider final : public sstv::rig::PttProvider {
public:
	[[nodiscard]] sstv::rig::PttOperationResult execute(
		const sstv::rig::PttRequest&) noexcept override
	{
		++calls;
		return {};
	}
	std::atomic<std::size_t> calls{0};
};

class ModelRuntime final : public sstv::app::LiveTransmitRuntime {
public:
	[[nodiscard]] std::shared_ptr<sstv::audio::AudioDiscoveryProvider>
	createDiscoveryProvider() override
	{
		++discoveryCreations;
		return std::make_shared<DeviceProvider>();
	}
	[[nodiscard]] std::unique_ptr<sstv::audio::AudioStreamAdapter>
	createAudioAdapter() override
	{
		++audioCreations;
		return nullptr;
	}
	[[nodiscard]] std::shared_ptr<sstv::rig::MonotonicClock> createClock() override
	{
		++clockCreations;
		return sstv::rig::createSteadyMonotonicClock();
	}
	[[nodiscard]] std::shared_ptr<sstv::rig::MonotonicScheduler> createScheduler(
		std::shared_ptr<sstv::rig::MonotonicClock> clock) override
	{
		return sstv::rig::createSteadyMonotonicScheduler(std::move(clock));
	}
	[[nodiscard]] std::variant<std::shared_ptr<sstv::rig::PttProvider>,
		sstv::app::LiveTransmitError> createPttProvider(
		const sstv::app::LivePttConfiguration&,
		std::shared_ptr<sstv::rig::MonotonicClock>) override
	{
		++pttCreations;
		return std::shared_ptr<sstv::rig::PttProvider>(provider);
	}
	std::shared_ptr<NoopPttProvider> provider = std::make_shared<NoopPttProvider>();
	std::atomic<std::size_t> discoveryCreations{0};
	std::atomic<std::size_t> audioCreations{0};
	std::atomic<std::size_t> clockCreations{0};
	std::atomic<std::size_t> pttCreations{0};
};

} // namespace

class LiveTransmitModelTests final : public QObject {
	Q_OBJECT

private slots:
	void readinessAndConfirmationGate();
	void editorChangeInvalidatesReadiness();
};

void
LiveTransmitModelTests::readinessAndConfirmationGate()
{
	PreparedImageProvider imageProvider;
	TxEditorModel editor(&imageProvider);
	editor.selectInput(QUrl::fromLocalFile(
		QStringLiteral(SSTV_IMAGE_FIXTURE_DIR "/marker.png")));
	editor.updateRequest(QStringLiteral("martin-m1"), QStringLiteral("contain"),
		QStringLiteral("000000"), false, 0, 0, 1, 1, 48'000, false, {});
	QTRY_COMPARE_WITH_TIMEOUT(editor.state(), QStringLiteral("ready"), 20'000);
	auto runtime = std::make_shared<ModelRuntime>();
	auto service = std::make_shared<sstv::app::LiveTransmitService>(runtime);
	LiveTransmitModel model(&editor, service);
	QVERIFY(!model.isReady());
	model.refreshDevices(QStringLiteral("alsa"));
	QTRY_COMPARE_WITH_TIMEOUT(model.playbackDevices().size(), 1, 5'000);
	QCOMPARE(runtime->discoveryCreations.load(), std::size_t(1));
	model.updateConfiguration(QStringLiteral("alsa"), QStringLiteral("hex:0102"),
		0, 2, -30.0, QStringLiteral("flrig"), QStringLiteral("127.0.0.1"),
		12'345, QStringLiteral("/RPC2"), 250, 250);
	QVERIFY(model.isReady());
	model.refreshDevices(QStringLiteral("alsa"));
	QTRY_COMPARE_WITH_TIMEOUT(runtime->discoveryCreations.load(), std::size_t(2), 5'000);
	QTRY_COMPARE_WITH_TIMEOUT(model.selectedPlaybackIdentity(), QStringLiteral("hex:0102"), 5'000);
	model.updateConfiguration(QStringLiteral("alsa"), QStringLiteral("hex:0102"),
		0, 2, -30.0, QStringLiteral("flrig"), QStringLiteral("127.0.0.1"),
		12'345, QStringLiteral("/RPC2"), 250, 250);
	QVERIFY(model.isReady());
	model.confirmAndTransmit(true, true, false,
		QString::fromStdString(std::string(sstv::app::liveTransmitConfirmationPhrase)));
	QVERIFY(!model.isReady());
	QVERIFY(model.primaryError().contains(QStringLiteral("three arms")));
	QCOMPARE(runtime->clockCreations.load(), std::size_t(0));
	QCOMPARE(runtime->pttCreations.load(), std::size_t(0));
	QCOMPARE(runtime->audioCreations.load(), std::size_t(0));
}

void
LiveTransmitModelTests::editorChangeInvalidatesReadiness()
{
	PreparedImageProvider imageProvider;
	TxEditorModel editor(&imageProvider);
	editor.selectInput(QUrl::fromLocalFile(
		QStringLiteral(SSTV_IMAGE_FIXTURE_DIR "/marker.png")));
	editor.updateRequest(QStringLiteral("martin-m1"), QStringLiteral("contain"),
		QStringLiteral("000000"), false, 0, 0, 1, 1, 48'000, false, {});
	QTRY_COMPARE_WITH_TIMEOUT(editor.state(), QStringLiteral("ready"), 20'000);
	auto runtime = std::make_shared<ModelRuntime>();
	LiveTransmitModel model(&editor,
		std::make_shared<sstv::app::LiveTransmitService>(runtime));
	model.refreshDevices(QStringLiteral("alsa"));
	QTRY_COMPARE_WITH_TIMEOUT(model.playbackDevices().size(), 1, 5'000);
	model.updateConfiguration(QStringLiteral("alsa"), QStringLiteral("hex:0102"),
		0, 2, -30.0, QStringLiteral("rigctld"), QStringLiteral("::1"),
		12'345, QString(), 0, 0);
	QVERIFY(model.isReady());
	editor.updateRequest(QStringLiteral("scottie-s1"), QStringLiteral("contain"),
		QStringLiteral("000000"), false, 0, 0, 1, 1, 48'000, false, {});
	QVERIFY(!model.isReady());
	QCOMPARE(model.revision(), qulonglong(0));
}

QTEST_MAIN(LiveTransmitModelTests)

#include "live_transmit_model_tests.moc"
