// SPDX-License-Identifier: GPL-3.0-or-later

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStringList>

int
main(int argc, char *argv[])
{
	QGuiApplication application(argc, argv);
	QCoreApplication::setOrganizationName(QStringLiteral("M6VPN"));
	QCoreApplication::setApplicationName(QStringLiteral("Scanline SSTV"));
	QQuickStyle::setStyle(QStringLiteral("Fusion"));
	QQmlApplicationEngine engine;
	engine.loadFromModule(QStringLiteral(SCANLINE_SSTV_QML_URI),
	    QStringLiteral("Main"));
	if (engine.rootObjects().isEmpty()) {
		return 1;
	}
	if (QCoreApplication::arguments().contains(
	    QStringLiteral("--smoke-test"))) {
		return 0;
	}
	return application.exec();
}
