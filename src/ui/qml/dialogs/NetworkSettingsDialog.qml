import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ".." as Shell
import "../controls" as Controls

Dialog {
    id: root

    signal submitted(string userAgent, string epgUrl)

    parent: Overlay.overlay
    x: Math.round(Math.max(0, ((parent ? parent.width : 0) - width) / 2))
    y: Math.round(Math.max(0, ((parent ? parent.height : 0) - height) / 2))
    modal: true
    focus: true
    width: 520
    padding: Shell.Theme.spacingMd
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    title: qsTr("Network Settings")

    function openWithValues(userAgent, epgUrl) {
        userAgentField.text = userAgent
        epgUrlField.text = epgUrl
        open()
        userAgentField.forceActiveFocus()
    }

    background: Rectangle {
        radius: Shell.Theme.radiusMd
        color: Shell.Theme.surfaceContainer
        border.width: 1
        border.color: Shell.Theme.outline
    }

    contentItem: ColumnLayout {
        spacing: Shell.Theme.spacingMd

        Label {
            Layout.fillWidth: true
            text: qsTr("Configure the HTTP User-Agent and an optional XMLTV EPG URL used for current/next programme lookup.")
            color: Shell.Theme.textSecondary
            wrapMode: Text.WordWrap
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Shell.Theme.spacingSm

            Label {
                text: qsTr("User-Agent")
                color: Shell.Theme.textPrimary
            }

            Controls.ThemedTextField {
                id: userAgentField
                Layout.fillWidth: true
                placeholderText: qsTr("Leave empty to use the default")
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Shell.Theme.spacingSm

            Label {
                text: qsTr("EPG URL")
                color: Shell.Theme.textPrimary
            }

            Controls.ThemedTextField {
                id: epgUrlField
                Layout.fillWidth: true
                placeholderText: qsTr("https://example.com/guide.xml.gz")
            }
        }
    }

    footer: Item {
        implicitHeight: 56

        RowLayout {
            anchors.fill: parent
            anchors.margins: Shell.Theme.spacingMd
            spacing: Shell.Theme.spacingSm

            Item {
                Layout.fillWidth: true
            }

            Controls.ThemedToolButton {
                text: qsTr("Cancel")
                onClicked: root.close()
            }

            Controls.ThemedToolButton {
                text: qsTr("Save")
                onClicked: {
                    root.submitted(userAgentField.text.trim(), epgUrlField.text.trim())
                    root.close()
                }
            }
        }
    }
}
