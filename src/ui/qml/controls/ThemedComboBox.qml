import QtQuick
import QtQuick.Controls

import ".." as Shell

ComboBox {
    id: control

    implicitHeight: 38
    leftPadding: Shell.Theme.spacingMd
    rightPadding: Shell.Theme.spacingMd + indicator.width + Shell.Theme.spacingSm
    topPadding: Shell.Theme.spacingSm
    bottomPadding: Shell.Theme.spacingSm
    hoverEnabled: true

    delegate: ItemDelegate {
        id: optionDelegate
        required property var modelData
        required property int index

        width: control.width
        text: modelData
        highlighted: control.highlightedIndex === index
        hoverEnabled: true

        contentItem: Text {
            text: optionDelegate.text
            font: optionDelegate.font
            color: Shell.Theme.textPrimary
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: Shell.Theme.radiusSm
            color: optionDelegate.highlighted
                ? Shell.Theme.listItemCurrent
                : (optionDelegate.hovered ? Shell.Theme.listItemHover : "transparent")
        }
    }

    contentItem: TextInput {
        leftPadding: 0
        rightPadding: 0
        text: control.displayText
        font: control.font
        color: Shell.Theme.textPrimary
        verticalAlignment: Text.AlignVCenter
        readOnly: true
        selectByMouse: false
        cursorVisible: false
        selectionColor: "transparent"
        selectedTextColor: color
    }

    indicator: Canvas {
        x: control.width - width - Shell.Theme.spacingMd
        y: (control.height - height) / 2
        width: 10
        height: 6

        onPaint: {
            const context = getContext("2d")
            context.reset()
            context.moveTo(0, 0)
            context.lineTo(width, 0)
            context.lineTo(width / 2, height)
            context.closePath()
            context.fillStyle = Shell.Theme.textSecondary
            context.fill()
        }
    }

    background: Rectangle {
        radius: Shell.Theme.radiusSm
        color: control.pressed
            ? Shell.Theme.controlSurfacePressed
            : (control.hovered ? Shell.Theme.controlSurfaceHover : Shell.Theme.controlSurface)
        border.width: 1
        border.color: control.visualFocus ? Shell.Theme.controlBorderStrong : Shell.Theme.controlBorder
    }

    popup: Popup {
        y: control.height + Shell.Theme.spacingXs
        width: control.width
        padding: Shell.Theme.spacingXs

        background: Rectangle {
            color: Shell.Theme.surfaceContainer
            radius: Shell.Theme.radiusMd
            border.width: 1
            border.color: Shell.Theme.outline
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.delegateModel
            currentIndex: control.highlightedIndex
        }
    }
}
