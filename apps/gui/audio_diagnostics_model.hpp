// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/gui/audio_diagnostics_model.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <sstv/audio/audio_diagnostics.hpp>

#include <QObject>
#include <QThreadPool>
#include <QVariantList>

#include <cstdint>
#include <memory>

class AudioDiagnosticsModel final : public QObject {
	Q_OBJECT
	Q_PROPERTY(QVariantList backends READ backends CONSTANT)
	Q_PROPERTY(QVariantList playbackDevices READ playbackDevices NOTIFY devicesChanged)
	Q_PROPERTY(QVariantList captureDevices READ captureDevices NOTIFY devicesChanged)
	Q_PROPERTY(QString state READ state NOTIFY snapshotChanged)
	Q_PROPERTY(QString statusText READ statusText NOTIFY snapshotChanged)
	Q_PROPERTY(QString resultText READ resultText NOTIFY snapshotChanged)
	Q_PROPERTY(bool running READ isRunning NOTIFY snapshotChanged)
	Q_PROPERTY(bool hasDevices READ hasDevices NOTIFY devicesChanged)
	Q_PROPERTY(double peakDbfs READ peakDbfs NOTIFY snapshotChanged)

public:
	explicit AudioDiagnosticsModel(QObject* = nullptr);
	AudioDiagnosticsModel(std::shared_ptr<sstv::audio::AudioDiscoveryProvider>,
	    sstv::audio::AudioStreamAdapterFactory, QObject* = nullptr);
	~AudioDiagnosticsModel() override;
	[[nodiscard]] QVariantList backends() const;
	[[nodiscard]] QVariantList playbackDevices() const;
	[[nodiscard]] QVariantList captureDevices() const;
	[[nodiscard]] QString state() const;
	[[nodiscard]] QString statusText() const;
	[[nodiscard]] QString resultText() const;
	[[nodiscard]] bool isRunning() const;
	[[nodiscard]] bool hasDevices() const;
	[[nodiscard]] double peakDbfs() const;

	Q_INVOKABLE void refreshDevices(const QString&);
	Q_INVOKABLE void startMeter(const QString&, const QString&, int, int, int, int, int);
	Q_INVOKABLE void startOutput(const QString&, const QString&, int, int, int,
	    int, int, double, bool);
	Q_INVOKABLE void startLoopback(const QString&, const QString&, const QString&,
	    int, int, int, int, int, int, double, bool);
	Q_INVOKABLE void stop();

signals:
	void devicesChanged();
	void snapshotChanged();

private:
	void start(sstv::audio::DiagnosticRequest);
	void acceptSnapshot(std::uint64_t,
	    std::shared_ptr<const sstv::audio::DiagnosticSnapshot>);
	void acceptResult(std::uint64_t, const sstv::audio::DiagnosticResult&);
	[[nodiscard]] std::optional<sstv::audio::AudioBackend> parseBackend(
	    const QString&) const;
	[[nodiscard]] std::optional<sstv::audio::DeviceIdentity> identity(
	    const QString&, sstv::audio::AudioDirection) const;

	QThreadPool threadPool;
	std::shared_ptr<sstv::audio::AudioDiagnosticsService> service;
	QVariantList backendValues;
	QVariantList playbackValues;
	QVariantList captureValues;
	std::shared_ptr<const sstv::audio::AudioDiscoverySnapshot> discovery;
	QString currentState = QStringLiteral("idle");
	QString currentStatus = QStringLiteral("Refresh devices to begin.");
	QString currentResult;
	double currentPeakDbfs = sstv::audio::silenceDbfsFloor;
	std::uint64_t requestRevision = 0;
	bool isBusy = false;
};
