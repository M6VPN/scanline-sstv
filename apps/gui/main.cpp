// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/gui/main.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_diagnostics_model.hpp"
#include "prepared_image_provider.hpp"
#include "tx_editor_model.hpp"
#if defined(SSTV_ENABLE_LIVE_TX)
#include "live_transmit_model.hpp"
#include <sstv/app/live_transmit_signals.hpp>
#endif

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStringList>
#include <QTimer>

#include <memory>

int
main(int argc, char *argv[])
{
	QGuiApplication application(argc, argv);
	QCoreApplication::setOrganizationName(QStringLiteral("M6VPN"));
	QCoreApplication::setApplicationName(QStringLiteral("Scanline SSTV"));
	QQuickStyle::setStyle(QStringLiteral("Fusion"));
	auto provider = std::make_unique<PreparedImageProvider>();
	PreparedImageProvider* providerPointer = provider.get();
	TxEditorModel editorModel(providerPointer);
	AudioDiagnosticsModel audioDiagnosticsModel;
#if defined(SSTV_ENABLE_LIVE_TX)
	LiveTransmitModel liveTransmitModel(&editorModel);
	sstv::app::LiveTransmitSignalScope liveSignalScope;
#endif
	QQmlApplicationEngine engine;
	engine.addImageProvider(QStringLiteral("prepared"), provider.release());
	engine.rootContext()->setContextProperty(
	    QStringLiteral("txEditorModel"), &editorModel);
	engine.rootContext()->setContextProperty(
	    QStringLiteral("audioDiagnosticsModel"), &audioDiagnosticsModel);
#if defined(SSTV_ENABLE_LIVE_TX)
	engine.rootContext()->setContextProperty(
	    QStringLiteral("liveTransmitModel"), &liveTransmitModel);
#endif
	engine.loadFromModule(QStringLiteral(SCANLINE_SSTV_QML_URI),
	    QStringLiteral("Main"));
	if (engine.rootObjects().isEmpty()) {
		return 1;
	}
#if defined(SSTV_ENABLE_LIVE_TX)
	QTimer signalTimer;
	signalTimer.setInterval(50);
	QObject::connect(&signalTimer, &QTimer::timeout, &application,
		[&engine, &liveSignalScope]() {
			if (!liveSignalScope.isCancellationRequested()) return;
			if (QObject* root = engine.rootObjects().front()) {
				QMetaObject::invokeMethod(root, "close");
			}
		});
	signalTimer.start();
#endif
	if (QCoreApplication::arguments().contains(
	    QStringLiteral("--smoke-test"))) {
		QObject* root = engine.rootObjects().front();
		QObject* workspace = root->findChild<QObject*>(
		    QStringLiteral("txEditorWorkspace"));
		QObject* notice = root->findChild<QObject*>(
		    QStringLiteral("offlineSafetyNotice"));
		QObject* audioPanel = root->findChild<QObject*>(
		    QStringLiteral("audioDiagnosticsPanel"));
		QObject* livePanel = root->findChild<QObject*>(
		    QStringLiteral("liveTransmitPanel"));
		if (workspace == nullptr || notice == nullptr
		    || audioPanel == nullptr
		    || editorModel.modeCount() != 4
		    || !notice->property("text").toString().contains(
			QStringLiteral("PTT"))) {
			return 2;
		}
#if defined(SSTV_ENABLE_LIVE_TX)
		if (livePanel == nullptr
			|| livePanel->property("ready").toBool()) return 4;
#else
		if (livePanel != nullptr) return 4;
#endif
		for (QObject* object : root->findChildren<QObject*>()) {
			if (object->property("text").toString() == QStringLiteral("Transmit")
			    && object->property("enabled").toBool()) {
				return 3;
			}
		}
		return 0;
	}
	return application.exec();
}
