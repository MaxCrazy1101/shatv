import QtQuick
import QtQuick.Controls

import ".." as Shell

ToolButton {
    id: control

    implicitHeight: 34
    leftPadding: Shell.Theme.spacingMd
    rightPadding: Shell.Theme.spacingMd
    topPadding: Shell.Theme.spacingSm
    bottomPadding: Shell.Theme.spacingSm
    hoverEnabled: true

    contentItem: Text {
        text: control.text
        font: control.font
        color: Shell.Theme.textPrimary
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Shell.Theme.radiusSm
        color: control.down
            ? Shell.Theme.controlSurfacePressed
            : (control.hovered ? Shell.Theme.controlSurfaceHover : Shell.Theme.controlSurface)
        border.width: 1
        border.color: control.visualFocus ? Shell.Theme.controlBorderStrong : Shell.Theme.controlBorder
        opacity: control.enabled ? 1.0 : 0.6
    }
}
