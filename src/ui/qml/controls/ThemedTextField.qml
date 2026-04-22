import QtQuick
import QtQuick.Controls

import ".." as Shell

TextField {
    id: control

    implicitHeight: 38
    color: Shell.Theme.textPrimary
    placeholderTextColor: Shell.Theme.textSecondary
    selectedTextColor: Shell.Theme.surface
    selectionColor: Shell.Theme.controlAccent
    leftPadding: Shell.Theme.spacingMd
    rightPadding: Shell.Theme.spacingMd
    topPadding: Shell.Theme.spacingSm
    bottomPadding: Shell.Theme.spacingSm

    background: Rectangle {
        radius: Shell.Theme.radiusSm
        color: control.enabled ? Shell.Theme.controlSurface : Shell.Theme.controlSurfaceDisabled
        border.width: 1
        border.color: control.activeFocus ? Shell.Theme.controlBorderStrong : Shell.Theme.controlBorder
    }
}
