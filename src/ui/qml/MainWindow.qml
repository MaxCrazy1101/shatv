pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "."

Rectangle {
    id: root
    objectName: "mainWindowRoot"
    color: Theme.windowBackground

    readonly property var bridge: mainWindowBridge
    readonly property var groupItems: [qsTr("All groups"), ...bridge.availableGroups]
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: bridge.fullscreenActive ? 0 : Theme.toolbarHeight
            visible: !bridge.fullscreenActive
            color: Theme.toolbarBackground

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.spacingMd
                anchors.rightMargin: Theme.spacingMd
                spacing: Theme.spacingSm

                ToolButton {
                    text: qsTr("Open File")
                    onClicked: bridge.requestOpenFile()
                }

                ToolButton {
                    text: qsTr("Open Link")
                    onClicked: bridge.requestOpenUrl()
                }

                MenuSeparator {}

                ToolButton {
                    text: qsTr("Recent")
                    enabled: bridge.recentItems.length > 0
                    onClicked: recentMenu.open()

                    Menu {
                        id: recentMenu

                        Repeater {
                            model: bridge.recentItems

                            delegate: MenuItem {
                                required property int index
                                required property var modelData

                                text: modelData.label
                                onTriggered: bridge.openRecentAt(index)
                            }
                        }
                    }
                }

                ToolButton {
                    text: qsTr("Network")
                    onClicked: bridge.requestNetworkSettings()
                }

                Item { Layout.fillWidth: true }

                ToolButton {
                    text: qsTr("About")
                    onClicked: bridge.requestAbout()
                }

                ToolButton {
                    text: qsTr("Full Screen")
                    onClicked: bridge.toggleFullscreen()
                }
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            Rectangle {
                visible: !bridge.fullscreenActive
                color: Theme.sidebarBackground
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
                color: Theme.contentBackground
                SplitView.fillWidth: true
                SplitView.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: bridge.fullscreenActive ? 0 : Theme.spacingMd
                    spacing: bridge.fullscreenActive ? 0 : Theme.spacingMd

                    Item {
                        id: videoHost
                        objectName: "videoHost"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: Theme.videoMinHeight
                    }

                    Rectangle {
                        visible: !bridge.fullscreenActive
                        Layout.fillWidth: true
                        implicitHeight: Theme.controlPanelHeight
                        radius: Theme.radiusMd
                        color: Theme.panelBackground

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Theme.spacingMd
                            spacing: Theme.spacingMd

                            ToolButton {
                                text: bridge.playing ? qsTr("Pause") : qsTr("Play")
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
                        color: Theme.panelBackground

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
                                    text: bridge.statusMessage.length > 0
                                          ? bridge.statusMessage
                                          : (bridge.playbackStateText.length > 0 ? bridge.playbackStateText : qsTr("Ready"))
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
