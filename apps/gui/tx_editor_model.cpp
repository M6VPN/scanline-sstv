// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/gui/tx_editor_model.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "tx_editor_model.hpp"

#include <sstv/analog/fsk_id.hpp>
#include <sstv/offline/wav_writer.hpp>

#include <QFutureWatcher>
#include <QGuiApplication>
#include <QVariantMap>
#include <QtConcurrentRun>

#include <iomanip>
#include <sstream>
#include <variant>

namespace {

[[nodiscard]] QString
formatDuration(const sstv::core::Duration duration)
{
	const long double seconds = static_cast<long double>(duration.numerator())
		/ static_cast<long double>(duration.denominator());
	return QString::number(static_cast<double>(seconds), 'f', 3) + QStringLiteral(" s");
}

[[nodiscard]] QString
formatInspection(const sstv::offline::WavInspection& value)
{
	std::ostringstream output;
	output.imbue(std::locale::classic());
	output << "RIFF/WAVE, PCM format " << value.audioFormat << '\n'
		<< "Channels: " << value.channels << '\n'
		<< "Sample rate: " << value.sampleRate << " Hz\n"
		<< "Bits per sample: " << value.bitsPerSample << '\n'
		<< "Data bytes: " << value.dataBytes << '\n'
		<< "Frames: " << value.frameCount << '\n'
		<< "Duration: " << std::fixed << std::setprecision(6)
		<< static_cast<long double>(value.duration.numerator())
			/ static_cast<long double>(value.duration.denominator()) << " s\n"
		<< "Sample range: " << value.minimumSample << " to " << value.maximumSample << '\n'
		<< "Peak absolute: " << value.peakAbsolute << '\n'
		<< "DC mean: " << value.dcMean << '\n'
		<< "RMS: " << value.rmsLevel << '\n'
		<< "Clipped + / -: " << value.clippedPositiveSamples << " / "
		<< value.clippedNegativeSamples;
	return QString::fromStdString(output.str());
}

[[nodiscard]] QString
formatError(const sstv::app::EditorError& error)
{
	return QString::fromStdString(error.operation + ": " + error.message);
}

} // namespace

TxEditorModel::TxEditorModel(PreparedImageProvider* provider, QObject* parent)
	: QObject(parent), imageProvider(provider)
{
	threadPool.setMaxThreadCount(2);
	const auto modeResult = sstv::app::offlineImageModes();
	if (const auto* error = std::get_if<sstv::app::EditorError>(&modeResult)) {
		setError(formatError(*error));
	} else {
		for (const auto& mode : std::get<std::vector<sstv::app::OfflineModeInfo>>(modeResult)) {
			QVariantMap value;
			value.insert(QStringLiteral("id"), QString::fromStdString(mode.id));
			value.insert(QStringLiteral("displayName"), QString::fromStdString(mode.displayName));
			value.insert(QStringLiteral("width"), mode.width);
			value.insert(QStringLiteral("height"), mode.height);
			value.insert(QStringLiteral("colourEncoding"), QString::fromStdString(mode.colourEncoding));
			value.insert(QStringLiteral("vis"), mode.visCode.has_value()
				? QString::number(*mode.visCode) : QStringLiteral("not applicable"));
			value.insert(QStringLiteral("baseDuration"), formatDuration(mode.baseDuration));
			modeValues.push_back(value);
		}
	}
	for (const std::uint32_t rate : sstv::offline::supportedSampleRates()) {
		rateValues.push_back(rate);
	}
}

TxEditorModel::~TxEditorModel()
{
	threadPool.waitForDone();
}

QVariantList TxEditorModel::modes() const { return modeValues; }
QVariantList TxEditorModel::sampleRates() const { return rateValues; }
int TxEditorModel::modeCount() const { return static_cast<int>(modeValues.size()); }
QString TxEditorModel::state() const { return currentState; }
QString TxEditorModel::errorText() const { return currentError; }
QString TxEditorModel::statusText() const { return currentStatus; }
QString TxEditorModel::previewSource() const { return currentPreviewSource; }
bool TxEditorModel::isReady() const { return snapshot != nullptr; }
bool TxEditorModel::isBusy() const
{
	return currentState == QStringLiteral("loading")
		|| currentState == QStringLiteral("exporting");
}
QString TxEditorModel::sourceName() const
{
	return inputPath.empty() ? QStringLiteral("No image selected")
		: QString::fromStdString(inputPath.filename().string());
}
QString TxEditorModel::sourceDimensions() const
{
	if (!snapshot) return QStringLiteral("-");
	return QStringLiteral("%1 x %2").arg(snapshot->prepared.source.width)
		.arg(snapshot->prepared.source.height);
}
QString TxEditorModel::orientedDimensions() const
{
	if (!snapshot) return QStringLiteral("-");
	return QStringLiteral("%1 x %2 (orientation %3)")
		.arg(snapshot->prepared.source.orientedWidth)
		.arg(snapshot->prepared.source.orientedHeight)
		.arg(static_cast<int>(snapshot->prepared.source.orientation));
}
QString TxEditorModel::modeMetadata() const
{
	if (!snapshot) return QStringLiteral("-");
	return QStringLiteral("%1 | %2 x %3 | %4 | VIS %5")
		.arg(QString::fromStdString(snapshot->mode.displayName))
		.arg(snapshot->mode.width).arg(snapshot->mode.height)
		.arg(QString::fromStdString(snapshot->mode.colourEncoding))
		.arg(snapshot->mode.visCode.has_value()
			? QString::number(*snapshot->mode.visCode) : QStringLiteral("N/A"));
}
QString TxEditorModel::durationMetadata() const
{
	if (!snapshot) return QStringLiteral("-");
	return QStringLiteral("Base %1 | Combined %2")
		.arg(formatDuration(snapshot->mode.baseDuration))
		.arg(formatDuration(snapshot->transmission.duration));
}
QString TxEditorModel::frameMetadata() const
{
	if (!snapshot) return QStringLiteral("-");
	return QStringLiteral("%1 frames | %2 Hz | %3 bytes projected | FSK ID %4")
		.arg(snapshot->frameCount).arg(snapshot->sampleRate)
		.arg(snapshot->projectedFileBytes)
		.arg(snapshot->fskIdentifier.has_value()
			? QString::fromStdString(*snapshot->fskIdentifier) : QStringLiteral("off"));
}
QString TxEditorModel::inspectionText() const { return currentInspection; }
QString TxEditorModel::platformName() const { return QGuiApplication::platformName(); }

void
TxEditorModel::clearPreparedState()
{
	snapshot.reset();
	currentPreviewSource.clear();
	emit previewChanged();
	emit metadataChanged();
}

void
TxEditorModel::setError(const QString& error)
{
	currentState = QStringLiteral("error");
	currentError = error;
	currentStatus = QStringLiteral("Operation failed.");
	emit stateChanged();
}

void
TxEditorModel::setState(const QString& value, const QString& status)
{
	currentState = value;
	currentError.clear();
	currentStatus = status;
	emit stateChanged();
}

std::optional<std::filesystem::path>
TxEditorModel::localPath(const QUrl& url, const QString& purpose)
{
	if (!url.isValid() || !url.isLocalFile()) {
		setError(purpose + QStringLiteral(" must be a local filesystem path."));
		return std::nullopt;
	}
	return std::filesystem::path(url.toLocalFile().toStdString());
}

void
TxEditorModel::selectInput(const QUrl& url)
{
	const auto path = localPath(url, QStringLiteral("Input"));
	if (!path) return;
	inputPath = *path;
	static_cast<void>(generationGate.issue());
	clearPreparedState();
	setState(QStringLiteral("initial"), QStringLiteral("Image selected; preparing current settings."));
	emit metadataChanged();
}

void
TxEditorModel::updateRequest(const QString& modeId, const QString& fit,
	const QString& background, const bool cropEnabled, const int cropX,
	const int cropY, const int cropWidth, const int cropHeight,
	const int sampleRate, const bool fskEnabled, const QString& fskText)
{
	const std::uint64_t revision = generationGate.issue();
	clearPreparedState();
	if (inputPath.empty()) {
		setError(QStringLiteral("Choose a local JPEG or PNG image first."));
		return;
	}
	bool backgroundOk = false;
	const uint backgroundValue = background.toUInt(&backgroundOk, 16);
	if (!backgroundOk || background.size() != 6) {
		setError(QStringLiteral("Background must be exactly six hexadecimal digits."));
		return;
	}
	std::optional<sstv::image::CropRect> crop;
	if (cropEnabled) {
		if (cropX < 0 || cropY < 0 || cropWidth <= 0 || cropHeight <= 0) {
			setError(QStringLiteral("Crop values require nonnegative coordinates and positive dimensions."));
			return;
		}
		crop = sstv::image::CropRect{static_cast<std::uint64_t>(cropX),
			static_cast<std::uint64_t>(cropY), static_cast<std::uint64_t>(cropWidth),
			static_cast<std::uint64_t>(cropHeight)};
	}
	std::optional<sstv::analog::FskIdentifier> fskIdentifier;
	if (fskEnabled) {
		const auto validation = sstv::analog::validateFskIdentifier(fskText.toStdString());
		if (const auto* error = std::get_if<sstv::analog::FskIdError>(&validation)) {
			setError(QString::fromStdString(error->message));
			return;
		}
		fskIdentifier = std::get<sstv::analog::FskIdentifier>(validation);
	}
	const sstv::core::Rgb8Pixel colour{
		static_cast<std::uint8_t>((backgroundValue >> 16U) & 0xffU),
		static_cast<std::uint8_t>((backgroundValue >> 8U) & 0xffU),
		static_cast<std::uint8_t>(backgroundValue & 0xffU)};
	const sstv::app::OfflineEditorRequest request{revision, modeId.toStdString(), inputPath,
		fit == QStringLiteral("cover") ? sstv::image::FitMode::cover
			: sstv::image::FitMode::contain,
		crop, colour, static_cast<std::uint32_t>(sampleRate), std::move(fskIdentifier)};
	setState(QStringLiteral("loading"), QStringLiteral("Preparing image and offline waveform metadata..."));
	auto* watcher = new QFutureWatcher<sstv::app::SnapshotResult>(this);
	connect(watcher, &QFutureWatcher<sstv::app::SnapshotResult>::finished, this,
		[this, watcher, revision]() {
			acceptSnapshot(watcher->result(), revision);
			watcher->deleteLater();
		});
	watcher->setFuture(QtConcurrent::run(&threadPool,
		[request]() { return sstv::app::prepareOfflineEditor(request); }));
}

void
TxEditorModel::acceptSnapshot(const sstv::app::SnapshotResult& result,
	const std::uint64_t revision)
{
	if (!generationGate.isCurrent(revision)) return;
	if (const auto* error = std::get_if<sstv::app::EditorError>(&result)) {
		setError(formatError(*error));
		return;
	}
	snapshot = std::get<std::shared_ptr<const sstv::app::OfflineEditorSnapshot>>(result);
	imageProvider->publish(snapshot->revision, snapshot->prepared.frame.view());
	currentPreviewSource = QStringLiteral("image://prepared/%1").arg(snapshot->revision);
	emit previewChanged();
	emit metadataChanged();
	setState(QStringLiteral("ready"), QStringLiteral("Prepared preview is current and ready to export."));
}

void
TxEditorModel::exportPng(const QUrl& url, const bool replace)
{
	if (!snapshot) {
		setError(QStringLiteral("Prepare the current image before export."));
		return;
	}
	const auto path = localPath(url, QStringLiteral("PNG output"));
	if (!path) return;
	const auto captured = snapshot;
	const std::uint64_t revision = captured->revision;
	setState(QStringLiteral("exporting"), QStringLiteral("Publishing prepared PNG..."));
	auto* watcher = new QFutureWatcher<sstv::app::PngExportResult>(this);
	connect(watcher, &QFutureWatcher<sstv::app::PngExportResult>::finished, this,
		[this, watcher, url, revision]() {
			acceptPngExport(watcher->result(), url, revision);
			watcher->deleteLater();
		});
	watcher->setFuture(QtConcurrent::run(&threadPool, [captured, path = *path, replace]() {
		return sstv::app::exportPreparedPng(*captured, path, replace);
	}));
}

void
TxEditorModel::acceptPngExport(const sstv::app::PngExportResult& result,
	const QUrl& url, const std::uint64_t revision)
{
	if (!generationGate.isCurrent(revision)) return;
	if (const auto* error = std::get_if<sstv::app::EditorError>(&result)) {
		if (error->code == sstv::app::EditorErrorCode::destinationExists) {
			setState(QStringLiteral("ready"), QStringLiteral("Overwrite confirmation required."));
			emit overwriteRequired(url, QStringLiteral("png"));
			return;
		}
		setError(formatError(*error));
		return;
	}
	setState(QStringLiteral("completed"), QStringLiteral("Prepared PNG exported atomically."));
}

void
TxEditorModel::exportWav(const QUrl& url, const bool replace)
{
	if (!snapshot) {
		setError(QStringLiteral("Prepare the current image before export."));
		return;
	}
	const auto path = localPath(url, QStringLiteral("WAV output"));
	if (!path) return;
	const auto captured = snapshot;
	const std::uint64_t revision = captured->revision;
	setState(QStringLiteral("exporting"), QStringLiteral("Rendering and publishing offline WAV..."));
	auto* watcher = new QFutureWatcher<sstv::app::WavEditorExportResult>(this);
	connect(watcher, &QFutureWatcher<sstv::app::WavEditorExportResult>::finished, this,
		[this, watcher, url, revision]() {
			const auto result = watcher->result();
			if (!generationGate.isCurrent(revision)) {
				watcher->deleteLater();
				return;
			}
			if (const auto* error = std::get_if<sstv::app::EditorError>(&result);
				error != nullptr && error->code == sstv::app::EditorErrorCode::destinationExists) {
				setState(QStringLiteral("ready"), QStringLiteral("Overwrite confirmation required."));
				emit overwriteRequired(url, QStringLiteral("wav"));
			} else {
				acceptWavExport(result, revision);
			}
			watcher->deleteLater();
		});
	watcher->setFuture(QtConcurrent::run(&threadPool, [captured, path = *path, replace]() {
		return sstv::app::exportOfflineWav(*captured, path, replace);
	}));
}

void
TxEditorModel::acceptWavExport(const sstv::app::WavEditorExportResult& result,
	const std::uint64_t revision)
{
	if (!generationGate.isCurrent(revision)) return;
	if (const auto* error = std::get_if<sstv::app::EditorError>(&result)) {
		setError(formatError(*error));
		return;
	}
	const auto& exported = std::get<sstv::app::WavExportResult>(result);
	currentInspection = formatInspection(exported.inspection);
	emit inspectionChanged();
	setState(QStringLiteral("completed"),
		QStringLiteral("Exported %1 | %2 frames | %3 | FSK ID %4")
			.arg(QString::fromStdString(exported.path.string()))
			.arg(exported.frameCount)
			.arg(formatDuration(exported.transmissionDuration))
			.arg(exported.fskIdentifier.has_value()
				? QString::fromStdString(*exported.fskIdentifier) : QStringLiteral("off")));
}

void
TxEditorModel::inspectWav(const QUrl& url)
{
	const auto path = localPath(url, QStringLiteral("WAV input"));
	if (!path) return;
	setState(QStringLiteral("loading"), QStringLiteral("Inspecting local PCM16 WAV..."));
	auto* watcher = new QFutureWatcher<sstv::app::InspectionResult>(this);
	connect(watcher, &QFutureWatcher<sstv::app::InspectionResult>::finished, this,
		[this, watcher]() {
			acceptInspection(watcher->result());
			watcher->deleteLater();
		});
	watcher->setFuture(QtConcurrent::run(&threadPool,
		[path = *path]() { return sstv::app::inspectOfflineWav(path); }));
}

void
TxEditorModel::acceptInspection(const sstv::app::InspectionResult& result)
{
	if (const auto* error = std::get_if<sstv::app::EditorError>(&result)) {
		setError(formatError(*error));
		return;
	}
	currentInspection = formatInspection(std::get<sstv::offline::WavInspection>(result));
	emit inspectionChanged();
	setState(QStringLiteral("completed"), QStringLiteral("WAV inspection completed."));
}
