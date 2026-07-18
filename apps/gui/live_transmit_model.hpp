// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/gui/live_transmit_model.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "tx_editor_model.hpp"

#include <sstv/app/live_transmit.hpp>
#include <sstv/app/hil_evidence.hpp>

#include <QObject>
#include <QThreadPool>
#include <QTimer>
#include <QVariantList>

#include <cstdint>
#include <memory>
#include <optional>

class LiveTransmitModel final : public QObject {
	Q_OBJECT
	Q_PROPERTY(QVariantList backends READ backends CONSTANT)
	Q_PROPERTY(QVariantList hilStages READ hilStages CONSTANT)
	Q_PROPERTY(QVariantList playbackDevices READ playbackDevices NOTIFY devicesChanged)
	Q_PROPERTY(QString selectedPlaybackIdentity READ selectedPlaybackIdentity NOTIFY devicesChanged)
	Q_PROPERTY(QString state READ state NOTIFY snapshotChanged)
	Q_PROPERTY(QString statusText READ statusText NOTIFY snapshotChanged)
	Q_PROPERTY(QString primaryError READ primaryError NOTIFY snapshotChanged)
	Q_PROPERTY(QString cleanupErrors READ cleanupErrors NOTIFY snapshotChanged)
	Q_PROPERTY(QString pttCertainty READ pttCertainty NOTIFY snapshotChanged)
	Q_PROPERTY(QString watchdogStatus READ watchdogStatus NOTIFY snapshotChanged)
	Q_PROPERTY(QString audioStatus READ audioStatus NOTIFY snapshotChanged)
	Q_PROPERTY(QString confirmationPhrase READ confirmationPhrase CONSTANT)
	Q_PROPERTY(bool ready READ isReady NOTIFY readinessChanged)
	Q_PROPERTY(bool active READ isActive NOTIFY snapshotChanged)
	Q_PROPERTY(bool hazardous READ isHazardous NOTIFY snapshotChanged)
	Q_PROPERTY(double progress READ progress NOTIFY snapshotChanged)
	Q_PROPERTY(qulonglong revision READ revision NOTIFY readinessChanged)

public:
	explicit LiveTransmitModel(TxEditorModel*, QObject* = nullptr);
	LiveTransmitModel(TxEditorModel*, std::shared_ptr<sstv::app::LiveTransmitService>,
		QObject* = nullptr);
	~LiveTransmitModel() override;
	[[nodiscard]] QVariantList backends() const;
	[[nodiscard]] QVariantList hilStages() const;
	[[nodiscard]] QVariantList playbackDevices() const;
	[[nodiscard]] QString selectedPlaybackIdentity() const;
	[[nodiscard]] QString state() const;
	[[nodiscard]] QString statusText() const;
	[[nodiscard]] QString primaryError() const;
	[[nodiscard]] QString cleanupErrors() const;
	[[nodiscard]] QString pttCertainty() const;
	[[nodiscard]] QString watchdogStatus() const;
	[[nodiscard]] QString audioStatus() const;
	[[nodiscard]] QString confirmationPhrase() const;
	[[nodiscard]] bool isReady() const;
	[[nodiscard]] bool isActive() const;
	[[nodiscard]] bool isHazardous() const;
	[[nodiscard]] double progress() const;
	[[nodiscard]] qulonglong revision() const;

	Q_INVOKABLE void refreshDevices(const QString&);
	Q_INVOKABLE void updateConfiguration(const QString&, const QString&, int, int,
		double, const QString&, const QString&, int, const QString&, int, int);
	Q_INVOKABLE void confirmAndTransmit(bool, bool, bool, const QString&);
	Q_INVOKABLE void checkPttState(const QString&, const QString&, int, const QString&);
	Q_INVOKABLE void stop();
	Q_INVOKABLE void retryUnkey();
	Q_INVOKABLE bool requestWindowClose();

signals:
	void devicesChanged();
	void snapshotChanged();
	void readinessChanged();
	void safeToClose();

private:
	void invalidate();
	void pollSnapshot();
	void setError(const QString&);
	void acceptDiscovery(std::uint64_t, const sstv::audio::DiscoveryResult&,
		const QString&);
	[[nodiscard]] std::optional<sstv::audio::AudioBackend> parseBackend(
		const QString&) const;
	[[nodiscard]] std::optional<sstv::app::LivePttConfiguration> parsePtt(
		const QString&, const QString&, int, const QString&) const;
	[[nodiscard]] bool isTerminal() const;

	TxEditorModel* editorModel;
	std::shared_ptr<sstv::app::LiveTransmitService> service;
	QThreadPool threadPool;
	QTimer snapshotTimer;
	QVariantList backendValues;
	QVariantList playbackValues;
	QString retainedPlaybackIdentity;
	QString currentState = QStringLiteral("idle");
	QString currentStatus = QStringLiteral("Prepare an image and configure exact live resources.");
	QString currentPrimaryError;
	QString currentCleanupErrors;
	QString currentPttCertainty = QStringLiteral("unknown");
	QString currentWatchdogStatus = QStringLiteral("not armed");
	QString currentAudioStatus = QStringLiteral("closed");
	double currentProgress = 0.0;
	std::uint64_t requestGeneration = 0;
	std::uint64_t configuredRevision = 0;
	std::uint64_t lastSnapshotSequence = 0;
	bool hasConfiguration = false;
	bool hasExactDevice = false;
	bool closePending = false;
};
