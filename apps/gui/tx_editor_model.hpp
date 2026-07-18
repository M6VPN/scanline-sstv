// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/gui/tx_editor_model.hpp
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "prepared_image_provider.hpp"

#include <sstv/app/offline_tx_editor.hpp>

#include <QObject>
#include <QThreadPool>
#include <QUrl>
#include <QVariantList>

#include <filesystem>
#include <memory>
#include <optional>

class TxEditorModel final : public QObject {
	Q_OBJECT
	Q_PROPERTY(QVariantList modes READ modes CONSTANT)
	Q_PROPERTY(QVariantList sampleRates READ sampleRates CONSTANT)
	Q_PROPERTY(int modeCount READ modeCount CONSTANT)
	Q_PROPERTY(QString state READ state NOTIFY stateChanged)
	Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
	Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
	Q_PROPERTY(QString previewSource READ previewSource NOTIFY previewChanged)
	Q_PROPERTY(bool ready READ isReady NOTIFY stateChanged)
	Q_PROPERTY(bool busy READ isBusy NOTIFY stateChanged)
	Q_PROPERTY(QString sourceName READ sourceName NOTIFY metadataChanged)
	Q_PROPERTY(QString sourceDimensions READ sourceDimensions NOTIFY metadataChanged)
	Q_PROPERTY(QString orientedDimensions READ orientedDimensions NOTIFY metadataChanged)
	Q_PROPERTY(QString modeMetadata READ modeMetadata NOTIFY metadataChanged)
	Q_PROPERTY(QString durationMetadata READ durationMetadata NOTIFY metadataChanged)
	Q_PROPERTY(QString frameMetadata READ frameMetadata NOTIFY metadataChanged)
	Q_PROPERTY(QString inspectionText READ inspectionText NOTIFY inspectionChanged)
	Q_PROPERTY(QString platformName READ platformName CONSTANT)
	Q_PROPERTY(qulonglong preparedRevision READ preparedRevision NOTIFY metadataChanged)

public:
	explicit TxEditorModel(PreparedImageProvider*, QObject* = nullptr);
	~TxEditorModel() override;

	[[nodiscard]] QVariantList modes() const;
	[[nodiscard]] QVariantList sampleRates() const;
	[[nodiscard]] int modeCount() const;
	[[nodiscard]] QString state() const;
	[[nodiscard]] QString errorText() const;
	[[nodiscard]] QString statusText() const;
	[[nodiscard]] QString previewSource() const;
	[[nodiscard]] bool isReady() const;
	[[nodiscard]] bool isBusy() const;
	[[nodiscard]] QString sourceName() const;
	[[nodiscard]] QString sourceDimensions() const;
	[[nodiscard]] QString orientedDimensions() const;
	[[nodiscard]] QString modeMetadata() const;
	[[nodiscard]] QString durationMetadata() const;
	[[nodiscard]] QString frameMetadata() const;
	[[nodiscard]] QString inspectionText() const;
	[[nodiscard]] QString platformName() const;
	[[nodiscard]] qulonglong preparedRevision() const;
	[[nodiscard]] std::shared_ptr<const sstv::app::OfflineEditorSnapshot>
		preparedSnapshot() const;

	Q_INVOKABLE void selectInput(const QUrl&);
	Q_INVOKABLE void updateRequest(const QString&, const QString&, const QString&,
		bool, int, int, int, int, int, bool, const QString&);
	Q_INVOKABLE void exportPng(const QUrl&, bool replace = false);
	Q_INVOKABLE void exportWav(const QUrl&, bool replace = false);
	Q_INVOKABLE void inspectWav(const QUrl&);

signals:
	void stateChanged();
	void previewChanged();
	void metadataChanged();
	void inspectionChanged();
	void overwriteRequired(const QUrl&, const QString&);

private:
	void clearPreparedState();
	void setError(const QString&);
	void setState(const QString&, const QString&);
	[[nodiscard]] std::optional<std::filesystem::path> localPath(
		const QUrl&, const QString&);
	void acceptSnapshot(const sstv::app::SnapshotResult&, std::uint64_t);
	void acceptPngExport(const sstv::app::PngExportResult&, const QUrl&, std::uint64_t);
	void acceptWavExport(const sstv::app::WavEditorExportResult&, std::uint64_t);
	void acceptInspection(const sstv::app::InspectionResult&);

	PreparedImageProvider* imageProvider;
	QThreadPool threadPool;
	QVariantList modeValues;
	QVariantList rateValues;
	sstv::app::GenerationGate generationGate;
	std::filesystem::path inputPath;
	std::shared_ptr<const sstv::app::OfflineEditorSnapshot> snapshot;
	QString currentState = QStringLiteral("initial");
	QString currentError;
	QString currentStatus = QStringLiteral("Choose a local JPEG or PNG image.");
	QString currentPreviewSource;
	QString currentInspection;
};
