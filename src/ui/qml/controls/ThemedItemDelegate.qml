import QtQuick
import QtQuick.Controls

import ".." as Shell

ItemDelegate {
    id: control

    leftPadding: Shell.Theme.spacingMd
    rightPadding: Shell.Theme.spacingMd
    topPadding: Shell.Theme.spacingSm
    bottomPadding: Shell.Theme.spacingSm
    hoverEnabled: true

    contentItem: Text {
        text: control.text
        font: control.font
        color: Shell.Theme.textPrimary
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: Shell.Theme.radiusSm
        color: control.highlighted
            ? Shell.Theme.listItemCurrent
            : (control.hovered ? Shell.Theme.listItemHover : "transparent")
        border.width: control.visualFocus ? 1 : 0
        border.color: Shell.Theme.controlBorderStrong
    }
}
