// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
  id: window
  width: 1200
  height: 760
  minimumWidth: 860
  minimumHeight: 560
  visible: true
  title: qsTr("SSTV Transceiver — Foundation")

  header: ToolBar {
    RowLayout {
      anchors.fill: parent
      anchors.leftMargin: 12
      anchors.rightMargin: 12

      Label {
        text: qsTr("SSTV Transceiver")
        font.bold: true
        font.pixelSize: 18
      }

      Item { Layout.fillWidth: true }

      Label {
        text: qsTr("M0 · offline foundation")
        opacity: 0.75
      }
    }
  }

  SplitView {
    anchors.fill: parent
    orientation: Qt.Horizontal

    Pane {
      SplitView.fillWidth: true
      SplitView.minimumWidth: 520

      ColumnLayout {
        anchors.fill: parent
        spacing: 10

        GroupBox {
          title: qsTr("Received image")
          Layout.fillWidth: true
          Layout.fillHeight: true

          Rectangle {
            anchors.fill: parent
            color: "#15181c"
            border.color: "#39424c"

            Label {
              anchors.centerIn: parent
              width: Math.min(parent.width - 40, 460)
              horizontalAlignment: Text.AlignHCenter
              wrapMode: Text.WordWrap
              text: qsTr("The rendering surface is ready for the M3/M4 decoder. " +
                         "This foundation build does not decode or transmit audio.")
              color: "#ccd5df"
            }
          }
        }

        GroupBox {
          title: qsTr("Waterfall and waveform")
          Layout.fillWidth: true
          Layout.preferredHeight: 190

          Rectangle {
            anchors.fill: parent
            color: "#101317"

            Label {
              anchors.centerIn: parent
              text: qsTr("Signal visualisation arrives with live RX")
              color: "#8995a3"
            }
          }
        }
      }
    }

    Pane {
      SplitView.preferredWidth: 330
      SplitView.minimumWidth: 280

      ColumnLayout {
        anchors.fill: parent
        spacing: 12

        GroupBox {
          title: qsTr("Receiver")
          Layout.fillWidth: true

          GridLayout {
            anchors.fill: parent
            columns: 2

            Label { text: qsTr("Mode") }
            ComboBox {
              Layout.fillWidth: true
              model: [qsTr("No verified modes in M0")]
              enabled: false
            }

            Label { text: qsTr("Input") }
            ComboBox {
              Layout.fillWidth: true
              model: [qsTr("Audio integration: M2")]
              enabled: false
            }
          }
        }

        GroupBox {
          title: qsTr("Transmitter")
          Layout.fillWidth: true

          ColumnLayout {
            anchors.fill: parent

            Button {
              text: qsTr("Choose image…")
              Layout.fillWidth: true
              enabled: false
            }

            Button {
              text: qsTr("Transmit")
              Layout.fillWidth: true
              highlighted: true
              enabled: false
            }
          }
        }

        GroupBox {
          title: qsTr("Rig control")
          Layout.fillWidth: true

          Label {
            anchors.fill: parent
            wrapMode: Text.WordWrap
            text: qsTr("PTT is intentionally unavailable until the M2 watchdog and " +
                       "fault-injection tests pass.")
          }
        }

        Item { Layout.fillHeight: true }
      }
    }
  }

  footer: ToolBar {
    RowLayout {
      anchors.fill: parent
      anchors.leftMargin: 12
      anchors.rightMargin: 12

      Label {
        text: qsTr("Idle · no audio stream · PTT disabled")
      }

      Item { Layout.fillWidth: true }

      Label {
        text: qsTr("Wayland/XCB selected by Qt")
        opacity: 0.7
      }
    }
  }
}

