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
    flags: Qt.Window | Qt.FramelessWindowHint | (root.alwaysOnTop ? Qt.WindowStaysOnTopHint : 0)

    QtObject {
        id: bridgeFallback

        property var channelModel: null
        property var availableGroups: []
        property string currentGroupFilter: ""
        property string searchText: ""
        property var recentItems: []
        property string statusMessage: ""
        property string currentChannelName: ""
        property bool hasProgrammeInfo: false
        property string currentProgrammeTitle: ""
        property string currentProgrammeTimeText: ""
        property real currentProgrammeProgress: 0
        property bool currentProgrammeProgressAvailable: false
        property string nextProgrammeTitle: ""
        property string nextProgrammeTimeText: ""
        property string playbackStateText: ""
        property string playbackStateToken: "idle"
        property bool playing: false
        property bool muted: false
        property int volume: 50
        property string configuredUserAgent: ""
        property string configuredEpgUrl: ""
        property string appVersion: ""
        property string buildId: ""
        property string logFilePath: ""
        property string logsDirectoryPath: ""
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
        function openLogsFolder() {}
        function copyDiagnosticsToClipboard() {}
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
    property bool alwaysOnTop: false
    property bool sidebarVisible: true
    property bool statusPanelVisible: false
    property bool fullscreenVolumePopupOpen: false
    property bool normalVolumePopupOpen: false
    property int savedNormalX: x
    property int savedNormalY: y
    property int savedNormalWidth: width
    property int savedNormalHeight: height
    readonly property var groupItems: [qsTr("All groups"), ...bridge.availableGroups]
    readonly property string effectiveStatusText: bridge.statusMessage.length > 0
        ? bridge.statusMessage
        : (bridge.playbackStateText.length > 0 ? bridge.playbackStateText : qsTr("Ready"))
    readonly property bool showPauseAction: bridge.playbackStateToken === "playing"
        || bridge.playbackStateToken === "loading"
        || bridge.playbackStateToken === "buffering"
        || bridge.playbackStateToken === "retrying"
    readonly property bool showPlaybackBusyOverlay: bridge.playbackStateToken === "loading"
        || bridge.playbackStateToken === "buffering"
        || bridge.playbackStateToken === "retrying"
    readonly property string playbackBusyTitle: bridge.playbackStateToken === "retrying"
        ? qsTr("Reconnecting")
        : (bridge.playbackStateToken === "buffering" ? qsTr("Buffering") : qsTr("Loading"))
    readonly property bool currentProgrammeVisible: bridge.currentProgrammeTitle.length > 0
        || bridge.currentProgrammeTimeText.length > 0
    readonly property bool nextProgrammeVisible: bridge.nextProgrammeTitle.length > 0
        || bridge.nextProgrammeTimeText.length > 0
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
        if (newVisibility === Window.Hidden) {
            return
        }

        if (root.fullscreenTransitionActive) {
            return
        }

        // Only update the non-fullscreen baseline when the transition is not active.
        if (newVisibility !== Window.FullScreen) {
            root.lastNonFullscreenVisibility = newVisibility
        }
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

    Timer {
        id: recentMenuCloseTimer
        interval: 160
        repeat: false
        onTriggered: root.closeRecentMenuIfIdle()
    }

    Timer {
        id: fullscreenVolumeCloseTimer
        interval: 160
        repeat: false
        onTriggered: root.closeFullscreenVolumePopupIfIdle()
    }

    Timer {
        id: normalVolumeCloseTimer
        interval: 160
        repeat: false
        onTriggered: root.closeNormalVolumePopupIfIdle()
    }

    function positionRecentMenu(button) {
        const overlay = Overlay.overlay
        const point = button.mapToItem(overlay, 0, button.height + Shell.Theme.spacingXs)
        recentMenuPopup.x = point.x
        recentMenuPopup.y = point.y
    }

    function openRecentMenu(button) {
        if (bridge.recentItems.length === 0) {
            return
        }

        recentMenuCloseTimer.stop()
        root.positionRecentMenu(button)
        if (!recentMenuPopup.visible) {
            recentMenuPopup.open()
        }
    }

    function scheduleRecentMenuClose() {
        if (!recentMenuPopup.visible) {
            return
        }

        recentMenuCloseTimer.restart()
    }

    function closeRecentMenuIfIdle() {
        if (recentButton.hovered || recentMenuHoverHandler.hovered) {
            return
        }

        recentMenuPopup.close()
    }

    function openFullscreenVolumePopup() {
        fullscreenVolumeCloseTimer.stop()
        root.fullscreenVolumePopupOpen = true
    }

    function scheduleFullscreenVolumePopupClose() {
        if (!root.fullscreenVolumePopupOpen) {
            return
        }

        fullscreenVolumeCloseTimer.restart()
    }

    function closeFullscreenVolumePopupIfIdle() {
        if (volumeHoverHandler.hovered || fullscreenVolumePopupHoverHandler.hovered || fullscreenVolumeSlider.pressed) {
            return
        }

        root.fullscreenVolumePopupOpen = false
    }

    function openNormalVolumePopup() {
        normalVolumeCloseTimer.stop()
        root.normalVolumePopupOpen = true
    }

    function scheduleNormalVolumePopupClose() {
        if (!root.normalVolumePopupOpen) {
            return
        }

        normalVolumeCloseTimer.restart()
    }

    function closeNormalVolumePopupIfIdle() {
        if (normalVolumeHoverHandler.hovered || normalVolumePopupHoverHandler.hovered || normalVolumeSlider.pressed) {
            return
        }

        root.normalVolumePopupOpen = false
    }

    function setVideoAspectRatioMode(mode) {
        ffmpegVideoItem.aspectRatioMode = mode
    }

    function openVideoContextMenu(sourceItem, mouse) {
        const point = sourceItem.mapToItem(Overlay.overlay, mouse.x, mouse.y)
        videoContextMenu.x = point.x
        videoContextMenu.y = point.y
        videoContextMenu.open()
    }

    function toggleAlwaysOnTop() {
        const targetVisibility = root.visibility
        root.alwaysOnTop = !root.alwaysOnTop

        if (!root.visible) {
            return
        }

        // 某些平台在运行时切换 WindowStaysOnTopHint 后，需要显式重新应用当前窗口状态。
        Qt.callLater(function() {
            if (targetVisibility === Window.FullScreen) {
                root.showFullScreen()
            } else if (targetVisibility === Window.Maximized) {
                root.showMaximized()
            } else {
                root.showNormal()
            }
            root.raise()
            root.requestActivate()
        })
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

    component HeaderIconButton: ToolButton {
        id: control
        required property string iconType
        property string toolTipText: control.text
        readonly property string symbolName: {
            if (control.iconType === "file") {
                return "file_open"
            }
            if (control.iconType === "link") {
                return "link"
            }
            if (control.iconType === "recent") {
                return "history"
            }
            if (control.iconType === "pin") {
                return "keep"
            }
            if (control.iconType === "info") {
                return "info"
            }
            if (control.iconType === "list") {
                return "list"
            }
            if (control.iconType === "settings") {
                return "settings"
            }
            return "settings"
        }
        readonly property bool symbolFilled: {
            return (control.iconType === "pin" && control.checked)
                || (control.iconType === "info" && control.checked)
                || (control.iconType === "list" && control.checked)
        }

        implicitWidth: Shell.Theme.titleBarHeight - Shell.Theme.spacingSm
        implicitHeight: Shell.Theme.titleBarHeight - Shell.Theme.spacingSm
        leftPadding: Shell.Theme.spacingSm
        rightPadding: Shell.Theme.spacingSm
        topPadding: Shell.Theme.spacingSm
        bottomPadding: Shell.Theme.spacingSm
        hoverEnabled: true

        contentItem: Controls.MaterialSymbolIcon {
            anchors.centerIn: parent
            symbolName: control.symbolName
            filled: control.symbolFilled
            iconSize: 20
        }

        background: Rectangle {
            radius: Shell.Theme.radiusSm
            color: control.down
                ? Shell.Theme.surfaceContainerHighest
                : (control.checked
                    ? Shell.Theme.controlSurfacePressed
                    : (control.hovered ? Shell.Theme.controlSurfaceHover : "transparent"))
            border.width: (control.visualFocus || control.checked) ? 1 : 0
            border.color: control.checked ? Shell.Theme.controlBorderStrong : Shell.Theme.focusRing
        }
    }

    component PlaybackIconButton: Controls.ThemedToolButton {
        id: control
        required property string symbolName
        property bool symbolFilled: false
        property string toolTipText: control.text

        implicitWidth: 38
        implicitHeight: 38
        leftPadding: Shell.Theme.spacingSm
        rightPadding: Shell.Theme.spacingSm
        topPadding: Shell.Theme.spacingSm
        bottomPadding: Shell.Theme.spacingSm

        ToolTip.visible: hovered
        ToolTip.text: control.toolTipText

        contentItem: Controls.MaterialSymbolIcon {
            anchors.centerIn: parent
            symbolName: control.symbolName
            filled: control.symbolFilled
            iconSize: 22
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

            HeaderIconButton {
                text: qsTr("Open File")
                toolTipText: qsTr("Open File...")
                iconType: "file"
                ToolTip.visible: hovered
                ToolTip.text: toolTipText
                onClicked: fileDialog.open()
            }

            HeaderIconButton {
                text: qsTr("Open Link")
                toolTipText: qsTr("Open Link...")
                iconType: "link"
                ToolTip.visible: hovered
                ToolTip.text: toolTipText
                onClicked: openUrlDialog.openWithValue("http://")
            }

            HeaderIconButton {
                id: recentButton
                text: qsTr("Open Recent")
                iconType: "recent"
                enabled: bridge.recentItems.length > 0
                ToolTip.visible: hovered && !recentMenuPopup.visible
                ToolTip.text: toolTipText
                onHoveredChanged: {
                    if (hovered) {
                        root.openRecentMenu(recentButton)
                        return
                    }

                    root.scheduleRecentMenuClose()
                }
                onClicked: {
                    if (recentMenuPopup.visible) {
                        recentMenuPopup.close()
                        return
                    }

                    root.openRecentMenu(recentButton)
                }
            }

            HeaderIconButton {
                text: qsTr("Settings")
                iconType: "settings"
                ToolTip.visible: hovered
                ToolTip.text: toolTipText
                onClicked: settingsWindow.openWithValues(bridge.configuredUserAgent, bridge.configuredEpgUrl)
            }

            Connections {
                target: bridge

                function onRecentItemsChanged() {
                    if (bridge.recentItems.length === 0) {
                        recentMenuPopup.close()
                    }
                }
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

            HeaderIconButton {
                text: qsTr("Toggle Status Panel")
                iconType: "info"
                checkable: true
                checked: root.statusPanelVisible
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Toggle Status Panel")
                onClicked: root.statusPanelVisible = !root.statusPanelVisible
            }

            HeaderIconButton {
                text: qsTr("Always on Top")
                iconType: "pin"
                checkable: true
                checked: root.alwaysOnTop
                ToolTip.visible: hovered
                ToolTip.text: toolTipText
                onClicked: root.toggleAlwaysOnTop()
            }

            HeaderIconButton {
                text: qsTr("Toggle Channel List")
                iconType: "list"
                checkable: true
                checked: root.sidebarVisible
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Toggle Channel List")
                onClicked: root.sidebarVisible = !root.sidebarVisible
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
            id: recentMenuPopup
            parent: Overlay.overlay
            popupType: Popup.Item
            width: 280
            padding: Shell.Theme.spacingXs
            modal: false
            dim: false
            focus: true
            closePolicy: Popup.CloseOnEscape
            onClosed: recentMenuCloseTimer.stop()

            background: Rectangle {
                color: Shell.Theme.surfaceContainer
                radius: Shell.Theme.radiusMd
                border.color: Shell.Theme.outline
                border.width: 1
            }

            contentItem: Column {
                spacing: Shell.Theme.spacingXs

                HoverHandler {
                    id: recentMenuHoverHandler
                    onHoveredChanged: {
                        if (hovered) {
                            recentMenuCloseTimer.stop()
                            return
                        }

                        root.scheduleRecentMenuClose()
                    }
                }

                Label {
                    width: parent.width
                    leftPadding: Shell.Theme.spacingMd
                    rightPadding: Shell.Theme.spacingMd
                    topPadding: Shell.Theme.spacingXs
                    bottomPadding: Shell.Theme.spacingXs
                    text: qsTr("Open Recent")
                    color: Shell.Theme.textSecondary
                }

                Repeater {
                    model: bridge.recentItems

                    delegate: Controls.ThemedItemDelegate {
                        required property int index
                        required property var modelData

                        width: parent ? parent.width : implicitWidth
                        text: modelData.label
                        onClicked: {
                            recentMenuPopup.close()
                            bridge.openRecentAt(index)
                        }
                    }
                }
            }
        }

        ButtonGroup {
            id: aspectRatioModeGroup
            exclusive: true
        }

        Menu {
            id: videoContextMenu
            parent: Overlay.overlay
            popupType: Popup.Item
            modal: false
            dim: false

            Menu {
                title: qsTr("Aspect Ratio")

                MenuItem {
                    text: qsTr("Preserve (Fit)")
                    checkable: true
                    checked: ffmpegVideoItem.aspectRatioMode === VideoPresenterItem.PreserveAspectRatio
                    ButtonGroup.group: aspectRatioModeGroup
                    onTriggered: root.setVideoAspectRatioMode(VideoPresenterItem.PreserveAspectRatio)
                }
                MenuItem {
                    text: qsTr("Stretch")
                    checkable: true
                    checked: ffmpegVideoItem.aspectRatioMode === VideoPresenterItem.StretchToFill
                    ButtonGroup.group: aspectRatioModeGroup
                    onTriggered: root.setVideoAspectRatioMode(VideoPresenterItem.StretchToFill)
                }
                MenuItem {
                    text: qsTr("Fill (Crop)")
                    checkable: true
                    checked: ffmpegVideoItem.aspectRatioMode === VideoPresenterItem.CropToFill
                    ButtonGroup.group: aspectRatioModeGroup
                    onTriggered: root.setVideoAspectRatioMode(VideoPresenterItem.CropToFill)
                }
                MenuItem {
                    text: qsTr("Native (1:1)")
                    checkable: true
                    checked: ffmpegVideoItem.aspectRatioMode === VideoPresenterItem.NativeSize
                    ButtonGroup.group: aspectRatioModeGroup
                    onTriggered: root.setVideoAspectRatioMode(VideoPresenterItem.NativeSize)
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

    Dialogs.SettingsWindow {
        id: settingsWindow
        transientParent: root
        appVersion: bridge.appVersion
        buildId: bridge.buildId
        logFilePath: bridge.logFilePath
        logsDirectoryPath: bridge.logsDirectoryPath
        onSubmitted: (userAgent, epgUrl) => bridge.submitNetworkSettings(userAgent, epgUrl)
        onOpenLogsFolderRequested: bridge.openLogsFolder()
        onCopyDiagnosticsRequested: bridge.copyDiagnosticsToClipboard()
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
                visible: root.sidebarVisible && !root.fullscreenActive
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

                        VideoPresenterItem {
                            id: ffmpegVideoItem
                            objectName: "ffmpegVideoItem"
                            anchors.fill: parent
                            visible: ready
                        }

                        // Right-click context menu for aspect ratio
                        MouseArea {
                            id: videoContextMouseArea
                            anchors.fill: parent
                            acceptedButtons: Qt.RightButton
                            onClicked: function(mouse) {
                                if (mouse.button === Qt.RightButton) {
                                    root.openVideoContextMenu(videoContextMouseArea, mouse)
                                }
                            }
                        }

                        // OSD status panel — semi-transparent overlay over video area
                        Rectangle {
                            visible: root.statusPanelVisible && !root.fullscreenActive
                            z: 15
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.margins: Shell.Theme.spacingSm
                            implicitHeight: osdStatusContent.implicitHeight + Shell.Theme.spacingSm * 2
                            opacity: 0.82
                            radius: Shell.Theme.radiusSm
                            color: Shell.Theme.surfaceContainer

                            ColumnLayout {
                                id: osdStatusContent
                                anchors.fill: parent
                                anchors.margins: Shell.Theme.spacingSm
                                spacing: Shell.Theme.spacingXs

                                Label {
                                    text: bridge.currentChannelName
                                    color: Shell.Theme.textPrimary
                                    visible: bridge.currentChannelName.length > 0
                                    font.pixelSize: 13
                                    font.bold: true
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                Label {
                                    text: root.effectiveStatusText
                                    color: Shell.Theme.textSecondary
                                    font.pixelSize: 12
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                    visible: !root.showPlaybackBusyOverlay
                                }

                                RowLayout {
                                    visible: root.currentProgrammeVisible
                                    Layout.fillWidth: true
                                    spacing: Shell.Theme.spacingSm

                                    Label {
                                        text: qsTr("Now")
                                        color: Shell.Theme.textPrimary
                                        font.pixelSize: 11
                                        font.bold: true
                                        Layout.preferredWidth: Shell.Theme.statusProgrammeLabelWidth
                                    }

                                    Label {
                                        text: bridge.currentProgrammeTimeText
                                        color: Shell.Theme.textSecondary
                                        font.pixelSize: 11
                                        Layout.preferredWidth: Shell.Theme.statusProgrammeTimeWidth
                                        elide: Text.ElideRight
                                        visible: bridge.currentProgrammeTimeText.length > 0
                                    }

                                    Label {
                                        text: bridge.currentProgrammeTitle
                                        color: Shell.Theme.textSecondary
                                        font.pixelSize: 11
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        visible: bridge.currentProgrammeTitle.length > 0
                                    }
                                }

                                Rectangle {
                                    visible: bridge.currentProgrammeProgressAvailable
                                    Layout.fillWidth: true
                                    Layout.leftMargin: Shell.Theme.statusProgrammeLabelWidth + Shell.Theme.spacingSm
                                    implicitHeight: Shell.Theme.statusProgrammeProgressHeight
                                    radius: Shell.Theme.statusProgrammeProgressHeight / 2
                                    color: Shell.Theme.outline

                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: parent.width * Math.max(0, Math.min(1, bridge.currentProgrammeProgress))
                                        radius: parent.radius
                                        color: Shell.Theme.focusRing
                                    }
                                }

                                RowLayout {
                                    visible: root.nextProgrammeVisible
                                    Layout.fillWidth: true
                                    spacing: Shell.Theme.spacingSm

                                    Label {
                                        text: qsTr("Next")
                                        color: Shell.Theme.textDisabled
                                        font.pixelSize: 11
                                        font.bold: true
                                        Layout.preferredWidth: Shell.Theme.statusProgrammeLabelWidth
                                    }

                                    Label {
                                        text: bridge.nextProgrammeTimeText
                                        color: Shell.Theme.textDisabled
                                        font.pixelSize: 11
                                        Layout.preferredWidth: Shell.Theme.statusProgrammeTimeWidth
                                        elide: Text.ElideRight
                                        visible: bridge.nextProgrammeTimeText.length > 0
                                    }

                                    Label {
                                        text: bridge.nextProgrammeTitle
                                        color: Shell.Theme.textDisabled
                                        font.pixelSize: 11
                                        Layout.fillWidth: true
                                        elide: Text.ElideRight
                                        visible: bridge.nextProgrammeTitle.length > 0
                                    }
                                }
                            }
                        }

                        Rectangle {
                            id: playbackBusyOverlay
                            anchors.fill: parent
                            visible: opacity > 0
                            opacity: root.showPlaybackBusyOverlay ? 1 : 0
                            z: 10
                            color: Shell.Theme.playbackBusyOverlay

                            Behavior on opacity {
                                NumberAnimation {
                                    duration: 160
                                    easing.type: Easing.OutCubic
                                }
                            }

                            ColumnLayout {
                                anchors.centerIn: parent
                                spacing: Shell.Theme.spacingMd

                                BusyIndicator {
                                    Layout.alignment: Qt.AlignHCenter
                                    Layout.preferredWidth: Shell.Theme.playbackBusyIndicatorSize
                                    Layout.preferredHeight: Shell.Theme.playbackBusyIndicatorSize
                                    running: root.showPlaybackBusyOverlay
                                }

                                Label {
                                    Layout.alignment: Qt.AlignHCenter
                                    text: root.playbackBusyTitle
                                    color: Shell.Theme.textPrimary
                                    font.pixelSize: Shell.Theme.playbackBusyTitlePixelSize
                                    font.bold: true
                                }
                            }
                        }

                        Rectangle {
                            visible: root.fullscreenActive
                            z: 20
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

                                    PlaybackIconButton {
                                        text: root.showPauseAction ? qsTr("Pause") : qsTr("Play")
                                        symbolName: root.showPauseAction ? "pause" : "play_arrow"
                                        onClicked: bridge.requestPlayPause()
                                    }

                                    PlaybackIconButton {
                                        text: qsTr("Stop")
                                        symbolName: "stop"
                                        onClicked: bridge.requestStop()
                                    }

                                    Item {
                                        id: fullscreenVolumeArea
                                        Layout.preferredWidth: volumeIconBtn.implicitWidth
                                        Layout.preferredHeight: 38

                                        PlaybackIconButton {
                                            id: volumeIconBtn
                                            anchors.fill: parent
                                            text: bridge.muted ? qsTr("Unmute") : qsTr("Mute")
                                            symbolName: bridge.muted ? "volume_off" : "volume_up"
                                            onClicked: bridge.toggleMute()
                                        }

                                        Rectangle {
                                            id: fullscreenVolumePopup
                                            visible: root.fullscreenVolumePopupOpen
                                            z: 100
                                            anchors.centerIn: parent
                                            anchors.verticalCenterOffset: -parent.height / 2 - 28
                                            implicitWidth: 200
                                            implicitHeight: 36
                                            width: implicitWidth
                                            height: implicitHeight
                                            radius: Shell.Theme.radiusSm
                                            color: Shell.Theme.surfaceContainerHigh
                                            border.width: 1
                                            border.color: Shell.Theme.outline

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.margins: Shell.Theme.spacingSm
                                                spacing: Shell.Theme.spacingSm

                                                Label {
                                                    Layout.preferredWidth: 24
                                                    font.pixelSize: 12
                                                    color: Shell.Theme.textSecondary
                                                    horizontalAlignment: Text.AlignHCenter
                                                    text: bridge.muted ? qsTr("X") : "🔊"
                                                }

                                                Controls.ThemedSlider {
                                                    id: fullscreenVolumeSlider
                                                    Layout.fillWidth: true
                                                    from: 0
                                                    to: 100
                                                    stepSize: 1
                                                    value: bridge.volume
                                                    onMoved: bridge.setVolume(Math.round(value))
                                                    onPressedChanged: function() {
                                                        if (!pressed) {
                                                            root.scheduleFullscreenVolumePopupClose()
                                                        }
                                                    }
                                                }

                                                Label {
                                                    Layout.preferredWidth: 28
                                                    text: bridge.volume
                                                    font.pixelSize: 12
                                                    color: Shell.Theme.textSecondary
                                                    horizontalAlignment: Text.AlignRight
                                                }
                                            }

                                            HoverHandler {
                                                id: fullscreenVolumePopupHoverHandler
                                                onHoveredChanged: function() {
                                                    if (hovered) {
                                                        fullscreenVolumeCloseTimer.stop()
                                                        return
                                                    }

                                                    root.scheduleFullscreenVolumePopupClose()
                                                }
                                            }
                                        }

                                        HoverHandler {
                                            id: volumeHoverHandler
                                            onHoveredChanged: function() {
                                                if (hovered) {
                                                    root.openFullscreenVolumePopup()
                                                    return
                                                }

                                                root.scheduleFullscreenVolumePopupClose()
                                            }
                                        }
                                    }

                                    PlaybackIconButton {
                                        text: qsTr("Exit Full Screen")
                                        symbolName: "fullscreen_exit"
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

                                    RowLayout {
                                        visible: root.currentProgrammeVisible
                                        Layout.fillWidth: true
                                        spacing: Shell.Theme.spacingSm

                                        Label {
                                            text: qsTr("Now")
                                            color: Shell.Theme.textPrimary
                                            font.bold: true
                                            Layout.preferredWidth: Shell.Theme.statusProgrammeLabelWidth
                                        }

                                        Label {
                                            text: bridge.currentProgrammeTimeText
                                            color: Shell.Theme.textSecondary
                                            Layout.preferredWidth: Shell.Theme.statusProgrammeTimeWidth
                                            elide: Text.ElideRight
                                            visible: bridge.currentProgrammeTimeText.length > 0
                                        }

                                        Label {
                                            text: bridge.currentProgrammeTitle
                                            color: Shell.Theme.textPrimary
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                            visible: bridge.currentProgrammeTitle.length > 0
                                        }
                                    }

                                    RowLayout {
                                        visible: root.nextProgrammeVisible
                                        Layout.fillWidth: true
                                        spacing: Shell.Theme.spacingSm

                                        Label {
                                            text: qsTr("Next")
                                            color: Shell.Theme.textSecondary
                                            font.bold: true
                                            Layout.preferredWidth: Shell.Theme.statusProgrammeLabelWidth
                                        }

                                        Label {
                                            text: bridge.nextProgrammeTimeText
                                            color: Shell.Theme.textSecondary
                                            Layout.preferredWidth: Shell.Theme.statusProgrammeTimeWidth
                                            elide: Text.ElideRight
                                            visible: bridge.nextProgrammeTimeText.length > 0
                                        }

                                        Label {
                                            text: bridge.nextProgrammeTitle
                                            color: Shell.Theme.textSecondary
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                            visible: bridge.nextProgrammeTitle.length > 0
                                        }
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

                            PlaybackIconButton {
                                text: root.showPauseAction ? qsTr("Pause") : qsTr("Play")
                                symbolName: root.showPauseAction ? "pause" : "play_arrow"
                                onClicked: bridge.requestPlayPause()
                            }

                            PlaybackIconButton {
                                text: qsTr("Stop")
                                symbolName: "stop"
                                onClicked: bridge.requestStop()
                            }

                            Item {
                                id: normalVolumeArea
                                Layout.preferredWidth: normalVolumeIconBtn.implicitWidth
                                Layout.preferredHeight: 38

                                PlaybackIconButton {
                                    id: normalVolumeIconBtn
                                    anchors.fill: parent
                                    text: bridge.muted ? qsTr("Unmute") : qsTr("Mute")
                                    symbolName: bridge.muted ? "volume_off" : "volume_up"
                                    onClicked: bridge.toggleMute()
                                }

                                Rectangle {
                                    id: normalVolumePopup
                                    visible: root.normalVolumePopupOpen
                                    z: 100
                                    anchors.centerIn: parent
                                    anchors.verticalCenterOffset: -parent.height / 2 - 28
                                    implicitWidth: 200
                                    implicitHeight: 36
                                    width: implicitWidth
                                    height: implicitHeight
                                    radius: Shell.Theme.radiusSm
                                    color: Shell.Theme.surfaceContainerHigh
                                    border.width: 1
                                    border.color: Shell.Theme.outline

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: Shell.Theme.spacingSm
                                        spacing: Shell.Theme.spacingSm

                                        Label {
                                            Layout.preferredWidth: 24
                                            text: bridge.muted ? "🔇" : "🔊"
                                            font.pixelSize: 14
                                            horizontalAlignment: Text.AlignHCenter
                                        }

                                        Controls.ThemedSlider {
                                            id: normalVolumeSlider
                                            Layout.fillWidth: true
                                            from: 0
                                            to: 100
                                            stepSize: 1
                                            value: bridge.volume
                                            onMoved: bridge.setVolume(Math.round(value))
                                            onPressedChanged: function() {
                                                if (!pressed) {
                                                    root.scheduleNormalVolumePopupClose()
                                                }
                                            }
                                        }

                                        Label {
                                            Layout.preferredWidth: 28
                                            text: bridge.volume
                                            font.pixelSize: 12
                                            color: Shell.Theme.textSecondary
                                            horizontalAlignment: Text.AlignRight
                                        }
                                    }

                                    HoverHandler {
                                        id: normalVolumePopupHoverHandler
                                        onHoveredChanged: function() {
                                            if (hovered) {
                                                normalVolumeCloseTimer.stop()
                                                return
                                            }

                                            root.scheduleNormalVolumePopupClose()
                                        }
                                    }
                                }

                                HoverHandler {
                                    id: normalVolumeHoverHandler
                                    onHoveredChanged: function() {
                                        if (hovered) {
                                            root.openNormalVolumePopup()
                                            return
                                        }

                                        root.scheduleNormalVolumePopupClose()
                                    }
                                }
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            PlaybackIconButton {
                                text: qsTr("Full Screen")
                                symbolName: "fullscreen"
                                onClicked: root.toggleFullscreen()
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
