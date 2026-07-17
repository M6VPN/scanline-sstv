// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/gui/Main.qml
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

ApplicationWindow {
	id: window
	width: 1200
	height: 760
	minimumWidth: 860
	minimumHeight: 560
	visible: true
	title: qsTr("Scanline SSTV - Offline TX Editor")

	property url pendingOverwriteUrl
	property string pendingOverwriteKind

	function prepareCurrent() {
		txEditorModel.updateRequest(
			modeBox.currentValue, fitBox.currentValue, backgroundField.text,
			cropEnabled.checked, cropX.value, cropY.value, cropWidth.value,
			cropHeight.value, Number(sampleRateBox.currentText), fskEnabled.checked,
			fskField.text)
	}

	header: ToolBar {
		RowLayout {
			anchors.fill: parent
			anchors.leftMargin: 14
			anchors.rightMargin: 14
			Label {
				text: qsTr("Scanline SSTV")
				font.bold: true
				font.pixelSize: 18
			}
			Label {
				text: qsTr("Offline TX editor")
				opacity: 0.72
			}
			Item { Layout.fillWidth: true }
			Label {
				text: txEditorModel.state.toUpperCase()
				font.bold: true
			}
		}
	}

	SplitView {
		objectName: "txEditorWorkspace"
		anchors.fill: parent
		orientation: Qt.Horizontal

		Pane {
			SplitView.fillWidth: true
			SplitView.minimumWidth: 500
			ColumnLayout {
				anchors.fill: parent
				spacing: 10
				Label {
					text: qsTr("Prepared mode image")
					font.bold: true
					font.pixelSize: 16
				}
				Rectangle {
					Layout.fillWidth: true
					Layout.fillHeight: true
					color: "#171a1f"
					border.color: "#4b5563"
					Image {
						id: previewImage
						anchors.fill: parent
						anchors.margins: 12
						source: txEditorModel.previewSource
						fillMode: Image.PreserveAspectFit
						cache: false
						smooth: false
						mipmap: false
						Accessible.name: qsTr("Exact prepared SSTV image preview")
					}
					Label {
						anchors.centerIn: parent
						visible: previewImage.source.toString() === ""
						text: txEditorModel.busy ? qsTr("Preparing preview...")
							: qsTr("Choose a JPEG or PNG image")
						color: "#d1d5db"
					}
				}
				GridLayout {
					Layout.fillWidth: true
					columns: 2
					columnSpacing: 16
					Label { text: qsTr("Source") }
					Label { text: txEditorModel.sourceName; Layout.fillWidth: true; elide: Text.ElideMiddle }
					Label { text: qsTr("Source size") }
					Label { text: txEditorModel.sourceDimensions }
					Label { text: qsTr("Oriented size") }
					Label { text: txEditorModel.orientedDimensions }
					Label { text: qsTr("Mode") }
					Label { text: txEditorModel.modeMetadata; wrapMode: Text.Wrap }
					Label { text: qsTr("Duration") }
					Label { text: txEditorModel.durationMetadata }
					Label { text: qsTr("WAV projection") }
					Label { text: txEditorModel.frameMetadata; wrapMode: Text.Wrap }
				}
			}
		}

		Pane {
			SplitView.preferredWidth: 390
			SplitView.minimumWidth: 340
			ScrollView {
				anchors.fill: parent
				contentWidth: availableWidth
				ColumnLayout {
					width: parent.width
					spacing: 10
					Label { text: qsTr("Preparation recipe"); font.bold: true; font.pixelSize: 16 }
					Label { text: qsTr("Mode"); Accessible.description: qsTr("Offline analogue SSTV mode") }
					ComboBox {
						id: modeBox
						Layout.fillWidth: true
						model: txEditorModel.modes
						textRole: "displayName"
						valueRole: "id"
						activeFocusOnTab: true
						Accessible.name: qsTr("SSTV mode")
						onActivated: if (txEditorModel.sourceName !== "No image selected") window.prepareCurrent()
					}
					Button {
						text: qsTr("Choose image...")
						Layout.fillWidth: true
						activeFocusOnTab: true
						Accessible.name: qsTr("Choose local image")
						onClicked: sourceDialog.open()
					}
					RowLayout {
						Layout.fillWidth: true
						Label { text: qsTr("Fit"); Layout.preferredWidth: 90 }
						ComboBox {
							id: fitBox
							Layout.fillWidth: true
							model: [{ text: qsTr("Contain"), value: "contain" },
								{ text: qsTr("Cover"), value: "cover" }]
							textRole: "text"
							valueRole: "value"
							Accessible.name: qsTr("Image fit mode")
							onActivated: window.prepareCurrent()
						}
					}
					RowLayout {
						Layout.fillWidth: true
						Label { text: qsTr("Background"); Layout.preferredWidth: 90 }
						TextField {
							id: backgroundField
							Layout.fillWidth: true
							text: "000000"
							maximumLength: 6
							validator: RegularExpressionValidator { regularExpression: /[0-9A-Fa-f]{6}/ }
							placeholderText: "RRGGBB"
							Accessible.name: qsTr("Background RGB hexadecimal value")
							onEditingFinished: window.prepareCurrent()
						}
						Rectangle {
							Layout.preferredWidth: 30
							Layout.preferredHeight: 30
							color: backgroundField.acceptableInput
								? "#" + backgroundField.text : "#000000"
							border.color: "#737b86"
							Accessible.name: qsTr("Background colour swatch")
						}
					}
					CheckBox {
						id: cropEnabled
						text: qsTr("Crop oriented source")
						Accessible.description: qsTr("Enable source-pixel crop coordinates after EXIF orientation")
						onToggled: window.prepareCurrent()
					}
					GridLayout {
						Layout.fillWidth: true
						columns: 4
						enabled: cropEnabled.checked
						Label { text: "X" }
						SpinBox { id: cropX; from: 0; to: 100000; editable: true; Accessible.name: qsTr("Crop X"); onValueModified: window.prepareCurrent() }
						Label { text: "Y" }
						SpinBox { id: cropY; from: 0; to: 100000; editable: true; Accessible.name: qsTr("Crop Y"); onValueModified: window.prepareCurrent() }
						Label { text: qsTr("Width") }
						SpinBox { id: cropWidth; from: 1; to: 100000; value: 1; editable: true; Accessible.name: qsTr("Crop width"); onValueModified: window.prepareCurrent() }
						Label { text: qsTr("Height") }
						SpinBox { id: cropHeight; from: 1; to: 100000; value: 1; editable: true; Accessible.name: qsTr("Crop height"); onValueModified: window.prepareCurrent() }
					}
					RowLayout {
						Layout.fillWidth: true
						Label { text: qsTr("Sample rate"); Layout.preferredWidth: 90 }
						ComboBox {
							id: sampleRateBox
							Layout.fillWidth: true
							model: txEditorModel.sampleRates
							currentIndex: Math.max(0, model.indexOf(48000))
							Accessible.name: qsTr("WAV sample rate")
							onActivated: window.prepareCurrent()
						}
					}
					CheckBox {
						id: fskEnabled
						text: qsTr("Append FSK ID")
						onToggled: window.prepareCurrent()
					}
					TextField {
						id: fskField
						Layout.fillWidth: true
						enabled: fskEnabled.checked
						maximumLength: 9
						placeholderText: qsTr("Identifier, maximum 9 characters")
						Accessible.name: qsTr("FSK identifier")
						onEditingFinished: window.prepareCurrent()
					}
					Button {
						text: qsTr("Refresh preview")
						Layout.fillWidth: true
						enabled: !txEditorModel.busy
						onClicked: window.prepareCurrent()
					}
					RowLayout {
						Layout.fillWidth: true
						Button {
							text: qsTr("Export prepared PNG...")
							Layout.fillWidth: true
							enabled: txEditorModel.ready && !txEditorModel.busy
							Accessible.name: qsTr("Export prepared PNG")
							onClicked: pngDialog.open()
						}
						Button {
							text: qsTr("Export WAV...")
							Layout.fillWidth: true
							enabled: txEditorModel.ready && !txEditorModel.busy
							Accessible.name: qsTr("Generate offline WAV")
							onClicked: wavDialog.open()
						}
					}
					Button {
						text: qsTr("Inspect WAV...")
						Layout.fillWidth: true
						enabled: !txEditorModel.busy
						onClicked: inspectDialog.open()
					}
					Label {
						objectName: "offlineSafetyNotice"
						Layout.fillWidth: true
						wrapMode: Text.WordWrap
						font.bold: true
						text: qsTr("Offline only: audio playback, sound-card access, radio control, and PTT are unavailable.")
						Accessible.name: qsTr("Offline safety notice")
					}
					Label {
						Layout.fillWidth: true
						visible: txEditorModel.errorText !== ""
						wrapMode: Text.WordWrap
						text: txEditorModel.errorText
						font.bold: true
						Accessible.name: qsTr("Preparation error")
					}
				}
			}
		}
	}

	footer: ToolBar {
		RowLayout {
			anchors.fill: parent
			anchors.leftMargin: 12
			anchors.rightMargin: 12
			Label { text: txEditorModel.statusText; Layout.fillWidth: true; elide: Text.ElideRight }
			Label { text: qsTr("Qt platform: %1").arg(txEditorModel.platformName); opacity: 0.72 }
		}
	}

	FileDialog {
		id: sourceDialog
		title: qsTr("Choose raster image")
		fileMode: FileDialog.OpenFile
		nameFilters: [qsTr("Raster images (*.png *.jpg *.jpeg)"), qsTr("All files (*)")]
		onAccepted: {
			txEditorModel.selectInput(selectedFile)
			Qt.callLater(window.prepareCurrent)
		}
	}
	FileDialog {
		id: pngDialog
		title: qsTr("Export prepared PNG")
		fileMode: FileDialog.SaveFile
		defaultSuffix: "png"
		nameFilters: [qsTr("PNG images (*.png)")]
		onAccepted: txEditorModel.exportPng(selectedFile)
	}
	FileDialog {
		id: wavDialog
		title: qsTr("Export offline PCM16 WAV")
		fileMode: FileDialog.SaveFile
		defaultSuffix: "wav"
		nameFilters: [qsTr("WAVE audio (*.wav)")]
		onAccepted: txEditorModel.exportWav(selectedFile)
	}
	FileDialog {
		id: inspectDialog
		title: qsTr("Inspect PCM16 WAV")
		fileMode: FileDialog.OpenFile
		nameFilters: [qsTr("WAVE audio (*.wav)"), qsTr("All files (*)")]
		onAccepted: txEditorModel.inspectWav(selectedFile)
	}
	Dialog {
		id: overwriteDialog
		title: qsTr("Replace existing file?")
		modal: true
		standardButtons: Dialog.Yes | Dialog.No
		Label { text: qsTr("The selected file already exists. Replace it atomically?") }
		onAccepted: {
			if (window.pendingOverwriteKind === "png")
				txEditorModel.exportPng(window.pendingOverwriteUrl, true)
			else
				txEditorModel.exportWav(window.pendingOverwriteUrl, true)
		}
	}
	Dialog {
		id: inspectionResults
		title: qsTr("WAV inspection")
		modal: true
		width: Math.min(window.width - 80, 560)
		height: Math.min(window.height - 80, 520)
		standardButtons: Dialog.Close
		TextArea {
			anchors.fill: parent
			readOnly: true
			selectByMouse: true
			text: txEditorModel.inspectionText
			Accessible.name: qsTr("WAV inspection results")
		}
	}
	Connections {
		target: txEditorModel
		function onOverwriteRequired(url, kind) {
			window.pendingOverwriteUrl = url
			window.pendingOverwriteKind = kind
			overwriteDialog.open()
		}
		function onInspectionChanged() {
			if (txEditorModel.inspectionText !== "") inspectionResults.open()
		}
	}
}
