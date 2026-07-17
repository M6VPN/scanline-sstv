// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/gui/tx_editor_model_tests.cpp
// SPDX-License-Identifier: GPL-3.0-or-later

#include "prepared_image_provider.hpp"
#include "tx_editor_model.hpp"

#include <QFile>
#include <QFileInfo>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TxEditorModelTests final : public QObject {
	Q_OBJECT

private slots:
	void listsRegistryModesAndRates();
	void rejectsNonLocalInput();
	void preparesAndInvalidatesPreview();
	void ignoresStaleCompletion();
	void validatesFskIdentifier();
	void confirmsReplacementAndInspectsExport();
};

void
TxEditorModelTests::listsRegistryModesAndRates()
{
	PreparedImageProvider provider;
	TxEditorModel model(&provider);
	QCOMPARE(model.state(), QStringLiteral("initial"));
	QCOMPARE(model.modeCount(), 4);
	QCOMPARE(model.sampleRates().size(), 11);
	QVERIFY(!model.isReady());
	QVERIFY(!model.isBusy());
}

void
TxEditorModelTests::rejectsNonLocalInput()
{
	PreparedImageProvider provider;
	TxEditorModel model(&provider);
	model.selectInput(QUrl(QStringLiteral("https://example.invalid/image.png")));
	QCOMPARE(model.state(), QStringLiteral("error"));
	QVERIFY(model.errorText().contains(QStringLiteral("local filesystem")));
	QVERIFY(!model.isReady());
}

void
TxEditorModelTests::preparesAndInvalidatesPreview()
{
	PreparedImageProvider provider;
	TxEditorModel model(&provider);
	model.selectInput(QUrl::fromLocalFile(
		QStringLiteral(SSTV_IMAGE_FIXTURE_DIR "/marker.png")));
	model.updateRequest(QStringLiteral("martin-m1"), QStringLiteral("contain"),
		QStringLiteral("000000"), false, 0, 0, 1, 1, 48'000, false, {});
	QCOMPARE(model.state(), QStringLiteral("loading"));
	QVERIFY(!model.isReady());
	QTRY_COMPARE_WITH_TIMEOUT(model.state(), QStringLiteral("ready"), 20'000);
	QVERIFY(model.isReady());
	QVERIFY(model.previewSource().startsWith(QStringLiteral("image://prepared/")));
	QCOMPARE(model.sourceDimensions(), QStringLiteral("5 x 3"));
	QVERIFY(model.modeMetadata().contains(QStringLiteral("320 x 256")));
	const QString firstPreview = model.previewSource();
	model.updateRequest(QStringLiteral("martin-m1"), QStringLiteral("cover"),
		QStringLiteral("112233"), false, 0, 0, 1, 1, 48'000, false, {});
	QCOMPARE(model.state(), QStringLiteral("loading"));
	QVERIFY(!model.isReady());
	QVERIFY(model.previewSource().isEmpty());
	QTRY_COMPARE_WITH_TIMEOUT(model.state(), QStringLiteral("ready"), 20'000);
	QVERIFY(model.previewSource() != firstPreview);
}

void
TxEditorModelTests::ignoresStaleCompletion()
{
	PreparedImageProvider provider;
	TxEditorModel model(&provider);
	model.selectInput(QUrl::fromLocalFile(
		QStringLiteral(SSTV_IMAGE_FIXTURE_DIR "/marker.png")));
	model.updateRequest(QStringLiteral("pd-120"), QStringLiteral("contain"),
		QStringLiteral("000000"), false, 0, 0, 1, 1, 48'000, false, {});
	model.updateRequest(QStringLiteral("robot-36"), QStringLiteral("contain"),
		QStringLiteral("000000"), false, 0, 0, 1, 1, 48'000, false, {});
	QTRY_COMPARE_WITH_TIMEOUT(model.state(), QStringLiteral("ready"), 20'000);
	QVERIFY(model.modeMetadata().contains(QStringLiteral("320 x 240")));
}

void
TxEditorModelTests::validatesFskIdentifier()
{
	PreparedImageProvider provider;
	TxEditorModel model(&provider);
	model.selectInput(QUrl::fromLocalFile(
		QStringLiteral(SSTV_IMAGE_FIXTURE_DIR "/marker.png")));
	model.updateRequest(QStringLiteral("martin-m1"), QStringLiteral("contain"),
		QStringLiteral("000000"), false, 0, 0, 1, 1, 48'000, true,
		QStringLiteral("BAD`ID"));
	QCOMPARE(model.state(), QStringLiteral("error"));
	QVERIFY(!model.errorText().isEmpty());
	model.updateRequest(QStringLiteral("martin-m1"), QStringLiteral("contain"),
		QStringLiteral("000000"), false, 0, 0, 1, 1, 48'000, true,
		QStringLiteral("m6vpn"));
	QTRY_COMPARE_WITH_TIMEOUT(model.state(), QStringLiteral("ready"), 20'000);
	QVERIFY(model.durationMetadata().contains(QStringLiteral("Combined")));
}

void
TxEditorModelTests::confirmsReplacementAndInspectsExport()
{
	QTemporaryDir directory;
	QVERIFY(directory.isValid());
	PreparedImageProvider provider;
	TxEditorModel model(&provider);
	model.selectInput(QUrl::fromLocalFile(
		QStringLiteral(SSTV_IMAGE_FIXTURE_DIR "/marker.png")));
	model.updateRequest(QStringLiteral("robot-36"), QStringLiteral("contain"),
		QStringLiteral("000000"), false, 0, 0, 1, 1, 8'000, false, {});
	QTRY_COMPARE_WITH_TIMEOUT(model.state(), QStringLiteral("ready"), 20'000);
	const QString pngPath = directory.filePath(QStringLiteral("prepared.png"));
	QFile existing(pngPath);
	QVERIFY(existing.open(QIODevice::WriteOnly));
	QVERIFY(existing.write("existing") == 8);
	existing.close();
	QSignalSpy confirmation(&model, &TxEditorModel::overwriteRequired);
	model.exportPng(QUrl::fromLocalFile(pngPath));
	QTRY_COMPARE_WITH_TIMEOUT(confirmation.count(), 1, 10'000);
	QCOMPARE(model.state(), QStringLiteral("ready"));
	model.exportPng(QUrl::fromLocalFile(pngPath), true);
	QTRY_COMPARE_WITH_TIMEOUT(model.state(), QStringLiteral("completed"), 10'000);
	QVERIFY(QFileInfo(pngPath).size() > 8);
	const QString wavPath = directory.filePath(QStringLiteral("offline.wav"));
	model.exportWav(QUrl::fromLocalFile(wavPath));
	QTRY_COMPARE_WITH_TIMEOUT(model.state(), QStringLiteral("completed"), 30'000);
	QVERIFY(QFileInfo(wavPath).size() > 44);
	QVERIFY(model.inspectionText().contains(QStringLiteral("Sample rate: 8000 Hz")));
	QVERIFY(model.statusText().contains(wavPath));
	QVERIFY(model.statusText().contains(QStringLiteral("FSK ID off")));
	model.inspectWav(QUrl::fromLocalFile(wavPath));
	QTRY_COMPARE_WITH_TIMEOUT(model.state(), QStringLiteral("completed"), 10'000);
	QVERIFY(model.inspectionText().contains(QStringLiteral("RIFF/WAVE")));
}

QTEST_MAIN(TxEditorModelTests)

#include "tx_editor_model_tests.moc"
