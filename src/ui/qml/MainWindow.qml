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
    readonly property int menuPopupType: Qt.platform.pluginName !== "wayland" ? Popup.Window : Popup.Item
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

    Component {
        id: menuBarItemDelegate

        MenuBarItem {
            id: control

            implicitHeight: 32
            leftPadding: Theme.spacingMd
            rightPadding: Theme.spacingMd

            palette.windowText: Theme.textPrimary
            palette.disabled.windowText: Theme.textDisabled

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
            leftPadding: Theme.spacingMd
            rightPadding: Theme.spacingMd

            palette.windowText: Theme.textPrimary
            palette.disabled.windowText: Theme.textDisabled

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
            leftPadding: Theme.spacingMd
            rightPadding: Theme.spacingMd
            text: modelData.label

            palette.windowText: Theme.textPrimary
            palette.disabled.windowText: Theme.textDisabled

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
                            leftPadding: Theme.spacingMd
                            rightPadding: Theme.spacingMd
                            text: modelData.label

                            palette.windowText: Theme.textPrimary
                            palette.disabled.windowText: Theme.textDisabled

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

                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search channels")
                        text: bridge.searchText
                        onTextChanged: bridge.setSearchText(text)
                    }

                    ComboBox {
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

                        delegate: ItemDelegate {
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

                                    ToolButton {
                                        text: root.showPauseAction ? qsTr("Pause") : qsTr("Play")
                                        onClicked: bridge.requestPlayPause()
                                    }

                                    ToolButton {
                                        text: qsTr("Stop")
                                        onClicked: bridge.requestStop()
                                    }

                                    ToolButton {
                                        text: bridge.muted ? qsTr("Unmute") : qsTr("Mute")
                                        onClicked: bridge.toggleMute()
                                    }

                                    Slider {
                                        Layout.fillWidth: true
                                        from: 0
                                        to: 100
                                        stepSize: 1
                                        value: bridge.volume
                                        onMoved: bridge.setVolume(Math.round(value))
                                    }

                                    ToolButton {
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

                            ToolButton {
                                text: root.showPauseAction ? qsTr("Pause") : qsTr("Play")
                                onClicked: bridge.requestPlayPause()
                            }

                            ToolButton {
                                text: qsTr("Stop")
                                onClicked: bridge.requestStop()
                            }

                            ToolButton {
                                text: bridge.muted ? qsTr("Unmute") : qsTr("Mute")
                                onClicked: bridge.toggleMute()
                            }

                            Slider {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                stepSize: 1
                                value: bridge.volume
                                onMoved: bridge.setVolume(Math.round(value))
                            }

                            ToolButton {
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
                            }
                        }
                    }
                }
            }
        }
    }
}
