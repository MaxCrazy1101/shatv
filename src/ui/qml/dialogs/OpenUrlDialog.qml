import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import ".." as Shell
import "../controls" as Controls

Dialog {
    id: root

    signal submitted(string urlText)

    parent: Overlay.overlay
    x: Math.round(Math.max(0, ((parent ? parent.width : 0) - width) / 2))
    y: Math.round(Math.max(0, ((parent ? parent.height : 0) - height) / 2))
    modal: true
    focus: true
    width: 500
    padding: Shell.Theme.spacingMd
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    title: qsTr("Open Link")

    function openWithValue(urlText) {
        urlField.text = urlText.length > 0 ? urlText : "http://"
        validationMessage.visible = false
        validationMessage.text = ""
        open()
        urlField.forceActiveFocus()
        urlField.selectAll()
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
            text: qsTr("URL")
            color: Shell.Theme.textPrimary
        }

        Controls.ThemedTextField {
            id: urlField
            Layout.fillWidth: true
            text: "http://"
        }

        Label {
            id: validationMessage
            Layout.fillWidth: true
            visible: false
            color: "#e6a0a0"
            wrapMode: Text.WordWrap
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
                text: qsTr("Open")
                onClicked: {
                    const trimmedUrl = urlField.text.trim()
                    if (trimmedUrl.length === 0) {
                        validationMessage.text = qsTr("URL cannot be empty.")
                        validationMessage.visible = true
                        return
                    }

                    validationMessage.visible = false
                    validationMessage.text = ""
                    root.submitted(trimmedUrl)
                    root.close()
                }
            }
        }
    }
}
