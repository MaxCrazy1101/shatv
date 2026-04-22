pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window
import ShaTV.Video 1.0
import "." as Shell
import "controls" as Controls
import "dialogs" as Dialogs

ApplicationWindow {
    id: root
    objectName: "mainWindowRoot"
    visible: true
    width: 1280
    height: 720
    minimumWidth: 960
    minimumHeight: 600
    title: qsTr("ShaTV")
    color: Shell.Theme.surfaceDim
    flags: Qt.Window | Qt.FramelessWindowHint

    QtObject {
        id: bridgeFallback

        property var channelModel: null
        property var availableGroups: []
        property string currentGroupFilter: ""
        property string searchText: ""
        property var recentItems: []
        property string statusMessage: ""
        property string currentChannelName: ""
        property string currentProgrammeText: ""
        property string nextProgrammeText: ""
        property string playbackStateText: ""
        property string playbackStateToken: "idle"
        property bool playing: false
        property bool muted: false
        property int volume: 50
        property string configuredUserAgent: ""
        property string configuredEpgUrl: ""
        property string appVersion: ""
        property string buildId: ""
        property string alertMessage: ""
        property bool alertVisible: false

        function activateChannelRow(row) {}
        function setSearchText(text) {}
        function setGroupFilter(group) {}
        function requestPlayPause() {}
        function requestStop() {}
        function toggleMute() {}
        function setVolume(volume) {}
        function submitOpenFile(path) {}
        function submitOpenUrl(urlText) {}
        function submitNetworkSettings(userAgent, epgUrl) {}
        function openRecentAt(index) {}
        function dismissAlert() {}
    }

    readonly property var bridge: (typeof appShellBridge !== "undefined" && appShellBridge !== null)
        ? appShellBridge
        : bridgeFallback
    readonly property bool fullscreenActive: visibility === Window.FullScreen
    readonly property bool canResizeWindow: !root.fullscreenActive && visibility !== Window.Maximized
    property int lastNonFullscreenVisibility: Window.Windowed
    property int fullscreenRestoreVisibility: Window.Windowed
    property bool fullscreenTransitionActive: false
    property bool normalGeometryRestorePending: false
    property bool hasSavedNormalGeometry: false
    property int savedNormalX: x
    property int savedNormalY: y
    property int savedNormalWidth: width
    property int savedNormalHeight: height
    readonly property var groupItems: [qsTr("All groups"), ...bridge.availableGroups]
    // 标题栏菜单不需要独立窗口，统一走 Popup.Item 才能稳定复用 overlay 事件拦截。
    readonly property int shellPopupType: Popup.Item
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

    function saveNormalGeometry() {
        if (root.visibility !== Window.Windowed || root.fullscreenActive || root.normalGeometryRestorePending) {
            return
        }

        root.savedNormalX = root.x
        root.savedNormalY = root.y
        root.savedNormalWidth = root.width
        root.savedNormalHeight = root.height
        root.hasSavedNormalGeometry = true
    }

    function restoreSavedNormalGeometry() {
        if (!root.hasSavedNormalGeometry) {
            return
        }

        root.x = root.savedNormalX
        root.y = root.savedNormalY
        root.width = root.savedNormalWidth
        root.height = root.savedNormalHeight
    }

    function maximizeWindow(preserveSavedGeometry = false) {
        if (!preserveSavedGeometry && root.visibility === Window.Windowed) {
            root.saveNormalGeometry()
        }
        root.lastNonFullscreenVisibility = Window.Maximized
        root.showMaximized()
    }

    function restoreWindow() {
        root.lastNonFullscreenVisibility = Window.Windowed
        root.normalGeometryRestorePending = root.hasSavedNormalGeometry
        root.showNormal()

        if (!root.normalGeometryRestorePending) {
            return
        }

        // 全屏/最大化切回普通窗口时，Qt 不一定还能保住原始普通尺寸，这里显式恢复。
        Qt.callLater(function() {
            root.restoreSavedNormalGeometry()
            root.normalGeometryRestorePending = false
            root.saveNormalGeometry()
        })
    }

    function enterFullscreen() {
        if (root.fullscreenActive) {
            return
        }

        if (root.visibility === Window.Windowed) {
            root.saveNormalGeometry()
        }

        root.fullscreenRestoreVisibility = root.visibility === Window.Maximized
            ? Window.Maximized
            : root.lastNonFullscreenVisibility
        root.fullscreenTransitionActive = true
        root.showFullScreen()
    }

    function toggleFullscreen() {
        if (root.fullscreenActive) {
            root.exitFullscreen()
            return
        }
        root.enterFullscreen()
    }

    function exitFullscreen() {
        if (!root.fullscreenActive) {
            return
        }

        const targetVisibility = root.fullscreenRestoreVisibility

        // FullScreen -> Maximized 在部分平台会先经历一次 Windowed 过渡，
        // 这里先显式退出全屏，再在下一轮事件循环恢复目标状态，避免中间态把最终状态冲掉。
        root.fullscreenTransitionActive = true
        root.showNormal()

        Qt.callLater(function() {
            if (targetVisibility === Window.Maximized) {
                root.maximizeWindow(true)
            } else {
                root.restoreWindow()
            }
            root.fullscreenTransitionActive = false
        })
    }

    onVisibilityChanged: function(newVisibility) {
        if (newVisibility === Window.FullScreen) {
            root.fullscreenTransitionActive = false
            return
        }

        if (newVisibility === Window.Hidden) {
            return
        }

        if (root.fullscreenTransitionActive) {
            return
        }

        root.lastNonFullscreenVisibility = newVisibility
    }

    onXChanged: root.saveNormalGeometry()
    onYChanged: root.saveNormalGeometry()
    onWidthChanged: root.saveNormalGeometry()
    onHeightChanged: root.saveNormalGeometry()

    Component.onCompleted: root.saveNormalGeometry()

    Shortcut {
        sequence: "F11"
        onActivated: root.toggleFullscreen()
    }

    Shortcut {
        sequence: "Escape"
        enabled: root.fullscreenActive
        onActivated: root.exitFullscreen()
    }

    function openFileMenu(button) {
        const overlay = Overlay.overlay
        const point = button.mapToItem(overlay, 0, button.height + Shell.Theme.spacingXs)
        settingsMenuPopup.close()
        fileMenuPopup.x = point.x
        fileMenuPopup.y = point.y
        if (fileMenuPopup.visible) {
            fileMenuPopup.close()
            return
        }
        fileMenuPopup.open()
    }

    function openSettingsMenu(button) {
        const overlay = Overlay.overlay
        const point = button.mapToItem(overlay, 0, button.height + Shell.Theme.spacingXs)
        fileMenuPopup.close()
        settingsMenuPopup.x = point.x
        settingsMenuPopup.y = point.y
        if (settingsMenuPopup.visible) {
            settingsMenuPopup.close()
            return
        }
        settingsMenuPopup.open()
    }

    function resizeCursor(edges) {
        if (edges === (Qt.LeftEdge | Qt.TopEdge) || edges === (Qt.RightEdge | Qt.BottomEdge)) {
            return Qt.SizeFDiagCursor
        }
        if (edges === (Qt.RightEdge | Qt.TopEdge) || edges === (Qt.LeftEdge | Qt.BottomEdge)) {
            return Qt.SizeBDiagCursor
        }
        if (edges === Qt.LeftEdge || edges === Qt.RightEdge) {
            return Qt.SizeHorCursor
        }
        if (edges === Qt.TopEdge || edges === Qt.BottomEdge) {
            return Qt.SizeVerCursor
        }
        return Qt.ArrowCursor
    }

    component HeaderActionButton: ToolButton {
        id: control

        implicitHeight: Shell.Theme.titleBarHeight - Shell.Theme.spacingSm
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
                ? Shell.Theme.surfaceContainerHighest
                : (control.hovered ? Shell.Theme.controlSurfaceHover : "transparent")
        }
    }

    component ResizeHandle: MouseArea {
        required property int edges

        acceptedButtons: Qt.LeftButton
        hoverEnabled: true
        preventStealing: true
        visible: root.canResizeWindow
        enabled: root.canResizeWindow
        cursorShape: root.resizeCursor(edges)

        onPressed: mouse => {
            if (mouse.button === Qt.LeftButton) {
                root.startSystemResize(edges)
            }
        }
    }

    component PopupActionItem: Controls.ThemedItemDelegate {
        implicitHeight: 32
        width: parent ? parent.width : implicitWidth
    }

    header: Rectangle {
        id: titleBar
        visible: !root.fullscreenActive
        implicitHeight: Shell.Theme.titleBarHeight
        color: Shell.Theme.surfaceContainerHigh
        border.width: 1
        border.color: Shell.Theme.outline

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Shell.Theme.spacingSm
            anchors.rightMargin: 0
            anchors.topMargin: 0
            anchors.bottomMargin: 0
            spacing: Shell.Theme.spacingSm

            Label {
                text: qsTr("ShaTV")
                color: Shell.Theme.textPrimary
                font.bold: true
                leftPadding: Shell.Theme.spacingSm
            }

            HeaderActionButton {
                id: fileButton
                text: qsTr("File")
                onClicked: root.openFileMenu(fileButton)
            }

            HeaderActionButton {
                id: settingsButton
                text: qsTr("Settings")
                onClicked: root.openSettingsMenu(settingsButton)
            }

            HeaderActionButton {
                text: qsTr("About")
                onClicked: aboutDialog.open()
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onDoubleTapped: {
                        if (root.visibility === Window.Maximized) {
                            root.restoreWindow()
                            return
                        }
                        root.maximizeWindow()
                    }
                }

                DragHandler {
                    target: null
                    onActiveChanged: {
                        if (active && !root.fullscreenActive) {
                            root.startSystemMove()
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    text: bridge.currentChannelName.length > 0 ? bridge.currentChannelName : root.title
                    color: Shell.Theme.textPrimary
                    elide: Text.ElideRight
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            Controls.WindowControlButton {
                text: qsTr("Full Screen")
                controlType: "fullscreen"
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Full Screen")
                onClicked: root.toggleFullscreen()
            }

            Controls.WindowControlButton {
                text: qsTr("Minimize")
                controlType: "minimize"
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Minimize")
                onClicked: root.showMinimized()
            }

            Controls.WindowControlButton {
                text: root.visibility === Window.Maximized ? qsTr("Restore") : qsTr("Maximize")
                controlType: root.visibility === Window.Maximized ? "restore" : "maximize"
                ToolTip.visible: hovered
                ToolTip.text: text
                onClicked: {
                    if (root.visibility === Window.Maximized) {
                        root.restoreWindow()
                        return
                    }
                    root.maximizeWindow()
                }
            }

            Controls.WindowControlButton {
                text: qsTr("Close")
                controlType: "close"
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Close")
                onClicked: root.close()
            }
        }

        Popup {
            id: fileMenuPopup
            parent: Overlay.overlay
            popupType: root.shellPopupType
            width: 260
            padding: Shell.Theme.spacingXs
            modal: true
            dim: false
            focus: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent

            background: Rectangle {
                color: Shell.Theme.surfaceContainer
                radius: Shell.Theme.radiusMd
                border.color: Shell.Theme.outline
                border.width: 1
            }

            contentItem: Column {
                spacing: Shell.Theme.spacingXs

                PopupActionItem {
                    text: qsTr("Open File...")
                    onClicked: {
                        fileMenuPopup.close()
                        fileDialog.open()
                    }
                }

                PopupActionItem {
                    text: qsTr("Open Link...")
                    onClicked: {
                        fileMenuPopup.close()
                        openUrlDialog.openWithValue("http://")
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 1
                    color: Shell.Theme.outline
                    opacity: 0.8
                }

                Label {
                    width: parent.width
                    leftPadding: Shell.Theme.spacingMd
                    rightPadding: Shell.Theme.spacingMd
                    topPadding: Shell.Theme.spacingXs
                    bottomPadding: Shell.Theme.spacingXs
                    text: qsTr("Open Recent")
                    color: bridge.recentItems.length > 0 ? Shell.Theme.textSecondary : Shell.Theme.textDisabled
                }

                Repeater {
                    model: bridge.recentItems

                    delegate: PopupActionItem {
                        required property int index
                        required property var modelData

                        text: modelData.label
                        onClicked: {
                            fileMenuPopup.close()
                            bridge.openRecentAt(index)
                        }
                    }
                }
            }
        }

        Popup {
            id: settingsMenuPopup
            parent: Overlay.overlay
            popupType: root.shellPopupType
            width: 260
            padding: Shell.Theme.spacingXs
            modal: true
            dim: false
            focus: true
            closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside | Popup.CloseOnPressOutsideParent

            background: Rectangle {
                color: Shell.Theme.surfaceContainer
                radius: Shell.Theme.radiusMd
                border.color: Shell.Theme.outline
                border.width: 1
            }

            contentItem: Column {
                PopupActionItem {
                    text: qsTr("Network Settings")
                    onClicked: {
                        settingsMenuPopup.close()
                        settingsDialog.openWithValues(bridge.configuredUserAgent, bridge.configuredEpgUrl)
                    }
                }
            }
        }

        ResizeHandle {
            edges: Qt.TopEdge
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 6
        }

        ResizeHandle {
            edges: Qt.LeftEdge
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 6
        }

        ResizeHandle {
            edges: Qt.RightEdge
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 6
        }

        ResizeHandle {
            edges: Qt.LeftEdge | Qt.TopEdge
            anchors.left: parent.left
            anchors.top: parent.top
            width: 10
            height: 10
        }

        ResizeHandle {
            edges: Qt.RightEdge | Qt.TopEdge
            anchors.right: parent.right
            anchors.top: parent.top
            width: 10
            height: 10
        }
    }

    Dialogs.AboutDialog {
        id: aboutDialog
        appVersion: bridge.appVersion
        buildId: bridge.buildId
    }

    Dialogs.NetworkSettingsDialog {
        id: settingsDialog
        onSubmitted: (userAgent, epgUrl) => bridge.submitNetworkSettings(userAgent, epgUrl)
    }

    Dialogs.OpenUrlDialog {
        id: openUrlDialog
        onSubmitted: urlText => bridge.submitOpenUrl(urlText)
    }

    FileDialog {
        id: fileDialog
        title: qsTr("Open File")
        options: FileDialog.DontUseNativeDialog
        nameFilters: [
            qsTr("Media Files (*.m3u *.m3u8 *.mp4 *.mkv *.ts *.mov *.webm *.mp3 *.flac)"),
            qsTr("All Files (*)")
        ]
        onAccepted: bridge.submitOpenFile(selectedFile)
    }

    Dialog {
        id: alertDialog
        parent: Overlay.overlay
        x: Math.round(Math.max(0, ((parent ? parent.width : 0) - width) / 2))
        y: Math.round(Math.max(0, ((parent ? parent.height : 0) - height) / 2))
        modal: true
        focus: true
        width: 420
        padding: Shell.Theme.spacingMd
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        title: qsTr("ShaTV")

        background: Rectangle {
            radius: Shell.Theme.radiusMd
            color: Shell.Theme.surfaceContainer
            border.width: 1
            border.color: Shell.Theme.outline
        }

        contentItem: Label {
            text: bridge.alertMessage
            color: Shell.Theme.textPrimary
            wrapMode: Text.WordWrap
        }

        footer: Item {
            implicitHeight: 56

            RowLayout {
                anchors.fill: parent
                anchors.margins: Shell.Theme.spacingMd

                Item {
                    Layout.fillWidth: true
                }

                Controls.ThemedToolButton {
                    text: qsTr("Close")
                    onClicked: {
                        alertDialog.close()
                        bridge.dismissAlert()
                    }
                }
            }
        }

        onClosed: bridge.dismissAlert()
    }

    Connections {
        target: bridge

        function onAlertVisibleChanged() {
            if (bridge.alertVisible) {
                alertDialog.open()
                return
            }
            alertDialog.close()
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            Rectangle {
                visible: !root.fullscreenActive
                color: Shell.Theme.surfaceContainer
                implicitWidth: visible ? Shell.Theme.sidebarPreferredWidth : 0
                SplitView.preferredWidth: visible ? Shell.Theme.sidebarPreferredWidth : 0
                SplitView.minimumWidth: visible ? Shell.Theme.sidebarMinWidth : 0
                SplitView.maximumWidth: visible ? Shell.Theme.sidebarMaxWidth : 0

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Shell.Theme.spacingMd
                    spacing: Shell.Theme.spacingSm

                    Controls.ThemedTextField {
                        id: searchField
                        Layout.fillWidth: true
                        placeholderText: qsTr("Search channels")
                        text: bridge.searchText
                        onTextChanged: bridge.setSearchText(text)
                    }

                    Controls.ThemedComboBox {
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
                        spacing: Shell.Theme.spacingXs

                        delegate: Controls.ThemedItemDelegate {
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
                color: Shell.Theme.surface
                SplitView.fillWidth: true
                SplitView.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: root.fullscreenActive ? 0 : Shell.Theme.spacingMd
                    spacing: root.fullscreenActive ? 0 : Shell.Theme.spacingMd

                    Rectangle {
                        id: videoSurface
                        color: "#000000"
                        radius: root.fullscreenActive ? 0 : Shell.Theme.radiusMd
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: Shell.Theme.videoMinHeight

                        MpvVideoItem {
                            id: playerVideoItem
                            objectName: "playerVideoItem"
                            anchors.fill: parent
                        }

                        Label {
                            anchors.centerIn: parent
                            visible: !playerVideoItem.ready
                            text: root.effectiveStatusText
                            color: Shell.Theme.textPrimary
                        }

                        Rectangle {
                            visible: root.fullscreenActive
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: Shell.Theme.spacingMd
                            implicitHeight: fullscreenOverlayContent.implicitHeight + Shell.Theme.spacingMd * 2
                            radius: Shell.Theme.radiusMd
                            color: "#A8101B31"
                            border.width: 1
                            border.color: Shell.Theme.outline

                            ColumnLayout {
                                id: fullscreenOverlayContent
                                anchors.fill: parent
                                anchors.margins: Shell.Theme.spacingMd
                                spacing: Shell.Theme.spacingSm

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Shell.Theme.spacingMd

                                    Controls.ThemedToolButton {
                                        text: root.showPauseAction ? qsTr("Pause") : qsTr("Play")
                                        onClicked: bridge.requestPlayPause()
                                    }

                                    Controls.ThemedToolButton {
                                        text: qsTr("Stop")
                                        onClicked: bridge.requestStop()
                                    }

                                    Controls.ThemedToolButton {
                                        text: bridge.muted ? qsTr("Unmute") : qsTr("Mute")
                                        onClicked: bridge.toggleMute()
                                    }

                                    Controls.ThemedSlider {
                                        Layout.fillWidth: true
                                        from: 0
                                        to: 100
                                        stepSize: 1
                                        value: bridge.volume
                                        onMoved: bridge.setVolume(Math.round(value))
                                    }

                                    Controls.ThemedToolButton {
                                        text: qsTr("Exit Full Screen")
                                        onClicked: root.exitFullscreen()
                                    }
                                }

                                ColumnLayout {
                                    Layout.fillWidth: true

                                    Label {
                                        text: bridge.currentChannelName
                                        color: Shell.Theme.textPrimary
                                        visible: bridge.currentChannelName.length > 0
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: root.effectiveStatusText
                                        color: Shell.Theme.textPrimary
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: bridge.currentProgrammeText
                                        color: Shell.Theme.textPrimary
                                        visible: bridge.currentProgrammeText.length > 0
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }

                                    Label {
                                        text: bridge.nextProgrammeText
                                        color: Shell.Theme.textSecondary
                                        visible: bridge.nextProgrammeText.length > 0
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        visible: !root.fullscreenActive
                        Layout.fillWidth: true
                        implicitHeight: Shell.Theme.controlPanelHeight
                        radius: Shell.Theme.radiusMd
                        color: Shell.Theme.surfaceBright

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Shell.Theme.spacingMd
                            spacing: Shell.Theme.spacingMd

                            Controls.ThemedToolButton {
                                text: root.showPauseAction ? qsTr("Pause") : qsTr("Play")
                                onClicked: bridge.requestPlayPause()
                            }

                            Controls.ThemedToolButton {
                                text: qsTr("Stop")
                                onClicked: bridge.requestStop()
                            }

                            Controls.ThemedToolButton {
                                text: bridge.muted ? qsTr("Unmute") : qsTr("Mute")
                                onClicked: bridge.toggleMute()
                            }

                            Controls.ThemedSlider {
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                stepSize: 1
                                value: bridge.volume
                                onMoved: bridge.setVolume(Math.round(value))
                            }

                            Controls.ThemedToolButton {
                                text: qsTr("Full Screen")
                                onClicked: root.toggleFullscreen()
                            }
                        }
                    }

                    Rectangle {
                        visible: !root.fullscreenActive
                        Layout.fillWidth: true
                        implicitHeight: Shell.Theme.statusPanelHeight
                        radius: Shell.Theme.radiusMd
                        color: Shell.Theme.surfaceBright

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: Shell.Theme.spacingMd
                            spacing: Shell.Theme.spacingMd

                            ColumnLayout {
                                Layout.fillWidth: true

                                Label {
                                    text: bridge.currentChannelName
                                    color: Shell.Theme.textPrimary
                                    visible: bridge.currentChannelName.length > 0
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: root.effectiveStatusText
                                    color: Shell.Theme.textPrimary
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: bridge.currentProgrammeText
                                    color: Shell.Theme.textPrimary
                                    visible: bridge.currentProgrammeText.length > 0
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: bridge.nextProgrammeText
                                    color: Shell.Theme.textSecondary
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

    ResizeHandle {
        edges: Qt.LeftEdge
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
    }

    ResizeHandle {
        edges: Qt.RightEdge
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 6
    }

    ResizeHandle {
        edges: Qt.BottomEdge
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 6
    }

    ResizeHandle {
        edges: Qt.LeftEdge | Qt.BottomEdge
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        width: 10
        height: 10
    }

    ResizeHandle {
        edges: Qt.RightEdge | Qt.BottomEdge
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        width: 10
        height: 10
    }
}
