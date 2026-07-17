// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/gui/audio_diagnostics_model.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio_diagnostics_model.hpp"

#include <QFutureWatcher>
#include <QMetaObject>
#include <QVariantMap>
#include <QtConcurrentRun>

#include <iomanip>
#include <locale>
#include <sstream>
#include <variant>

namespace {

[[nodiscard]] QString
stateName(const sstv::audio::DiagnosticState state)
{
	using sstv::audio::DiagnosticState;
	switch (state) {
	case DiagnosticState::idle: return QStringLiteral("idle");
	case DiagnosticState::validating: return QStringLiteral("validating");
	case DiagnosticState::opening: return QStringLiteral("opening");
	case DiagnosticState::priming: return QStringLiteral("priming");
	case DiagnosticState::running: return QStringLiteral("running");
	case DiagnosticState::stopping: return QStringLiteral("stopping");
	case DiagnosticState::analysing: return QStringLiteral("analysing");
	case DiagnosticState::completed: return QStringLiteral("completed");
	case DiagnosticState::cancelled: return QStringLiteral("cancelled");
	case DiagnosticState::faulted: return QStringLiteral("faulted");
	}
	return QStringLiteral("faulted");
}

[[nodiscard]] QString
formatSnapshot(const sstv::audio::DiagnosticSnapshot& snapshot)
{
	std::ostringstream output;
	output.imbue(std::locale::classic());
	output << "Progress: " << snapshot.processedFrames << " / "
	    << snapshot.totalFrames << " frames\n"
	    << "Elapsed / remaining: " << snapshot.elapsedMs << " / "
	    << snapshot.remainingMs << " ms\n";
	if (snapshot.playbackIdentity) {
		output << "Playback identity / channel: "
		    << snapshot.playbackIdentity->opaque << " / "
		    << *snapshot.playbackChannel << '\n';
	}
	if (snapshot.captureIdentity) {
		output << "Capture identity / channel: "
		    << snapshot.captureIdentity->opaque << " / "
		    << *snapshot.captureChannel << '\n';
	}
	if (snapshot.negotiated) {
		output << "Negotiated: " << snapshot.negotiated->callbackSampleRate
		    << " Hz, " << snapshot.negotiated->periodFrames << " frames x "
		    << snapshot.negotiated->periodCount << '\n';
	}
	if (snapshot.level) {
		output << std::fixed << std::setprecision(3)
		    << "Peak: " << snapshot.level->peakDbfs << " dBFS\n"
		    << "RMS: " << snapshot.level->rmsDbfs << " dBFS\n"
		    << "DC: " << snapshot.level->dcMean << '\n'
		    << "Clipped + / -: " << snapshot.level->clippedPositive << " / "
		    << snapshot.level->clippedNegative << '\n';
	}
	if (snapshot.loopback) {
		output << std::fixed << std::setprecision(3)
		    << "Correlation: " << snapshot.loopback->correlation << '\n'
		    << "Latency: " << snapshot.loopback->latencyFrames << " frames, "
		    << snapshot.loopback->latencyMilliseconds << " ms\n"
		    << "Gain: " << snapshot.loopback->gainDb << " dB\n"
		    << "Polarity inverted: "
		    << (snapshot.loopback->hasPolarityInversion ? "yes" : "no") << '\n';
	}
	output << "Underrun / dropped: " << snapshot.stream.underrunFrames << " / "
	    << snapshot.stream.captureFramesDropped;
	return QString::fromStdString(output.str());
}

} // namespace

AudioDiagnosticsModel::AudioDiagnosticsModel(QObject* parent)
	: AudioDiagnosticsModel(sstv::audio::createMiniaudioDiscoveryProvider(),
	    [] { return sstv::audio::createMiniaudioStreamAdapter(); }, parent)
{
}

AudioDiagnosticsModel::AudioDiagnosticsModel(
	std::shared_ptr<sstv::audio::AudioDiscoveryProvider> provider,
	sstv::audio::AudioStreamAdapterFactory factory, QObject* parent)
	: QObject(parent), service(std::make_shared<sstv::audio::AudioDiagnosticsService>(
	    std::move(provider), std::move(factory)))
{
	threadPool.setMaxThreadCount(1);
	for (const sstv::audio::AudioBackend backend : sstv::audio::defaultAudioBackends()) {
		QVariantMap value;
		value.insert(QStringLiteral("id"), QString::fromUtf8(
		    sstv::audio::audioBackendApiName(backend).data()));
		value.insert(QStringLiteral("name"), QString::fromUtf8(
		    sstv::audio::audioBackendDisplayName(backend).data()));
		backendValues.push_back(value);
	}
}

AudioDiagnosticsModel::~AudioDiagnosticsModel()
{
	service->requestStop();
	threadPool.waitForDone();
}

QVariantList AudioDiagnosticsModel::backends() const { return backendValues; }
QVariantList AudioDiagnosticsModel::playbackDevices() const { return playbackValues; }
QVariantList AudioDiagnosticsModel::captureDevices() const { return captureValues; }
QString AudioDiagnosticsModel::state() const { return currentState; }
QString AudioDiagnosticsModel::statusText() const { return currentStatus; }
QString AudioDiagnosticsModel::resultText() const { return currentResult; }
bool AudioDiagnosticsModel::isRunning() const { return isBusy; }
bool AudioDiagnosticsModel::hasDevices() const
{
	return !playbackValues.empty() || !captureValues.empty();
}
double AudioDiagnosticsModel::peakDbfs() const { return currentPeakDbfs; }

std::optional<sstv::audio::AudioBackend>
AudioDiagnosticsModel::parseBackend(const QString& value) const
{
	return sstv::audio::parseAudioBackend(value.toStdString());
}

std::optional<sstv::audio::DeviceIdentity>
AudioDiagnosticsModel::identity(const QString& opaque,
	const sstv::audio::AudioDirection direction) const
{
	if (!discovery || opaque.isEmpty()) return std::nullopt;
	for (const auto& backend : discovery->backends) {
		for (const auto& device : backend.devices) {
			if (device.identity.direction == direction
			    && QString::fromStdString(device.identity.opaque) == opaque
			    && !device.hasIdentityCollision) {
				return device.identity;
			}
		}
	}
	return std::nullopt;
}

void
AudioDiagnosticsModel::refreshDevices(const QString& backendText)
{
	if (isBusy) return;
	const auto backend = parseBackend(backendText);
	playbackValues.clear();
	captureValues.clear();
	discovery.reset();
	emit devicesChanged();
	if (!backend) {
		currentState = QStringLiteral("faulted");
		currentStatus = QStringLiteral("Select a valid backend.");
		emit snapshotChanged();
		return;
	}
	currentState = QStringLiteral("validating");
	currentStatus = QStringLiteral("Refreshing exact device identities...");
	isBusy = true;
	const std::uint64_t revision = ++requestRevision;
	emit snapshotChanged();
	auto* watcher = new QFutureWatcher<sstv::audio::DiagnosticDiscoveryResult>(this);
	connect(watcher, &QFutureWatcher<sstv::audio::DiagnosticDiscoveryResult>::finished,
	    this, [this, watcher, revision]() {
		const auto result = watcher->result();
		watcher->deleteLater();
		if (revision != requestRevision) return;
		isBusy = false;
		if (const auto* error = std::get_if<sstv::audio::DiagnosticError>(&result)) {
			currentState = QStringLiteral("faulted");
			currentStatus = QString::fromStdString(error->message);
			emit snapshotChanged();
			return;
		}
		discovery = std::get<std::shared_ptr<const sstv::audio::AudioDiscoverySnapshot>>(result);
		for (const auto& discoveredBackend : discovery->backends) {
			for (const auto& device : discoveredBackend.devices) {
				QVariantMap value;
				value.insert(QStringLiteral("id"), QString::fromStdString(device.identity.opaque));
				value.insert(QStringLiteral("name"), QString::fromStdString(device.name));
				value.insert(QStringLiteral("label"), QString::fromStdString(device.name)
				    + QStringLiteral(" [") + QString::fromStdString(device.identity.opaque)
				    + QStringLiteral("]"));
				if (device.identity.direction == sstv::audio::AudioDirection::playback) {
					playbackValues.push_back(value);
				} else {
					captureValues.push_back(value);
				}
			}
		}
		currentState = QStringLiteral("idle");
		currentStatus = QStringLiteral("Device refresh complete; no replacement was selected.");
		emit devicesChanged();
		emit snapshotChanged();
	});
	watcher->setFuture(QtConcurrent::run(&threadPool,
	    [service = service, backend = *backend] { return service->refresh(backend); }));
}

void
AudioDiagnosticsModel::start(sstv::audio::DiagnosticRequest request)
{
	if (isBusy) return;
	if (!request.playbackIdentity && request.operation != sstv::audio::DiagnosticOperation::inputMeter) {
		currentState = QStringLiteral("faulted");
		currentStatus = QStringLiteral("Select an exact playback device.");
		emit snapshotChanged();
		return;
	}
	if (!request.captureIdentity && request.operation != sstv::audio::DiagnosticOperation::outputCalibration) {
		currentState = QStringLiteral("faulted");
		currentStatus = QStringLiteral("Select an exact capture device.");
		emit snapshotChanged();
		return;
	}
	currentResult.clear();
	currentState = QStringLiteral("validating");
	currentStatus = QStringLiteral("Validating armed audio diagnostic...");
	isBusy = true;
	const std::uint64_t revision = ++requestRevision;
	emit snapshotChanged();
	auto* watcher = new QFutureWatcher<sstv::audio::DiagnosticResult>(this);
	connect(watcher, &QFutureWatcher<sstv::audio::DiagnosticResult>::finished,
	    this, [this, watcher, revision]() {
		acceptResult(revision, watcher->result());
		watcher->deleteLater();
	});
	watcher->setFuture(QtConcurrent::run(&threadPool,
	    [this, revision, request = std::move(request)] {
		return service->run(request, {}, [this, revision](const auto& snapshot) {
			QMetaObject::invokeMethod(this,
			    [this, revision, snapshot] {
				    acceptSnapshot(revision, snapshot);
			    }, Qt::QueuedConnection);
		});
	}));
}

void
AudioDiagnosticsModel::acceptSnapshot(const std::uint64_t revision,
	std::shared_ptr<const sstv::audio::DiagnosticSnapshot> snapshot)
{
	if (revision != requestRevision) return;
	currentState = stateName(snapshot->state);
	currentPeakDbfs = snapshot->level ? snapshot->level->peakDbfs
	    : sstv::audio::silenceDbfsFloor;
	currentStatus = snapshot->message.empty()
	    ? QStringLiteral("Audio diagnostic %1.").arg(currentState)
	    : QString::fromStdString(snapshot->message);
	currentResult = formatSnapshot(*snapshot);
	emit snapshotChanged();
}

void
AudioDiagnosticsModel::acceptResult(const std::uint64_t revision,
	const sstv::audio::DiagnosticResult& result)
{
	if (revision != requestRevision) return;
	isBusy = false;
	if (const auto* error = std::get_if<sstv::audio::DiagnosticError>(&result)) {
		currentState = stateName(error->state);
		currentStatus = QString::fromStdString(error->operation + ": " + error->message);
	} else {
		const auto& snapshot = std::get<sstv::audio::DiagnosticSnapshot>(result);
		currentState = stateName(snapshot.state);
		currentStatus = QStringLiteral("Audio diagnostic completed and stream closed.");
		currentResult = formatSnapshot(snapshot);
		currentPeakDbfs = snapshot.level ? snapshot.level->peakDbfs
		    : sstv::audio::silenceDbfsFloor;
	}
	++requestRevision;
	emit snapshotChanged();
}

void
AudioDiagnosticsModel::startMeter(const QString& backendText,
	const QString& captureId, const int channel, const int channels,
	const int duration, const int periodFrames, const int periodCount)
{
	const auto backend = parseBackend(backendText);
	if (!backend) return;
	sstv::audio::DiagnosticRequest request;
	request.operation = sstv::audio::DiagnosticOperation::inputMeter;
	request.backend = *backend;
	request.captureIdentity = identity(captureId, sstv::audio::AudioDirection::capture);
	request.expectedDiscoveryGeneration = discovery ? std::optional(discovery->generation) : std::nullopt;
	request.captureChannel = static_cast<std::uint32_t>(channel);
	request.captureChannels = static_cast<std::uint32_t>(channels);
	request.durationMs = static_cast<std::uint32_t>(duration * 1'000);
	request.periodFrames = static_cast<std::uint32_t>(periodFrames);
	request.periodCount = static_cast<std::uint32_t>(periodCount);
	start(std::move(request));
}

void
AudioDiagnosticsModel::startOutput(const QString& backendText,
	const QString& playbackId, const int channel, const int channels,
	const int duration, const int periodFrames, const int periodCount,
	const double level, const bool armed)
{
	const auto backend = parseBackend(backendText);
	if (!backend) return;
	sstv::audio::DiagnosticRequest request;
	request.operation = sstv::audio::DiagnosticOperation::outputCalibration;
	request.backend = *backend;
	request.playbackIdentity = identity(playbackId, sstv::audio::AudioDirection::playback);
	request.expectedDiscoveryGeneration = discovery ? std::optional(discovery->generation) : std::nullopt;
	request.playbackChannel = static_cast<std::uint32_t>(channel);
	request.playbackChannels = static_cast<std::uint32_t>(channels);
	request.durationMs = static_cast<std::uint32_t>(duration * 1'000);
	request.periodFrames = static_cast<std::uint32_t>(periodFrames);
	request.periodCount = static_cast<std::uint32_t>(periodCount);
	request.levelDbfs = level;
	request.isRealAudioArmed = armed;
	start(std::move(request));
}

void
AudioDiagnosticsModel::startLoopback(const QString& backendText,
	const QString& playbackId, const QString& captureId, const int outputChannel,
	const int inputChannel, const int playbackChannels, const int captureChannels,
	const int periodFrames, const int periodCount, const double level, const bool armed)
{
	const auto backend = parseBackend(backendText);
	if (!backend) return;
	sstv::audio::DiagnosticRequest request;
	request.operation = sstv::audio::DiagnosticOperation::loopback;
	request.backend = *backend;
	request.playbackIdentity = identity(playbackId, sstv::audio::AudioDirection::playback);
	request.captureIdentity = identity(captureId, sstv::audio::AudioDirection::capture);
	request.expectedDiscoveryGeneration = discovery ? std::optional(discovery->generation) : std::nullopt;
	request.playbackChannel = static_cast<std::uint32_t>(outputChannel);
	request.captureChannel = static_cast<std::uint32_t>(inputChannel);
	request.playbackChannels = static_cast<std::uint32_t>(playbackChannels);
	request.captureChannels = static_cast<std::uint32_t>(captureChannels);
	request.periodFrames = static_cast<std::uint32_t>(periodFrames);
	request.periodCount = static_cast<std::uint32_t>(periodCount);
	request.levelDbfs = level;
	request.isRealAudioArmed = armed;
	start(std::move(request));
}

void
AudioDiagnosticsModel::stop()
{
	if (!isBusy) return;
	service->requestStop();
	currentStatus = QStringLiteral("Emergency stop requested; waiting for stream cleanup.");
	emit snapshotChanged();
}
