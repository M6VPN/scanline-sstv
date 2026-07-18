// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/gui/live_transmit_model.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "live_transmit_model.hpp"

#include <QFutureWatcher>
#include <QVariantMap>
#include <QtConcurrentRun>

#include <algorithm>
#include <variant>

namespace {

[[nodiscard]] QString
serviceStateName(const sstv::app::LiveServiceState state)
{
	switch (state) {
	case sstv::app::LiveServiceState::idle: return QStringLiteral("idle");
	case sstv::app::LiveServiceState::ready: return QStringLiteral("ready");
	case sstv::app::LiveServiceState::confirmed: return QStringLiteral("confirmed");
	case sstv::app::LiveServiceState::checkingPtt: return QStringLiteral("checking PTT");
	case sstv::app::LiveServiceState::running: return QStringLiteral("running");
	case sstv::app::LiveServiceState::stopping: return QStringLiteral("stopping");
	case sstv::app::LiveServiceState::completed: return QStringLiteral("completed");
	case sstv::app::LiveServiceState::cancelled: return QStringLiteral("cancelled");
	case sstv::app::LiveServiceState::faulted: return QStringLiteral("faulted");
	case sstv::app::LiveServiceState::hazardous: return QStringLiteral("unresolved PTT hazard");
	}
	return QStringLiteral("unknown");
}

[[nodiscard]] QString
transmitStateName(const sstv::app::TransmitState state)
{
	switch (state) {
	case sstv::app::TransmitState::idle: return QStringLiteral("idle");
	case sstv::app::TransmitState::preparing: return QStringLiteral("preparing");
	case sstv::app::TransmitState::checkingPtt: return QStringLiteral("checking PTT");
	case sstv::app::TransmitState::openingAudio: return QStringLiteral("opening audio");
	case sstv::app::TransmitState::primingAudio: return QStringLiteral("priming audio");
	case sstv::app::TransmitState::armingWatchdog: return QStringLiteral("arming watchdog");
	case sstv::app::TransmitState::keying: return QStringLiteral("keying");
	case sstv::app::TransmitState::preKeyDelay: return QStringLiteral("pre-key delay");
	case sstv::app::TransmitState::transmitting: return QStringLiteral("transmitting");
	case sstv::app::TransmitState::draining: return QStringLiteral("draining");
	case sstv::app::TransmitState::postAudioTail: return QStringLiteral("post-audio tail");
	case sstv::app::TransmitState::unkeying: return QStringLiteral("unkeying");
	case sstv::app::TransmitState::completed: return QStringLiteral("completed");
	case sstv::app::TransmitState::faulting: return QStringLiteral("faulting");
	case sstv::app::TransmitState::faulted: return QStringLiteral("faulted");
	case sstv::app::TransmitState::cancelled: return QStringLiteral("cancelled");
	}
	return QStringLiteral("unknown");
}

[[nodiscard]] QString
certaintyName(const sstv::rig::PttCertainty certainty)
{
	switch (certainty) {
	case sstv::rig::PttCertainty::definitelyKeyed: return QStringLiteral("definitely keyed");
	case sstv::rig::PttCertainty::definitelyUnkeyed: return QStringLiteral("definitely unkeyed");
	case sstv::rig::PttCertainty::indeterminate: return QStringLiteral("indeterminate");
	}
	return QStringLiteral("unknown");
}

} // namespace

LiveTransmitModel::LiveTransmitModel(TxEditorModel* editor, QObject* parent)
	: LiveTransmitModel(editor,
		std::make_shared<sstv::app::LiveTransmitService>(), parent)
{
}

LiveTransmitModel::LiveTransmitModel(TxEditorModel* editor,
	std::shared_ptr<sstv::app::LiveTransmitService> transmitService, QObject* parent)
	: QObject(parent), editorModel(editor), service(std::move(transmitService))
{
	threadPool.setMaxThreadCount(2);
	for (const sstv::audio::AudioBackend backend : sstv::audio::defaultAudioBackends()) {
		QVariantMap value;
		value.insert(QStringLiteral("id"), QString::fromStdString(std::string(
			sstv::audio::audioBackendApiName(backend))));
		value.insert(QStringLiteral("name"), QString::fromStdString(std::string(
			sstv::audio::audioBackendDisplayName(backend))));
		backendValues.push_back(value);
	}
	connect(editorModel, &TxEditorModel::metadataChanged,
		this, &LiveTransmitModel::invalidate);
	snapshotTimer.setInterval(100);
	connect(&snapshotTimer, &QTimer::timeout,
		this, &LiveTransmitModel::pollSnapshot);
	snapshotTimer.start();
}

LiveTransmitModel::~LiveTransmitModel()
{
	snapshotTimer.stop();
	service->requestCancel();
	threadPool.waitForDone();
	static_cast<void>(service->shutdown());
}

QVariantList LiveTransmitModel::backends() const { return backendValues; }
QVariantList LiveTransmitModel::playbackDevices() const { return playbackValues; }
QString LiveTransmitModel::selectedPlaybackIdentity() const
{
	return retainedPlaybackIdentity;
}
QString LiveTransmitModel::state() const { return currentState; }
QString LiveTransmitModel::statusText() const { return currentStatus; }
QString LiveTransmitModel::primaryError() const { return currentPrimaryError; }
QString LiveTransmitModel::cleanupErrors() const { return currentCleanupErrors; }
QString LiveTransmitModel::pttCertainty() const { return currentPttCertainty; }
QString LiveTransmitModel::watchdogStatus() const { return currentWatchdogStatus; }
QString LiveTransmitModel::audioStatus() const { return currentAudioStatus; }
QString LiveTransmitModel::confirmationPhrase() const
{
	return QString::fromUtf8(sstv::app::liveTransmitConfirmationPhrase);
}
bool LiveTransmitModel::isReady() const
{
	return hasConfiguration && hasExactDevice && editorModel->isReady()
		&& !isActive() && !isHazardous();
}
bool LiveTransmitModel::isActive() const
{
	return currentState == QStringLiteral("checking PTT")
		|| currentState == QStringLiteral("opening audio")
		|| currentState == QStringLiteral("priming audio")
		|| currentState == QStringLiteral("arming watchdog")
		|| currentState == QStringLiteral("keying")
		|| currentState == QStringLiteral("pre-key delay")
		|| currentState == QStringLiteral("transmitting")
		|| currentState == QStringLiteral("draining")
		|| currentState == QStringLiteral("post-audio tail")
		|| currentState == QStringLiteral("unkeying")
		|| currentState == QStringLiteral("stopping");
}
bool LiveTransmitModel::isHazardous() const
{
	return currentState == QStringLiteral("unresolved PTT hazard");
}
double LiveTransmitModel::progress() const { return currentProgress; }
qulonglong LiveTransmitModel::revision() const
{
	return static_cast<qulonglong>(configuredRevision);
}

void
LiveTransmitModel::invalidate()
{
	++requestGeneration;
	hasConfiguration = false;
	configuredRevision = 0;
	currentStatus = QStringLiteral("Live readiness invalidated by editor changes.");
	emit readinessChanged();
	emit snapshotChanged();
}

std::optional<sstv::audio::AudioBackend>
LiveTransmitModel::parseBackend(const QString& value) const
{
	return sstv::audio::parseAudioBackend(value.toStdString());
}

std::optional<sstv::app::LivePttConfiguration>
LiveTransmitModel::parsePtt(const QString& provider, const QString& address,
	const int port, const QString& path) const
{
	if (port <= 0 || port > 65'535) return std::nullopt;
	sstv::app::LivePttConfiguration value;
	if (provider == QStringLiteral("flrig")) {
		value.provider = sstv::app::LivePttProviderKind::flrig;
		value.flrigPath = path.toStdString();
	} else if (provider == QStringLiteral("rigctld")) {
		value.provider = sstv::app::LivePttProviderKind::rigctld;
	} else {
		return std::nullopt;
	}
	value.address = address.toStdString();
	value.port = static_cast<std::uint16_t>(port);
	return value;
}

void
LiveTransmitModel::setError(const QString& error)
{
	currentPrimaryError = error;
	currentStatus = error;
	hasConfiguration = false;
	emit readinessChanged();
	emit snapshotChanged();
}

void
LiveTransmitModel::refreshDevices(const QString& backendName)
{
	const auto backend = parseBackend(backendName);
	if (!backend || !sstv::audio::isRealAudioBackend(*backend)) {
		setError(QStringLiteral("Select a compiled real audio backend."));
		return;
	}
	if (!editorModel->isReady()) {
		setError(QStringLiteral("Complete image preparation before audio discovery."));
		return;
	}
	const QString previousIdentity = retainedPlaybackIdentity;
	retainedPlaybackIdentity.clear();
	playbackValues.clear();
	hasExactDevice = false;
	hasConfiguration = false;
	++requestGeneration;
	const std::uint64_t generation = requestGeneration;
	emit devicesChanged();
	emit readinessChanged();
	auto* watcher = new QFutureWatcher<sstv::audio::DiscoveryResult>(this);
	connect(watcher, &QFutureWatcher<sstv::audio::DiscoveryResult>::finished,
		this, [this, watcher, generation, previousIdentity]() {
		acceptDiscovery(generation, watcher->result(), previousIdentity);
		watcher->deleteLater();
	});
	watcher->setFuture(QtConcurrent::run(&threadPool,
		[service = service, backend = *backend]() {
			return service->discover(backend);
		}));
}

void
LiveTransmitModel::acceptDiscovery(const std::uint64_t generation,
	const sstv::audio::DiscoveryResult& result, const QString& previousIdentity)
{
	if (generation != requestGeneration) return;
	if (const auto* error = std::get_if<sstv::audio::DiscoveryError>(&result)) {
		setError(QString::fromStdString(error->message));
		return;
	}
	const auto snapshot = std::get<std::shared_ptr<const sstv::audio::AudioDiscoverySnapshot>>(result);
	std::size_t previousMatches = 0;
	for (const auto& backend : snapshot->backends) {
		for (const auto& device : backend.devices) {
			if (device.identity.direction != sstv::audio::AudioDirection::playback) continue;
			QVariantMap value;
			value.insert(QStringLiteral("id"), QString::fromStdString(device.identity.opaque));
			value.insert(QStringLiteral("label"), QString::fromStdString(device.name));
			value.insert(QStringLiteral("collision"), device.hasIdentityCollision);
			playbackValues.push_back(value);
			if (!device.hasIdentityCollision
				&& QString::fromStdString(device.identity.opaque) == previousIdentity) {
				++previousMatches;
			}
		}
	}
	if (previousMatches == 1) retainedPlaybackIdentity = previousIdentity;
	currentStatus = QStringLiteral("Exact playback devices refreshed; select one identity.");
	emit devicesChanged();
	emit readinessChanged();
}

void
LiveTransmitModel::updateConfiguration(const QString& backendName,
	const QString& identity, const int outputChannel, const int playbackChannels,
	const double gain, const QString& provider, const QString& address,
	const int port, const QString& path, const int preKey, const int postAudio)
{
	++requestGeneration;
	hasConfiguration = false;
	const auto backend = parseBackend(backendName);
	const auto ptt = parsePtt(provider, address, port, path);
	const auto preparedSnapshot = editorModel->preparedSnapshot();
	if (!backend || !ptt || preparedSnapshot == nullptr
		|| preparedSnapshot->sampleRate != sstv::audio::nominalAudioSampleRate
		|| outputChannel < 0 || playbackChannels <= 0
		|| preKey < 0 || postAudio < 0) {
		setError(QStringLiteral("Live configuration is incomplete or invalid; use a 48000 Hz prepared snapshot."));
		return;
	}
	auto preparedResult = sstv::app::prepareLiveTransmitSnapshot(preparedSnapshot, gain);
	if (const auto* error = std::get_if<sstv::app::LiveTransmitError>(&preparedResult)) {
		setError(QString::fromStdString(error->operation + ": " + error->message));
		return;
	}
	std::size_t identityMatches = 0;
	bool hasCollision = false;
	for (const QVariant& item : playbackValues) {
		const QVariantMap value = item.toMap();
		if (value.value(QStringLiteral("id")).toString() != identity) continue;
		++identityMatches;
		hasCollision = hasCollision || value.value(QStringLiteral("collision")).toBool();
	}
	hasExactDevice = identityMatches == 1 && !hasCollision && !identity.isEmpty();
	if (!hasExactDevice) {
		setError(QStringLiteral("Select one fresh, non-colliding exact playback identity."));
		return;
	}
	sstv::app::LiveTransmitConfiguration configuration;
	configuration.revision = preparedSnapshot->revision;
	configuration.playback = {*backend, identity.toStdString(),
		static_cast<std::uint32_t>(playbackChannels),
		static_cast<std::uint32_t>(outputChannel)};
	configuration.ptt = *ptt;
	configuration.preKeyDelay = std::chrono::milliseconds(preKey);
	configuration.postAudioTail = std::chrono::milliseconds(postAudio);
	const auto error = service->configure(
		std::get<sstv::app::LiveTransmitPrepared>(preparedResult), configuration);
	if (error) {
		setError(QString::fromStdString(error->operation + ": " + error->message));
		return;
	}
	retainedPlaybackIdentity = identity;
	configuredRevision = preparedSnapshot->revision;
	hasConfiguration = true;
	currentPrimaryError.clear();
	currentStatus = QStringLiteral("Live configuration is ready for a fresh safety review.");
	emit readinessChanged();
	emit snapshotChanged();
}

void
LiveTransmitModel::confirmAndTransmit(const bool audioArm, const bool pttArm,
	const bool liveArm, const QString& phrase)
{
	if (!isReady()) {
		setError(QStringLiteral("Live transmission is not ready."));
		return;
	}
	auto confirmation = service->confirm({configuredRevision, audioArm, pttArm,
		liveArm, phrase.toStdString()});
	if (const auto* error = std::get_if<sstv::app::LiveTransmitError>(&confirmation)) {
		setError(QString::fromStdString(error->operation + ": " + error->message));
		return;
	}
	if (const auto error = service->start(
		std::get<sstv::app::LiveTransmitConfirmation>(confirmation))) {
		setError(QString::fromStdString(error->operation + ": " + error->message));
		return;
	}
	hasConfiguration = false;
	currentState = QStringLiteral("checking PTT");
	currentStatus = QStringLiteral("Confirmation consumed; checking exact PTT state.");
	emit readinessChanged();
	emit snapshotChanged();
}

void
LiveTransmitModel::checkPttState(const QString& provider, const QString& address,
	const int port, const QString& path)
{
	const auto configuration = parsePtt(provider, address, port, path);
	if (!configuration) {
		setError(QStringLiteral("PTT configuration is invalid."));
		return;
	}
	const std::uint64_t generation = ++requestGeneration;
	auto* watcher = new QFutureWatcher<sstv::app::LivePttQueryResult>(this);
	connect(watcher, &QFutureWatcher<sstv::app::LivePttQueryResult>::finished,
		this, [this, watcher, generation]() {
		if (generation == requestGeneration) {
			const auto result = watcher->result();
			if (const auto* error = std::get_if<sstv::app::LiveTransmitError>(&result)) {
				setError(QString::fromStdString(error->operation + ": " + error->message));
			} else {
				const auto value = std::get<sstv::rig::PttOperationResult>(result);
				currentPttCertainty = certaintyName(value.certainty);
				currentStatus = QStringLiteral("PTT query: ") + currentPttCertainty;
				emit snapshotChanged();
			}
		}
		watcher->deleteLater();
	});
	watcher->setFuture(QtConcurrent::run(&threadPool,
		[service = service, configuration = *configuration]() {
			return service->checkPtt(configuration);
		}));
}

void LiveTransmitModel::stop() { service->requestCancel(); }

void
LiveTransmitModel::retryUnkey()
{
	if (!isHazardous()) return;
	const std::uint64_t generation = ++requestGeneration;
	auto* watcher = new QFutureWatcher<sstv::rig::PttCleanupResult>(this);
	connect(watcher, &QFutureWatcher<sstv::rig::PttCleanupResult>::finished,
		this, [this, watcher, generation]() {
		if (generation == requestGeneration) pollSnapshot();
		watcher->deleteLater();
	});
	watcher->setFuture(QtConcurrent::run(&threadPool,
		[service = service]() { return service->retryUnkey(); }));
}

bool
LiveTransmitModel::requestWindowClose()
{
	if (!isActive() && !isHazardous()) return true;
	closePending = true;
	service->requestCancel();
	return false;
}

bool
LiveTransmitModel::isTerminal() const
{
	return currentState == QStringLiteral("completed")
		|| currentState == QStringLiteral("cancelled")
		|| currentState == QStringLiteral("faulted")
		|| currentState == QStringLiteral("unresolved PTT hazard");
}

void
LiveTransmitModel::pollSnapshot()
{
	const auto value = service->snapshot();
	if (value->sequence == lastSnapshotSequence && !isActive()) return;
	lastSnapshotSequence = value->sequence;
	currentState = serviceStateName(value->state);
	currentPrimaryError = QString::fromStdString(value->primaryError);
	currentCleanupErrors.clear();
	for (const std::string& error : value->cleanupErrors) {
		if (!currentCleanupErrors.isEmpty()) currentCleanupErrors.append('\n');
		currentCleanupErrors.append(QString::fromStdString(error));
	}
	currentProgress = 0.0;
	if (value->session) {
		currentState = transmitStateName(value->session->state);
		currentPttCertainty = certaintyName(value->session->ptt.certainty);
		currentWatchdogStatus = value->session->ptt.watchdogExpired
			? QStringLiteral("expired") : QStringLiteral("no expiry recorded");
		if (value->session->error == sstv::app::TransmitErrorCode::disconnect) {
			currentAudioStatus = QStringLiteral("disconnected or removed");
		} else if (value->session->audioCloseWasAttempted) {
			currentAudioStatus = QStringLiteral("close attempted");
		} else if (value->session->negotiated.has_value()) {
			currentAudioStatus = QStringLiteral("exact stream negotiated");
		} else {
			currentAudioStatus = QStringLiteral("not opened");
		}
		if (value->session->sourceFrames != 0) {
			currentProgress = std::min(1.0,
				static_cast<double>(value->session->submittedFrames)
				/ static_cast<double>(value->session->sourceFrames));
			const double elapsed = static_cast<double>(value->session->submittedFrames)
				/ static_cast<double>(sstv::audio::nominalAudioSampleRate);
			const double remaining = static_cast<double>(
				value->session->sourceFrames - std::min(value->session->sourceFrames,
					value->session->submittedFrames))
				/ static_cast<double>(sstv::audio::nominalAudioSampleRate);
			currentStatus = QStringLiteral("%1 / %2 source frames | elapsed %3 s | remaining %4 s")
				.arg(value->session->submittedFrames).arg(value->session->sourceFrames)
				.arg(elapsed, 0, 'f', 3).arg(remaining, 0, 'f', 3);
		}
		if (value->session->ptt.hasHazard) {
			currentState = QStringLiteral("unresolved PTT hazard");
		}
	}
	if (!currentPrimaryError.isEmpty()) currentStatus = currentPrimaryError;
	else if (!value->session || value->session->sourceFrames == 0) {
		currentStatus = QStringLiteral("Live state: ") + currentState;
	}
	emit snapshotChanged();
	emit readinessChanged();
	if (closePending && isTerminal()) {
		closePending = false;
		emit safeToClose();
	}
}
