// SPDX-License-Identifier: GPL-3.0-or-later

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char* argv[]) {
    QGuiApplication application(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("M6VPN"));
    QCoreApplication::setApplicationName(QStringLiteral("SSTV Transceiver"));
    QQuickStyle::setStyle(QStringLiteral("Fusion"));

    QQmlApplicationEngine engine;
    engine.loadFromModule("org.m6vpn.SstvTransceiver", "Main");
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return application.exec();
}

