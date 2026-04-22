import QtQuick
import QtQuick.Controls

import ".." as Shell

ToolButton {
    id: control

    property string controlType: "minimize"

    implicitWidth: Shell.Theme.windowButtonWidth
    implicitHeight: Shell.Theme.titleBarHeight
    hoverEnabled: true

    onControlTypeChanged: glyph.requestPaint()
    onEnabledChanged: glyph.requestPaint()

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

    contentItem: Item {
        Canvas {
            id: glyph
            anchors.centerIn: parent
            width: 14
            height: 14

            onPaint: {
                const context = getContext("2d")
                context.reset()
                context.strokeStyle = Shell.Theme.textPrimary
                context.lineWidth = 1.4

                if (control.controlType === "minimize") {
                    context.beginPath()
                    context.moveTo(2, 10)
                    context.lineTo(12, 10)
                    context.stroke()
                    return
                }

                if (control.controlType === "maximize") {
                    context.strokeRect(2.5, 2.5, 9, 9)
                    return
                }

                if (control.controlType === "restore") {
                    context.strokeRect(4.5, 2.5, 7, 7)
                    context.beginPath()
                    context.moveTo(4.5, 4.5)
                    context.lineTo(2.5, 4.5)
                    context.lineTo(2.5, 11.5)
                    context.lineTo(9.5, 11.5)
                    context.lineTo(9.5, 9.5)
                    context.stroke()
                    return
                }

                if (control.controlType === "close") {
                    context.beginPath()
                    context.moveTo(3, 3)
                    context.lineTo(11, 11)
                    context.moveTo(11, 3)
                    context.lineTo(3, 11)
                    context.stroke()
                    return
                }

                if (control.controlType === "fullscreen") {
                    context.beginPath()
                    context.moveTo(5, 2)
                    context.lineTo(2, 2)
                    context.lineTo(2, 5)
                    context.moveTo(9, 2)
                    context.lineTo(12, 2)
                    context.lineTo(12, 5)
                    context.moveTo(2, 9)
                    context.lineTo(2, 12)
                    context.lineTo(5, 12)
                    context.moveTo(12, 9)
                    context.lineTo(12, 12)
                    context.lineTo(9, 12)
                    context.stroke()
                }
            }
        }
    }
}
