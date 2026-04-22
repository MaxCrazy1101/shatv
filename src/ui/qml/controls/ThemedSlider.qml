import QtQuick
import QtQuick.Controls

import ".." as Shell

Slider {
    id: control

    implicitHeight: 24

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        width: control.availableWidth
        height: 4
        radius: 2
        color: Shell.Theme.controlAccentMuted

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: parent.radius
            color: Shell.Theme.controlAccent
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + (control.availableHeight - height) / 2
        width: 14
        height: 14
        radius: 7
        color: Shell.Theme.textPrimary
        border.width: 1
        border.color: Shell.Theme.controlBorder
    }
}
