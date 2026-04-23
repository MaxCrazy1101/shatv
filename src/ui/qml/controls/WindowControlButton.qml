import QtQuick
import QtQuick.Controls

import ".." as Shell

ToolButton {
    id: control

    property string controlType: "minimize"
    readonly property string symbolName: {
        if (control.controlType === "minimize") {
            return "minimize"
        }
        if (control.controlType === "maximize") {
            return "crop_square"
        }
        if (control.controlType === "restore") {
            return "filter_none"
        }
        if (control.controlType === "close") {
            return "close"
        }
        if (control.controlType === "fullscreen") {
            return "fullscreen"
        }
        return "settings"
    }

    implicitWidth: Shell.Theme.windowButtonWidth
    implicitHeight: Shell.Theme.titleBarHeight
    hoverEnabled: true

    background: Rectangle {
        color: {
            if (control.controlType === "close") {
                if (control.down) {
                    return Shell.Theme.titleBarClosePressed
                }
                if (control.hovered) {
                    return Shell.Theme.titleBarCloseHover
                }
                return "transparent"
            }

            if (control.down) {
                return Shell.Theme.controlSurfacePressed
            }
            if (control.hovered) {
                return Shell.Theme.controlSurfaceHover
            }
            return "transparent"
        }
    }

    contentItem: MaterialSymbolIcon {
        anchors.centerIn: parent
        symbolName: control.symbolName
        iconSize: control.controlType === "minimize" ? 18 : 20
    }
}
