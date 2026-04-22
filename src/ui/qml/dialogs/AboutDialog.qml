import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ".." as Shell
import "../controls" as Controls

Dialog {
    id: root

    property string appVersion: ""
    property string buildId: ""

    parent: Overlay.overlay
    x: Math.round(Math.max(0, ((parent ? parent.width : 0) - width) / 2))
    y: Math.round(Math.max(0, ((parent ? parent.height : 0) - height) / 2))
    modal: true
    focus: true
    width: 420
    padding: Shell.Theme.spacingMd
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    title: qsTr("About ShaTV")

    background: Rectangle {
        radius: Shell.Theme.radiusMd
        color: Shell.Theme.surfaceContainer
        border.width: 1
        border.color: Shell.Theme.outline
    }

    contentItem: ColumnLayout {
        spacing: Shell.Theme.spacingMd

        Label {
            text: qsTr("ShaTV")
            color: Shell.Theme.textPrimary
            font.pixelSize: 22
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("A cross-platform IPTV player.")
            color: Shell.Theme.textSecondary
            wrapMode: Text.WordWrap
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: Shell.Theme.spacingMd
            rowSpacing: Shell.Theme.spacingSm

            Label {
                text: qsTr("Version")
                color: Shell.Theme.textSecondary
            }

            Label {
                text: root.appVersion
                color: Shell.Theme.textPrimary
            }

            Label {
                text: qsTr("Build")
                color: Shell.Theme.textSecondary
            }

            Label {
                text: root.buildId
                color: Shell.Theme.textPrimary
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("Built with C++20, Qt 6, and libmpv.")
            color: Shell.Theme.textSecondary
            wrapMode: Text.WordWrap
        }

        Label {
            text: qsTr("Authors")
            color: Shell.Theme.textPrimary
            font.bold: true
        }

        Text {
            Layout.fillWidth: true
            textFormat: Text.RichText
            text: "MaxCrazy (<a href=\"mailto:alex02newton@gmail.com\">alex02newton@gmail.com</a>)"
            color: Shell.Theme.textPrimary
            wrapMode: Text.Wrap
            onLinkActivated: link => Qt.openUrlExternally(link)
        }

        Label {
            text: qsTr("License")
            color: Shell.Theme.textPrimary
            font.bold: true
        }

        Label {
            text: qsTr("Not specified")
            color: Shell.Theme.textSecondary
        }

        Label {
            text: qsTr("Repository")
            color: Shell.Theme.textPrimary
            font.bold: true
        }

        Text {
            Layout.fillWidth: true
            textFormat: Text.RichText
            text: "<a href=\"https://github.com/MaxCrazy1101/shatv\">github.com/MaxCrazy1101/shatv</a>"
            color: Shell.Theme.textPrimary
            wrapMode: Text.Wrap
            onLinkActivated: link => Qt.openUrlExternally(link)
        }
    }

    footer: Item {
        implicitHeight: 56

        RowLayout {
            anchors.fill: parent
            anchors.margins: Shell.Theme.spacingMd

            Item {
                Layout.fillWidth: true
            }

            Controls.ThemedToolButton {
                text: qsTr("Close")
                onClicked: root.close()
            }
        }
    }
}
