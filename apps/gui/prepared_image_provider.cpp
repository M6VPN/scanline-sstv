// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/gui/prepared_image_provider.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "prepared_image_provider.hpp"

#include <QMutexLocker>

#include <cstddef>
#include <cstdint>
#include <utility>

PreparedImageProvider::PreparedImageProvider()
	: QQuickImageProvider(QQuickImageProvider::Image)
{
}

void
PreparedImageProvider::publish(const quint64 value, const sstv::core::Rgb8View frame)
{
	QImage next(frame.width(), frame.height(), QImage::Format_RGB888);
	for (std::uint16_t y = 0; y < frame.height(); ++y) {
		auto* row = next.scanLine(y);
		for (std::uint16_t x = 0; x < frame.width(); ++x) {
			const auto& pixel = frame.pixel(x, y);
			const std::size_t offset = static_cast<std::size_t>(x) * 3U;
			row[offset] = pixel.red;
			row[offset + 1U] = pixel.green;
			row[offset + 2U] = pixel.blue;
		}
	}
	QMutexLocker lock(&mutex);
	image = std::move(next);
	revision = value;
}

QImage
PreparedImageProvider::requestImage(const QString& id, QSize* size, const QSize&)
{
	bool valid = false;
	const quint64 requested = id.toULongLong(&valid);
	QMutexLocker lock(&mutex);
	if (!valid || requested != revision) return {};
	if (size != nullptr) *size = image.size();
	return image;
}
