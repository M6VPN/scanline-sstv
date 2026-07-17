// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/gui/prepared_image_provider.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/core/rgb8_frame.hpp>

#include <QImage>
#include <QMutex>
#include <QQuickImageProvider>

class PreparedImageProvider final : public QQuickImageProvider {
public:
	PreparedImageProvider();
	void publish(quint64, sstv::core::Rgb8View);
	[[nodiscard]] QImage requestImage(const QString&, QSize*, const QSize&) override;
private:
	QImage image;
	QMutex mutex;
	quint64 revision = 0;
};

