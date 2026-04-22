pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import ShaTV.Video 1.0
import "."

Rectangle {
    id: root
    objectName: "mainWindowRoot"
    color: Theme.surfaceDim

    QtObject {
        id: bridgeFallback

        property var channelModel: null
        property var availableGroups: []
        property string currentGroupFilter: ""
        property string searchText: ""
        property var recentItems: []
        property bool fullscreenActive: false
        property string statusMessage: ""
        property string currentChannelName: ""
        property string currentProgrammeText: ""
        property string nextProgrammeText: ""
        property string playbackStateText: ""
        property string playbackStateToken: "idle"
        property bool playing: false
        property bool muted: false
        property int volume: 50

        function activateChannelRow(row) {}
        function setSearchText(text) {}
        function setGroupFilter(group) {}
        function requestPlayPause() {}
        function requestStop() {}
        function toggleMute() {}
        function setVolume(volume) {}
        function requestOpenFile() {}
        function requestOpenUrl() {}
        function requestNetworkSettings() {}
        function requestAbout() {}
        function openRecentAt(index) {}
        function toggleFullscreen() {}
        function exitFullscreen() {}
    }

    readonly property var bridge: (typeof mainWindowBridge !== "undefined" && mainWindowBridge !== null)
        ? mainWindowBridge
        : bridgeFallback
    readonly property var groupItems: [qsTr("All groups"), ...bridge.availableGroups]
    readonly property int menuPopupType: Popup.Item
    readonly property string effectiveStatusText: bridge.statusMessage.length > 0
        ? bridge.statusMessage
        : (bridge.playbackStateText.length > 0 ? bridge.playbackStateText : qsTr("Ready"))
    readonly property bool showPauseAction: bridge.playbackStateToken === "playing"
        || bridge.playbackStateToken === "loading"
        || bridge.playbackStateToken === "buffering"
        || bridge.playbackStateToken === "retrying"
    readonly property int selectedGroupIndex: {
        if (bridge.currentGroupFilter.length === 0) {
            return 0
        }
        const idx = bridge.availableGroups.indexOf(bridge.currentGroupFilter)
        return idx >= 0 ? idx + 1 : 0
    }

    function applyGroup(index) {
        if (index <= 0) {
            bridge.setGroupFilter("")
            return
        }
        bridge.setGroupFilter(bridge.availableGroups[index - 1])
    }

    property Component menuPopupBackground: Rectangle {
        color: Theme.surfaceContainer
        radius: Theme.radiusMd
        border.color: Theme.outline
        border.width: 1
    }

    component ThemedToolButton: ToolButton {
        id: control

        implicitHeight: 34
        leftPadding: Theme.spacingMd
        rightPadding: Theme.spacingMd
        topPadding: Theme.spacingSm
        bottomPadding: Theme.spacingSm
        hoverEnabled: true

        contentItem: Text {
            text: control.text
            font: control.font
            color: Theme.textPrimary
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: control.down
                ? Theme.controlSurfacePressed
                : (control.hovered ? Theme.controlSurfaceHover : Theme.controlSurface)
            border.width: 1
            border.color: control.visualFocus ? Theme.controlBorderStrong : Theme.controlBorder
            opacity: control.enabled ? 1.0 : 0.6
        }
    }

    component ThemedTextField: TextField {
        id: control

        implicitHeight: 38
        color: Theme.textPrimary
        placeholderTextColor: Theme.textSecondary
        selectedTextColor: Theme.surface
        selectionColor: Theme.controlAccent
        leftPadding: Theme.spacingMd
        rightPadding: Theme.spacingMd
        topPadding: Theme.spacingSm
        bottomPadding: Theme.spacingSm

        background: Rectangle {
            radius: Theme.radiusSm
            color: control.enabled ? Theme.controlSurface : Theme.controlSurfaceDisabled
            border.width: 1
            border.color: control.activeFocus ? Theme.controlBorderStrong : Theme.controlBorder
        }
    }

    component ThemedItemDelegate: ItemDelegate {
        id: control

        leftPadding: Theme.spacingMd
        rightPadding: Theme.spacingMd
        topPadding: Theme.spacingSm
        bottomPadding: Theme.spacingSm
        hoverEnabled: true

        contentItem: Text {
            text: control.text
            font: control.font
            color: Theme.textPrimary
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: control.highlighted
                ? Theme.listItemCurrent
                : (control.hovered ? Theme.listItemHover : "transparent")
            border.width: control.visualFocus ? 1 : 0
            border.color: Theme.controlBorderStrong
        }
    }

    component ThemedComboBox: ComboBox {
        id: control

        implicitHeight: 38
        leftPadding: Theme.spacingMd
        rightPadding: Theme.spacingMd + indicator.width + Theme.spacingSm
        topPadding: Theme.spacingSm
        bottomPadding: Theme.spacingSm
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
                color: Theme.textPrimary
                elide: Text.ElideRight
                verticalAlignment: Text.AlignVCenter
            }

            background: Rectangle {
                radius: Theme.radiusSm
                color: optionDelegate.highlighted
                    ? Theme.listItemCurrent
                    : (optionDelegate.hovered ? Theme.listItemHover : "transparent")
            }
        }

        contentItem: TextInput {
            leftPadding: 0
            rightPadding: 0
            text: control.displayText
            font: control.font
            color: Theme.textPrimary
            verticalAlignment: Text.AlignVCenter
            readOnly: true
            selectByMouse: false
            cursorVisible: false
            selectionColor: "transparent"
            selectedTextColor: color
        }

        indicator: Canvas {
            x: control.width - width - Theme.spacingMd
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
                context.fillStyle = Theme.textSecondary
                context.fill()
            }
        }

        background: Rectangle {
            radius: Theme.radiusSm
            color: control.pressed
                ? Theme.controlSurfacePressed
                : (control.hovered ? Theme.controlSurfaceHover : Theme.controlSurface)
            border.width: 1
            border.color: control.visualFocus ? Theme.controlBorderStrong : Theme.controlBorder
        }

        popup: Popup {
            y: control.height + Theme.spacingXs
            width: control.width
            padding: Theme.spacingXs

            background: Rectangle {
                color: Theme.surfaceContainer
                radius: Theme.radiusMd
                border.width: 1
                border.color: Theme.outline
            }

            contentItem: ListView {
                clip: true
                implicitHeight: contentHeight
                model: control.delegateModel
                currentIndex: control.highlightedIndex
            }
        }
    }

    component ThemedSlider: Slider {
        id: control

        implicitHeight: 24

        background: Rectangle {
            x: control.leftPadding
            y: control.topPadding + (control.availableHeight - height) / 2
            width: control.availableWidth
            height: 4
            radius: 2
            color: Theme.controlAccentMuted

            Rectangle {
                width: control.visualPosition * parent.width
                height: parent.height
                radius: parent.radius
                color: Theme.controlAccent
            }
        }

        handle: Rectangle {
            x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
            y: control.topPadding + (control.availableHeight - height) / 2
            width: 14
            height: 14
            radius: 7
            color: Theme.textPrimary
            border.width: 1
            border.color: Theme.controlBorder
        }
    }

    Component {
        id: menuBarItemDelegate

        MenuBarItem {
            id: control

            implicitHeight: 32
            implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                                    implicitContentWidth + leftPadding + rightPadding)
            leftPadding: Theme.spacingMd
            rightPadding: Theme.spacingMd

            palette.windowText: Theme.textPrimary
            palette.buttonText: Theme.textPrimary
            palette.text: Theme.textPrimary
            palette.disabled.windowText: Theme.textDisabled
            palette.disabled.buttonText: Theme.textDisabled
            palette.disabled.text: Theme.textDisabled

            background: Rectangle {
                radius: Theme.radiusSm
                color: control.highlighted || control.down ? Theme.surfaceContainerHighest : "transparent"
            }
        }
    }

    Component {
        id: menuItemDelegate

        MenuItem {
            id: control

            implicitHeight: 32
            implicitWidth: Math.max(160,
                                    implicitBackgroundWidth + leftInset + rightInset,
                                    implicitContentWidth + leftPadding + rightPadding)
            leftPadding: Theme.spacingMd
            rightPadding: Theme.spacingMd

            palette.windowText: Theme.textPrimary
            palette.buttonText: Theme.textPrimary
            palette.text: Theme.textPrimary
            palette.highlightedText: Theme.textPrimary
            palette.disabled.windowText: Theme.textDisabled
            palette.disabled.buttonText: Theme.textDisabled
            palette.disabled.text: Theme.textDisabled

            arrow: Canvas {
                x: control.mirrored ? Theme.spacingSm : control.width - width - Theme.spacingSm
                y: (control.height - height) / 2
                width: 8
                height: 8
                visible: control.subMenu

                onPaint: {
                    const context = getContext("2d")
                    context.reset()
                    context.moveTo(0, 0)
                    context.lineTo(width, height / 2)
                    context.lineTo(0, height)
                    context.closePath()
                    context.fillStyle = Theme.textSecondary
                    context.fill()
                }
            }

            indicator: Item {
                implicitWidth: 0
                implicitHeight: 0
            }

            background: Rectangle {
                radius: Theme.radiusSm
                color: control.highlighted ? Theme.surfaceContainerHighest : "transparent"
            }
        }
    }

    Component {
        id: recentItemDelegate

        MenuItem {
            id: control
            required property int index
            required property var modelData

            implicitHeight: 32
            implicitWidth: Math.max(160,
                                    implicitBackgroundWidth + leftInset + rightInset,
                                    implicitContentWidth + leftPadding + rightPadding)
            leftPadding: Theme.spacingMd
            rightPadding: Theme.spacingMd
            text: modelData.label

            palette.windowText: Theme.textPrimary
            palette.buttonText: Theme.textPrimary
            palette.text: Theme.textPrimary
            palette.disabled.windowText: Theme.textDisabled
            palette.disabled.buttonText: Theme.textDisabled
            palette.disabled.text: Theme.textDisabled

            background: Rectangle {
                radius: Theme.radiusSm
                color: control.highlighted ? Theme.surfaceContainerHighest : "transparent"
            }

            onTriggered: bridge.openRecentAt(index)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        MenuBar {
            visible: !bridge.fullscreenActive
            leftPadding: Theme.spacingSm
            rightPadding: Theme.spacingSm
            topPadding: Theme.spacingXs
            bottomPadding: Theme.spacingXs
            spacing: Theme.spacingXs
            delegate: menuBarItemDelegate

            background: Rectangle {
                color: Theme.surfaceContainerHigh
            }

            Menu {
                title: qsTr("&File")
                popupType: root.menuPopupType
                delegate: menuItemDelegate
                background: root.menuPopupBackground.createObject(root)

                Action {
                    text: qsTr("Open &File...")
                    onTriggered: bridge.requestOpenFile()
                }
                Action {
                    text: qsTr("Open &Link...")
                    onTriggered: bridge.requestOpenUrl()
                }

                Menu {
                    id: recentMenu
                    title: qsTr("Open &Recent")
                    enabled: bridge.recentItems.length > 0
                    popupType: root.menuPopupType
                    delegate: recentItemDelegate
                    background: root.menuPopupBackground.createObject(root)

                    Repeater {
                        model: bridge.recentItems

                        delegate: MenuItem {
                            id: recentControl
                            required property int index
                            required property var modelData

                            implicitHeight: 32
                            implicitWidth: Math.max(160,
                                                    implicitBackgroundWidth + leftInset + rightInset,
                                                    implicitContentWidth + leftPadding + rightPadding)
                            leftPadding: Theme.spacingMd
                            rightPadding: Theme.spacingMd
                            text: modelData.label

                            palette.windowText: Theme.textPrimary
                            palette.buttonText: Theme.textPrimary
                            palette.text: Theme.textPrimary
                            palette.disabled.windowText: Theme.textDisabled
                            palette.disabled.buttonText: Theme.textDisabled
                            palette.disabled.text: Theme.textDisabled

                            background: Rectangle {
                                radius: Theme.radiusSm
                                color: recentControl.highlighted ? Theme.surfaceContainerHighest : "transparent"
                            }

                            onTriggered: bridge.openRecentAt(index)
                        }
                    }
                }
            }

            Menu {
                title: qsTr("&Settings")
                popupType: root.menuPopupType
                delegate: menuItemDelegate
                background: root.menuPopupBackground.createObject(root)

                Action {
                    text: qsTr("&Network Settings...")
                    onTriggered: bridge.requestNetworkSettings()
                }
            }

            Menu {
                title: qsTr("&View")
                popupType: root.menuPopupType
                delegate: menuItemDelegate
                background: root.menuPopupBackground.createObject(root)

                Action {
                    text: qsTr("Toggle &Full Screen")
                    onTriggered: bridge.toggleFullscreen()
                }
            }

            Menu {
                title: qsTr("&Help")
                popupType: root.menuPopupType
                delegate: menuItemDelegate
                background: root.menuPopupBackground.createObject(root)

                Action {
                    text: qsTr("&About ShaTV...")
                    onTriggered: bridge.requestAbout()
                }
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            Rectangle {
                visible: !bridge.fullscreenActive
                color: Theme.surfaceContainer
                implicitWidth: visible ? Theme.sidebarPreferredWidth : 0
                SplitView.preferredWidth: visible ? Theme.sidebarPreferredWidth : 0
                SplitView.minimumWidth: visible ? Theme.sidebarMinWidth : 0
                SplitView.maximumWidth: visible ? Theme.sidebarMaxWidth : 0

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.spacingMd
                    spacing: Theme.spacingSm

                    ThemedTextField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search channels")
                        text: bridge.searchText
                        onTextChanged: bridge.setSearchText(text)
                    }

                    ThemedComboBox {
                        Layout.fillWidth: true
                        model: root.groupItems
                        currentIndex: root.selectedGroupIndex
                        onActivated: index => root.applyGroup(index)
                    }

                    ListView {
                        id: channelListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: bridge.channelModel
                        spacing: Theme.spacingXs

                        delegate: ThemedItemDelegate {
                            required property int index
                            required property string channelName
                            required property bool isCurrent

                            width: ListView.view.width
                            text: channelName
                            highlighted: isCurrent
                            onClicked: bridge.activateChannelRow(index)
                        }
                    }
                }
            }

            Rectangle {
                color: Theme.surface
                SplitView.fillWidth: true
                SplitView.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: bridge.fullscreenActive ? 0 : Theme.spacingMd
                    spacing: bridge.fullscreenActive ? 0 : Theme.spacingMd

                    Rectangle {
                        id: videoSurface
                        color: "#000000"
                        radius: bridge.fullscreenActive ? 0 : Theme.radiusMd
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: Theme.videoMinHeight

                        MpvVideoItem {
                            id: playerVideoItem
                            objectName: "playerVideoItem"
                            anchors.fill: parent
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: !playerVideoItem.ready
                            text: root.effectiveStatusText
                            color: Theme.textPrimary
                        }

                        Rectangle {
                            visible: bridge.fullscreenActive
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: Theme.spacingMd
                            implicitHeight: fullscreenOverlayContent.implicitHeight + Theme.spacingMd * 2
                            radius: Theme.radiusMd
                            color: "#A8101B31"
                            border.width: 1
                            border.color: Theme.outline

                            ColumnLayout {
                                id: fullscreenOverlayContent
                                anchors.fill: parent
                                anchors.margins: Theme.spacingMd
                                spacing: Theme.spacingSm

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Theme.spacingMd

                                    ThemedToolButton {
                                        text: root.showPauseAction ? qsTr("Pause") : qsTr("Play")
                                        onClicked: bridge.requestPlayPause()
                                    }

                                    ThemedToolButton {
                                        text: qsTr("Stop")
                                        onClicked: bridge.requestStop()
                                    }

                                    ThemedToolButton {
                                        text: bridge.muted ? qsTr("Unmute") : qsTr("Mute")
                                        onClicked: bridge.toggleMute()
                                    }

                                    ThemedSlider {
                                        Layout.fillWidth: true
                                        from: 0
                                        to: 100
                                        stepSize: 1
                                        value: bridge.volume
                                        onMoved: bridge.setVolume(Math.round(value))
                                    }

                                    ThemedToolButton {
                                        text: qsTr("Exit Full Screen")
                                        onClicked: bridge.exitFullscreen()
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true

                                    Label {
                                        text: bridge.currentChannelName
                                        color: Theme.textPrimary
                                        visible: bridge.currentChannelName.length > 0
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: root.effectiveStatusText
                                        color: Theme.textPrimary
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: bridge.currentProgrammeText
                                        color: Theme.textPrimary
                                        visible: bridge.currentProgrammeText.length > 0
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: bridge.nextProgrammeText
                                        color: Theme.textSecondary
                                        visible: bridge.nextProgrammeText.length > 0
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        visible: !bridge.fullscreenActive
                        Layout.fillWidth: true
                        implicitHeight: Theme.controlPanelHeight
                        radius: Theme.radiusMd
                        color: Theme.surfaceBright

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingMd
                            spacing: Theme.spacingMd

                            ThemedToolButton {
                                text: root.showPauseAction ? qsTr("Pause") : qsTr("Play")
                                onClicked: bridge.requestPlayPause()
                            }

                            ThemedToolButton {
                                text: qsTr("Stop")
                                onClicked: bridge.requestStop()
                            }

                            ThemedToolButton {
                                text: bridge.muted ? qsTr("Unmute") : qsTr("Mute")
                                onClicked: bridge.toggleMute()
                            }

                            ThemedSlider {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                stepSize: 1
                                value: bridge.volume
                                onMoved: bridge.setVolume(Math.round(value))
                            }

                            ThemedToolButton {
                                text: qsTr("Full Screen")
                                onClicked: bridge.toggleFullscreen()
                            }
                        }
                    }

                    Rectangle {
                        visible: !bridge.fullscreenActive
                        Layout.fillWidth: true
                        implicitHeight: Theme.statusPanelHeight
                        radius: Theme.radiusMd
                        color: Theme.surfaceBright

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingMd
                            spacing: Theme.spacingMd

                            ColumnLayout {
                                Layout.fillWidth: true

                                Label {
                                    text: bridge.currentChannelName
                                    color: Theme.textPrimary
                                    visible: bridge.currentChannelName.length > 0
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: root.effectiveStatusText
                                    color: Theme.textPrimary
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: bridge.currentProgrammeText
                                    color: Theme.textPrimary
                                    visible: bridge.currentProgrammeText.length > 0
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: bridge.nextProgrammeText
                                    color: Theme.textSecondary
                                    visible: bridge.nextProgrammeText.length > 0
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
