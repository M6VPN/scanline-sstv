// Scanline SSTV - Developed by M6VPN (M6VPN@tuta.com)
// scanline-sstv/apps/gui/LiveTransmitPanel.qml
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Button {
	id: root
	objectName: "liveTransmitPanel"
	property var hostWindow
	property bool ready: liveTransmitModel.ready
	text: qsTr("Live transmit...")
	enabled: !liveTransmitModel.active
	Accessible.name: qsTr("Open explicitly armed live transmit panel")
	onClicked: panel.open()

	function applyConfiguration() {
		liveTransmitModel.updateConfiguration(
			backendBox.currentValue, deviceBox.currentValue,
			outputChannel.value, playbackChannels.value, gain.value,
			providerBox.currentValue, addressField.text, port.value,
			pathField.text, preKey.value, postAudio.value)
	}

	function selectRetainedIdentity() {
		deviceBox.currentIndex = -1
		for (let index = 0; index < deviceBox.count; ++index) {
			if (deviceBox.valueAt(index) === liveTransmitModel.selectedPlaybackIdentity) {
				deviceBox.currentIndex = index
				break
			}
		}
	}

	Dialog {
		id: panel
		objectName: "liveTransmitWorkspace"
		parent: root.hostWindow ? root.hostWindow.contentItem : root
		title: qsTr("Live image transmission")
		modal: true
		width: root.hostWindow ? Math.min(root.hostWindow.width - 40, 760) : 700
		height: root.hostWindow ? Math.min(root.hostWindow.height - 40, 700) : 640
		standardButtons: Dialog.Close
		closePolicy: liveTransmitModel.active ? Popup.NoAutoClose
			: Popup.CloseOnEscape | Popup.CloseOnPressOutside
		onRejected: if (liveTransmitModel.active) liveTransmitModel.stop()
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
					text: qsTr("Live TX is build-gated and single-run armed. No default device, remote PTT, VOX, unattended operation, or automatic fallback is available.")
					Accessible.name: qsTr("Live transmit safety scope")
				}
				Label { text: qsTr("Prepared transmission") }
				Label {
					Layout.fillWidth: true
					wrapMode: Text.WordWrap
					text: txEditorModel.modeMetadata + " | "
						+ txEditorModel.durationMetadata + " | " + txEditorModel.frameMetadata
					Accessible.name: qsTr("Prepared transmission facts")
				}
				RowLayout {
					Layout.fillWidth: true
					Label { text: qsTr("Backend") }
					ComboBox {
						id: backendBox
						Layout.fillWidth: true
						model: liveTransmitModel.backends
						textRole: "name"
						valueRole: "id"
						Accessible.name: qsTr("Exact audio backend")
					}
					Button {
						text: qsTr("Refresh Devices")
						enabled: txEditorModel.ready && !liveTransmitModel.active
						Accessible.name: qsTr("Refresh exact playback devices")
						onClicked: liveTransmitModel.refreshDevices(backendBox.currentValue)
					}
				}
				ComboBox {
					id: deviceBox
					Layout.fillWidth: true
					model: liveTransmitModel.playbackDevices
					textRole: "label"
					valueRole: "id"
					currentIndex: -1
					Accessible.name: qsTr("Exact playback device identity")
					onActivated: root.applyConfiguration()
				}
				GridLayout {
					Layout.fillWidth: true
					columns: 4
					Label { text: qsTr("Output channel") }
					SpinBox { id: outputChannel; from: 0; to: 63; Accessible.name: qsTr("Live output channel"); onValueModified: root.applyConfiguration() }
					Label { text: qsTr("Channel count") }
					SpinBox { id: playbackChannels; from: 1; to: 64; value: 2; Accessible.name: qsTr("Live playback channel count"); onValueModified: root.applyConfiguration() }
					Label { text: qsTr("Gain dBFS") }
					SpinBox { id: gain; from: -60; to: -6; value: -30; Accessible.name: qsTr("Constant live software gain"); onValueModified: root.applyConfiguration() }
					Label { text: qsTr("PTT provider") }
					ComboBox {
						id: providerBox
						model: [{ text: "flrig", value: "flrig" },
							{ text: "rigctld", value: "rigctld" }]
						textRole: "text"
						valueRole: "value"
						Accessible.name: qsTr("Loopback PTT provider")
						onActivated: root.applyConfiguration()
					}
					Label { text: qsTr("Address") }
					TextField { id: addressField; text: "127.0.0.1"; Accessible.name: qsTr("Literal loopback PTT address"); onEditingFinished: root.applyConfiguration() }
					Label { text: qsTr("Port") }
					SpinBox { id: port; from: 1; to: 65535; value: 1; editable: true; Accessible.name: qsTr("Explicit PTT port"); onValueModified: root.applyConfiguration() }
					Label { text: qsTr("flrig path") }
					TextField { id: pathField; text: "/RPC2"; enabled: providerBox.currentValue === "flrig"; Accessible.name: qsTr("Explicit flrig XML RPC path"); onEditingFinished: root.applyConfiguration() }
					Label { text: qsTr("Pre-key ms") }
					SpinBox { id: preKey; from: 0; to: 10000; value: 250; editable: true; Accessible.name: qsTr("Pre-key delay milliseconds"); onValueModified: root.applyConfiguration() }
					Label { text: qsTr("Post-audio ms") }
					SpinBox { id: postAudio; from: 0; to: 10000; value: 250; editable: true; Accessible.name: qsTr("Post-audio delay milliseconds"); onValueModified: root.applyConfiguration() }
				}
				RowLayout {
					Layout.fillWidth: true
					Button {
						text: qsTr("Check PTT state")
						enabled: !liveTransmitModel.active
						Accessible.name: qsTr("Read only PTT state check")
						onClicked: liveTransmitModel.checkPttState(providerBox.currentValue,
							addressField.text, port.value, pathField.text)
					}
					Label { text: qsTr("PTT: %1").arg(liveTransmitModel.pttCertainty) }
					Item { Layout.fillWidth: true }
					Button {
						objectName: "liveTransmitAction"
						text: qsTr("Transmit")
						enabled: liveTransmitModel.ready
						Accessible.name: qsTr("Review and arm live image transmission")
						onClicked: {
							root.applyConfiguration()
							if (liveTransmitModel.ready) safetyDialog.open()
						}
					}
				}
				ProgressBar {
					Layout.fillWidth: true
					from: 0
					to: 1
					value: liveTransmitModel.progress
					Accessible.name: qsTr("Accepted live source frame progress")
				}
				Label {
					Layout.fillWidth: true
					wrapMode: Text.WordWrap
					font.bold: true
					text: liveTransmitModel.state.toUpperCase() + ": "
						+ liveTransmitModel.statusText
					Accessible.name: qsTr("Live transmit state and status")
				}
				Label {
					Layout.fillWidth: true
					wrapMode: Text.WordWrap
					text: qsTr("PTT: %1 | Watchdog: %2 | Audio: %3")
						.arg(liveTransmitModel.pttCertainty)
						.arg(liveTransmitModel.watchdogStatus)
						.arg(liveTransmitModel.audioStatus)
					Accessible.name: qsTr("PTT watchdog and audio cleanup status")
				}
				Label {
					Layout.fillWidth: true
					visible: liveTransmitModel.primaryError !== ""
					wrapMode: Text.WordWrap
					text: qsTr("Primary failure: %1").arg(liveTransmitModel.primaryError)
					Accessible.name: qsTr("Primary live transmit failure")
				}
				Label {
					Layout.fillWidth: true
					visible: liveTransmitModel.cleanupErrors !== ""
					wrapMode: Text.WordWrap
					text: qsTr("Cleanup failures: %1").arg(liveTransmitModel.cleanupErrors)
					Accessible.name: qsTr("Live transmit cleanup failures")
				}
				Button {
					objectName: "liveStopAction"
					text: qsTr("Stop / Cancel")
					Layout.fillWidth: true
					enabled: liveTransmitModel.active
					font.bold: true
					Accessible.name: qsTr("Stop live transmission and unkey safely")
					onClicked: liveTransmitModel.stop()
				}
				Button {
					text: qsTr("Retry unkey")
					Layout.fillWidth: true
					enabled: liveTransmitModel.hazardous
					Accessible.name: qsTr("Retry confirmed PTT unkey only")
					onClicked: liveTransmitModel.retryUnkey()
				}
			}
		}
	}

	Dialog {
		id: safetyDialog
		objectName: "liveTransmitSafetyDialog"
		parent: root.hostWindow ? root.hostWindow.contentItem : root
		title: qsTr("Final live transmit safety review")
		modal: true
		width: root.hostWindow ? Math.min(root.hostWindow.width - 60, 680) : 620
		standardButtons: Dialog.Ok | Dialog.Cancel
		property bool canConfirm: armAudio.checked && armPtt.checked
			&& armLive.checked && phrase.text === liveTransmitModel.confirmationPhrase
		onOpened: {
			armAudio.checked = false
			armPtt.checked = false
			armLive.checked = false
			phrase.clear()
			standardButton(Dialog.Ok).enabled = Qt.binding(function() { return safetyDialog.canConfirm })
		}
		onAccepted: liveTransmitModel.confirmAndTransmit(armAudio.checked,
			armPtt.checked, armLive.checked, phrase.text)
		ColumnLayout {
			width: parent.width
			Image {
				Layout.fillWidth: true
				Layout.preferredHeight: 180
				source: txEditorModel.previewSource
				fillMode: Image.PreserveAspectFit
				cache: false
				smooth: false
				Accessible.name: qsTr("Exact prepared live transmit preview")
			}
			Label {
				Layout.fillWidth: true
				wrapMode: Text.WordWrap
				text: txEditorModel.modeMetadata + " | " + txEditorModel.durationMetadata
					+ " | " + txEditorModel.frameMetadata + "\n"
					+ qsTr("Audio: %1 / %2 / channel %3 of %4 / %5 dBFS\n")
						.arg(backendBox.currentValue).arg(deviceBox.currentValue)
						.arg(outputChannel.value).arg(playbackChannels.value).arg(gain.value)
					+ qsTr("PTT: %1 %2:%3 / pre-key %4 ms / post-audio %5 ms")
						.arg(providerBox.currentValue).arg(addressField.text).arg(port.value)
						.arg(preKey.value).arg(postAudio.value)
				Accessible.name: qsTr("Immutable live transmission review facts")
			}
			Label {
				Layout.fillWidth: true
				wrapMode: Text.WordWrap
				font.bold: true
				text: qsTr("Disable VOX. Verify frequency and radio mode. Use minimum RF power and audio level. Keep a hardware unkey method ready. Enable radio timeout or TOT where available.")
				Accessible.name: qsTr("Live radio safety warnings")
			}
			CheckBox { id: armAudio; text: qsTr("I arm real audio for this transmission only"); Accessible.name: text }
			CheckBox { id: armPtt; text: qsTr("I arm automatic PTT for this transmission only"); Accessible.name: text }
			CheckBox { id: armLive; text: qsTr("I arm live SSTV transmission for this transmission only"); Accessible.name: text }
			Label { text: qsTr("Type exactly: %1").arg(liveTransmitModel.confirmationPhrase) }
			TextField {
				id: phrase
				Layout.fillWidth: true
				Accessible.name: qsTr("Exact live transmit confirmation phrase")
			}
		}
	}

	Connections {
		target: liveTransmitModel
		function onDevicesChanged() { root.selectRetainedIdentity() }
	}
}
