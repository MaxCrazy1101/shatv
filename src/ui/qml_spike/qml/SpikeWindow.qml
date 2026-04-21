import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ShaTV.Video 1.0

ApplicationWindow {
    id: root
    readonly property var bridge: spikePlaybackBridge
    width: 1280
    height: 720
    visible: true
    title: qsTr("ShaTV Pure QML Spike")
    color: "#07111f"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: qsTr("Phase 1 Spike: pure QML host")
            color: "#e8eefc"
            font.pixelSize: 24
            font.bold: true
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Button {
                text: qsTr("Play / Pause")
                onClicked: bridge.togglePlayPause()
            }

            Button {
                text: qsTr("Stop")
                onClicked: bridge.stop()
            }

            Label {
                Layout.fillWidth: true
                text: qsTr("State: %1 | Ready: %2").arg(bridge.playbackState).arg(bridge.videoReady ? "yes" : "no")
                color: "#e8eefc"
                elide: Text.ElideRight
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Source: %1").arg(bridge.sourceLabel)
            color: "#9fb2d8"
            elide: Text.ElideMiddle
        }

        Label {
            Layout.fillWidth: true
            visible: bridge.statusMessage.length > 0
            text: bridge.statusMessage
            color: "#c7d7f5"
            wrapMode: Text.Wrap
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: 12
            color: "#101b31"
            border.width: 1
            border.color: "#24304b"

            MpvVideoItem {
                id: mpvVideoItem
                objectName: "mpvVideoItem"
                anchors.fill: parent
            }

            Label {
                visible: !mpvVideoItem.ready
                anchors.centerIn: parent
                text: qsTr("Waiting for OpenGL render context")
                color: "#e8eefc"
            }
        }
    }
}
