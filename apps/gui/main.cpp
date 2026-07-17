// SPDX-License-Identifier: GPL-3.0-or-later

#include "prepared_image_provider.hpp"
#include "tx_editor_model.hpp"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStringList>

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
	QQmlApplicationEngine engine;
	engine.addImageProvider(QStringLiteral("prepared"), provider.release());
	engine.rootContext()->setContextProperty(
	    QStringLiteral("txEditorModel"), &editorModel);
	engine.loadFromModule(QStringLiteral(SCANLINE_SSTV_QML_URI),
	    QStringLiteral("Main"));
	if (engine.rootObjects().isEmpty()) {
		return 1;
	}
	if (QCoreApplication::arguments().contains(
	    QStringLiteral("--smoke-test"))) {
		QObject* root = engine.rootObjects().front();
		QObject* workspace = root->findChild<QObject*>(
		    QStringLiteral("txEditorWorkspace"));
		QObject* notice = root->findChild<QObject*>(
		    QStringLiteral("offlineSafetyNotice"));
		if (workspace == nullptr || notice == nullptr
		    || editorModel.modeCount() != 4
		    || !notice->property("text").toString().contains(
			QStringLiteral("PTT"))) {
			return 2;
		}
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
