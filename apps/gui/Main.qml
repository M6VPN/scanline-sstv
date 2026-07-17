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
	property string pendingAudioDiagnostic

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
			Button {
				text: qsTr("Audio diagnostics...")
				Accessible.name: qsTr("Open audio diagnostics")
				onClicked: audioDiagnosticsDialog.open()
			}
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
						text: qsTr("SSTV audio playback, radio control, and PTT are unavailable. Audio-interface diagnostics are separate and require explicit arming.")
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
		id: audioDiagnosticsDialog
		objectName: "audioDiagnosticsPanel"
		title: qsTr("Audio interface diagnostics")
		modal: true
		width: Math.min(window.width - 40, 760)
		height: Math.min(window.height - 40, 700)
		standardButtons: Dialog.Close
		onRejected: audioDiagnosticsModel.stop()
		ScrollView {
			anchors.fill: parent
			contentWidth: availableWidth
			ColumnLayout {
				width: parent.width
				spacing: 8
				Label {
					Layout.fillWidth: true
					wrapMode: Text.WordWrap
					font.bold: true
					text: qsTr("Calibration only. No SSTV audio. PTT unavailable. Output and loopback require a fresh warning confirmation.")
					Accessible.name: qsTr("Audio diagnostics safety status")
				}
				RowLayout {
					Layout.fillWidth: true
					Label { text: qsTr("Backend") }
					ComboBox {
						id: audioBackendBox
						Layout.fillWidth: true
						model: audioDiagnosticsModel.backends
						textRole: "name"
						valueRole: "id"
						Accessible.name: qsTr("Audio backend")
					}
					Button {
						text: qsTr("Refresh Devices")
						enabled: !audioDiagnosticsModel.running
						Accessible.name: qsTr("Refresh exact audio devices")
						onClicked: {
							playbackDeviceBox.currentIndex = -1
							captureDeviceBox.currentIndex = -1
							audioDiagnosticsModel.refreshDevices(audioBackendBox.currentValue)
						}
					}
				}
				Label { text: qsTr("Playback device") }
				ComboBox {
					id: playbackDeviceBox
					Layout.fillWidth: true
					model: audioDiagnosticsModel.playbackDevices
					textRole: "label"
					valueRole: "id"
					currentIndex: -1
					Accessible.name: qsTr("Exact playback device")
				}
				Label { text: qsTr("Capture device") }
				ComboBox {
					id: captureDeviceBox
					Layout.fillWidth: true
					model: audioDiagnosticsModel.captureDevices
					textRole: "label"
					valueRole: "id"
					currentIndex: -1
					Accessible.name: qsTr("Exact capture device")
				}
				GridLayout {
					Layout.fillWidth: true
					columns: 4
					Label { text: qsTr("Output channel") }
					SpinBox { id: outputChannel; from: 0; to: 63; Accessible.name: qsTr("Output channel") }
					Label { text: qsTr("Output channels") }
					SpinBox { id: outputChannels; from: 1; to: 64; value: 2; Accessible.name: qsTr("Requested output channels") }
					Label { text: qsTr("Input channel") }
					SpinBox { id: inputChannel; from: 0; to: 63; Accessible.name: qsTr("Input channel") }
					Label { text: qsTr("Input channels") }
					SpinBox { id: inputChannels; from: 1; to: 64; value: 2; Accessible.name: qsTr("Requested input channels") }
					Label { text: qsTr("Period frames") }
					SpinBox { id: audioPeriodFrames; from: 16; to: 8192; value: 480; editable: true; Accessible.name: qsTr("Audio period frames") }
					Label { text: qsTr("Period count") }
					SpinBox { id: audioPeriodCount; from: 2; to: 16; value: 3; Accessible.name: qsTr("Audio period count") }
					Label { text: qsTr("Duration seconds") }
					SpinBox { id: audioDuration; from: 1; to: 10; value: 2; Accessible.name: qsTr("Diagnostic duration") }
					Label { text: qsTr("Level dBFS") }
					SpinBox { id: audioLevel; from: -60; to: -6; value: -30; Accessible.name: qsTr("Calibration output level in dBFS") }
				}
				ProgressBar {
					Layout.fillWidth: true
					from: -120
					to: 0
					value: audioDiagnosticsModel.peakDbfs
					Accessible.name: qsTr("Capture peak level")
					Accessible.description: qsTr("Peak level in decibels relative to full scale")
				}
				Label {
					text: qsTr("Peak %1 dBFS").arg(audioDiagnosticsModel.peakDbfs.toFixed(1))
					Accessible.name: qsTr("Capture peak numeric value")
				}
				RowLayout {
					Layout.fillWidth: true
					Button {
						text: qsTr("Run input meter")
						enabled: !audioDiagnosticsModel.running && captureDeviceBox.currentIndex >= 0
						Accessible.name: qsTr("Run bounded input level meter")
						onClicked: audioDiagnosticsModel.startMeter(audioBackendBox.currentValue,
							captureDeviceBox.currentValue, inputChannel.value, inputChannels.value,
							audioDuration.value, audioPeriodFrames.value, audioPeriodCount.value)
					}
					Button {
						text: qsTr("Arm output calibration...")
						enabled: !audioDiagnosticsModel.running && playbackDeviceBox.currentIndex >= 0
						Accessible.name: qsTr("Arm audio interface output calibration")
						onClicked: {
							window.pendingAudioDiagnostic = "output"
							audioArmDialog.open()
						}
					}
					Button {
						text: qsTr("Arm local loopback...")
						enabled: !audioDiagnosticsModel.running
							&& playbackDeviceBox.currentIndex >= 0 && captureDeviceBox.currentIndex >= 0
						Accessible.name: qsTr("Arm local cable loopback")
						onClicked: {
							window.pendingAudioDiagnostic = "loopback"
							audioArmDialog.open()
						}
					}
				}
				Button {
					text: qsTr("Stop")
					Layout.fillWidth: true
					enabled: audioDiagnosticsModel.running
					font.bold: true
					Accessible.name: qsTr("Emergency stop audio diagnostic")
					onClicked: audioDiagnosticsModel.stop()
				}
				Label {
					Layout.fillWidth: true
					wrapMode: Text.WordWrap
					font.bold: true
					text: audioDiagnosticsModel.state.toUpperCase() + ": "
						+ audioDiagnosticsModel.statusText
					Accessible.name: qsTr("Audio diagnostic state")
				}
				TextArea {
					Layout.fillWidth: true
					Layout.preferredHeight: 180
					readOnly: true
					text: audioDiagnosticsModel.resultText
					Accessible.name: qsTr("Audio diagnostic negotiated facts and results")
				}
			}
		}
	}
	Dialog {
		id: audioArmDialog
		title: qsTr("Arm real audio for this run?")
		modal: true
		standardButtons: Dialog.Yes | Dialog.No
		Label {
			width: 520
			wrapMode: Text.WordWrap
			text: qsTr("Disconnect radio transmit audio where practical. Disable radio VOX and ensure no external PTT is asserted. Reduce monitor/headphone volume. Use a local cable or dummy audio load for loopback. This confirmation applies to one run only.")
			Accessible.name: qsTr("Real audio safety warning")
		}
		onAccepted: {
			if (window.pendingAudioDiagnostic === "output") {
				audioDiagnosticsModel.startOutput(audioBackendBox.currentValue,
					playbackDeviceBox.currentValue, outputChannel.value, outputChannels.value,
					audioDuration.value, audioPeriodFrames.value, audioPeriodCount.value,
					audioLevel.value, true)
			} else {
				audioDiagnosticsModel.startLoopback(audioBackendBox.currentValue,
					playbackDeviceBox.currentValue, captureDeviceBox.currentValue,
					outputChannel.value, inputChannel.value, outputChannels.value,
					inputChannels.value, audioPeriodFrames.value, audioPeriodCount.value,
					audioLevel.value, true)
			}
			window.pendingAudioDiagnostic = ""
		}
		onRejected: window.pendingAudioDiagnostic = ""
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
