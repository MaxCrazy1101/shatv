import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import QtQuick.Window

import ".." as Shell
import "../controls" as Controls

Window {
    id: root

    property string appVersion: ""
    property string buildId: ""
    property string logFilePath: ""
    property string logsDirectoryPath: ""
    property string speechModelStatusToken: ""
    property string speechModelStatusText: ""
    property string speechModelStatusDetail: ""
    property string speechModelName: ""
    property string speechModelVersion: ""
    property string speechModelSourceUrl: ""
    property string speechModelArchiveSizeText: ""
    property string speechModelInstalledSizeText: ""
    property string speechModelChecksum: ""
    property string speechModelLicense: ""
    property string speechModelAttribution: ""
    property string speechModelDirectory: ""
    property bool speechModelInstalled: false
    property bool speechModelDeveloperOverride: false
    property bool speechModelRuntimeAvailable: false
    property bool speechModelInstallSupported: false
    property bool speechModelBusy: false
    property bool speechModelDetailsVisible: false
    property bool speechModelDownloadActive: false
    property real speechModelProgress: -1
    property string speechModelOperationText: ""

    signal submitted(string userAgent, string epgUrl)
    signal openLogsFolderRequested()
    signal copyDiagnosticsRequested()
    signal refreshSpeechModelRequested()
    signal downloadSpeechModelRequested()
    signal cancelSpeechModelDownloadRequested()
    signal installSpeechModelArchiveRequested(url archiveUrl)
    signal deleteSpeechModelRequested()

    width: 720
    height: 560
    minimumWidth: 600
    minimumHeight: 460
    visible: false
    modality: Qt.NonModal
    title: qsTr("Settings")
    color: Shell.Theme.surfaceDim

    function openWithValues(userAgent, epgUrl, tabIndex = 0) {
        // 每次打开都回填当前配置，避免窗口常驻时展示过期值。
        userAgentField.text = userAgent
        epgUrlField.text = epgUrl
        settingsTabBar.currentIndex = tabIndex
        speechModelDetailsVisible = false

        show()
        raise()
        requestActivate()

        if (tabIndex === 0) {
            userAgentField.forceActiveFocus()
        }
    }

    function displayValue(value) {
        return value.length > 0 ? value : qsTr("Unknown")
    }

    readonly property bool speechModelActionsAvailable: speechModelRuntimeAvailable
        && speechModelInstallSupported
        && !speechModelDeveloperOverride

    Shortcut {
        sequence: "Escape"
        onActivated: root.close()
    }

    component SettingsTabButton: TabButton {
        id: control

        implicitHeight: 38
        leftPadding: Shell.Theme.spacingMd
        rightPadding: Shell.Theme.spacingMd
        topPadding: Shell.Theme.spacingSm
        bottomPadding: Shell.Theme.spacingSm

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
            color: control.checked
                ? Shell.Theme.controlSurfacePressed
                : (control.hovered ? Shell.Theme.controlSurfaceHover : "transparent")
            border.width: 1
            border.color: control.checked ? Shell.Theme.controlBorderStrong : "transparent"
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Shell.Theme.surfaceDim

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Shell.Theme.spacingMd
            spacing: Shell.Theme.spacingMd

            TabBar {
                id: settingsTabBar
                Layout.fillWidth: true
                spacing: Shell.Theme.spacingSm

                background: Rectangle {
                    radius: Shell.Theme.radiusMd
                    color: Shell.Theme.surfaceContainerHigh
                    border.width: 1
                    border.color: Shell.Theme.outline
                }

                SettingsTabButton {
                    text: qsTr("General")
                }

                SettingsTabButton {
                    text: qsTr("Speech")
                }

                SettingsTabButton {
                    text: qsTr("About")
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: settingsTabBar.currentIndex

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    contentWidth: availableWidth

                    ColumnLayout {
                        width: Math.max(parent ? parent.width : 0, root.width - Shell.Theme.spacingMd * 4)
                        spacing: Shell.Theme.spacingMd

                        Label {
                            Layout.fillWidth: true
                            text: qsTr(
                                "Configure the HTTP User-Agent and an optional XMLTV EPG URL used for current/next programme lookup.")
                            color: Shell.Theme.textSecondary
                            wrapMode: Text.WordWrap
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Shell.Theme.spacingSm

                            Label {
                                text: qsTr("User-Agent")
                                color: Shell.Theme.textPrimary
                            }

                            Controls.ThemedTextField {
                                id: userAgentField
                                Layout.fillWidth: true
                                placeholderText: qsTr("Leave empty to use the default")
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Shell.Theme.spacingSm

                            Label {
                                text: qsTr("EPG URL")
                                color: Shell.Theme.textPrimary
                            }

                            Controls.ThemedTextField {
                                id: epgUrlField
                                Layout.fillWidth: true
                                placeholderText: qsTr("https://example.com/guide.xml.gz")
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    contentWidth: availableWidth

                    ColumnLayout {
                        width: Math.max(parent ? parent.width : 0, root.width - Shell.Theme.spacingMd * 4)
                        spacing: Shell.Theme.spacingMd

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Shell.Theme.spacingSm

                            Label {
                                Layout.fillWidth: true
                                text: root.displayValue(root.speechModelStatusText)
                                color: root.speechModelInstalled ? Shell.Theme.accent : Shell.Theme.textPrimary
                                font.pixelSize: 18
                                font.bold: true
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.displayValue(root.speechModelName)
                                color: Shell.Theme.textPrimary
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.speechModelStatusDetail
                                color: Shell.Theme.textSecondary
                                wrapMode: Text.WrapAnywhere
                                visible: root.speechModelStatusDetail.length > 0
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Shell.Theme.spacingSm
                            visible: root.speechModelRuntimeAvailable

                            Controls.ThemedToolButton {
                                text: qsTr("Download")
                                visible: root.speechModelInstallSupported
                                    && !root.speechModelDeveloperOverride
                                enabled: root.speechModelActionsAvailable
                                    && !root.speechModelBusy
                                onClicked: root.downloadSpeechModelRequested()
                            }

                            Controls.ThemedToolButton {
                                text: qsTr("Cancel")
                                visible: root.speechModelDownloadActive
                                enabled: root.speechModelDownloadActive
                                onClicked: root.cancelSpeechModelDownloadRequested()
                            }

                            Controls.ThemedToolButton {
                                text: qsTr("Install Archive")
                                visible: root.speechModelInstallSupported
                                    && !root.speechModelDeveloperOverride
                                enabled: root.speechModelActionsAvailable
                                    && !root.speechModelBusy
                                onClicked: speechModelArchiveDialog.open()
                            }

                            Controls.ThemedToolButton {
                                text: qsTr("Refresh")
                                enabled: !root.speechModelBusy
                                onClicked: root.refreshSpeechModelRequested()
                            }

                            Controls.ThemedToolButton {
                                text: qsTr("Delete")
                                visible: root.speechModelInstallSupported
                                    && !root.speechModelDeveloperOverride
                                enabled: root.speechModelInstalled
                                    && root.speechModelActionsAvailable
                                    && !root.speechModelBusy
                                onClicked: root.deleteSpeechModelRequested()
                            }

                            BusyIndicator {
                                running: root.speechModelBusy
                                visible: running
                                implicitWidth: 28
                                implicitHeight: 28
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Shell.Theme.spacingSm
                            visible: root.speechModelRuntimeAvailable
                                && root.speechModelOperationText.length > 0

                            Label {
                                Layout.fillWidth: true
                                text: root.speechModelOperationText
                                color: Shell.Theme.textSecondary
                                wrapMode: Text.WordWrap
                            }

                            ProgressBar {
                                Layout.fillWidth: true
                                from: 0
                                to: 1
                                value: Math.max(0, Math.min(1, root.speechModelProgress))
                                visible: root.speechModelProgress >= 0
                            }
                        }

                        Controls.ThemedToolButton {
                            text: root.speechModelDetailsVisible
                                ? qsTr("Hide Details")
                                : qsTr("Show Details")
                            onClicked: root.speechModelDetailsVisible = !root.speechModelDetailsVisible
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: Shell.Theme.spacingMd
                            rowSpacing: Shell.Theme.spacingSm
                            visible: root.speechModelDetailsVisible

                            Label {
                                text: qsTr("Version")
                                color: Shell.Theme.textSecondary
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.displayValue(root.speechModelVersion)
                                color: Shell.Theme.textPrimary
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                text: qsTr("Source")
                                color: Shell.Theme.textSecondary
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.displayValue(root.speechModelSourceUrl)
                                color: Shell.Theme.textPrimary
                                wrapMode: Text.WrapAnywhere
                            }

                            Label {
                                text: qsTr("Archive")
                                color: Shell.Theme.textSecondary
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.displayValue(root.speechModelArchiveSizeText)
                                color: Shell.Theme.textPrimary
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                text: qsTr("Installed")
                                color: Shell.Theme.textSecondary
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.displayValue(root.speechModelInstalledSizeText)
                                color: Shell.Theme.textPrimary
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                text: qsTr("Checksum")
                                color: Shell.Theme.textSecondary
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.displayValue(root.speechModelChecksum)
                                color: Shell.Theme.textPrimary
                                wrapMode: Text.WrapAnywhere
                            }

                            Label {
                                text: qsTr("License")
                                color: Shell.Theme.textSecondary
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.displayValue(root.speechModelLicense)
                                color: Shell.Theme.textPrimary
                                wrapMode: Text.WordWrap
                            }

                            Label {
                                text: qsTr("Attribution")
                                color: Shell.Theme.textSecondary
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.displayValue(root.speechModelAttribution)
                                color: Shell.Theme.textPrimary
                                wrapMode: Text.WrapAnywhere
                            }

                            Label {
                                text: qsTr("Directory")
                                color: Shell.Theme.textSecondary
                            }

                            Label {
                                Layout.fillWidth: true
                                text: root.displayValue(root.speechModelDirectory)
                                color: Shell.Theme.textPrimary
                                wrapMode: Text.WrapAnywhere
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }
                    }
                }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    contentWidth: availableWidth

                    ColumnLayout {
                        width: Math.max(parent ? parent.width : 0, root.width - Shell.Theme.spacingMd * 4)
                        spacing: Shell.Theme.spacingMd

                        Label {
                            text: qsTr("ShaTV")
                            color: Shell.Theme.textPrimary
                            font.pixelSize: 22
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("A cross-platform IPTV player.")
                            color: Shell.Theme.textSecondary
                            wrapMode: Text.WordWrap
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: Shell.Theme.spacingMd
                            rowSpacing: Shell.Theme.spacingSm

                            Label {
                                text: qsTr("Version")
                                color: Shell.Theme.textSecondary
                            }

                            Label {
                                text: root.appVersion
                                color: Shell.Theme.textPrimary
                            }

                            Label {
                                text: qsTr("Build")
                                color: Shell.Theme.textSecondary
                            }

                            Label {
                                text: root.buildId
                                color: Shell.Theme.textPrimary
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Built with C++20, Qt 6, and FFmpeg.")
                            color: Shell.Theme.textSecondary
                            wrapMode: Text.WordWrap
                        }

                        Label {
                            text: qsTr("Diagnostics")
                            color: Shell.Theme.textPrimary
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: root.logFilePath.length > 0
                                ? qsTr("Log file: %1").arg(root.logFilePath)
                                : qsTr("File logging is not available.")
                            color: Shell.Theme.textSecondary
                            wrapMode: Text.WrapAnywhere
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Shell.Theme.spacingSm

                            Controls.ThemedToolButton {
                                text: qsTr("Open Logs Folder")
                                enabled: root.logsDirectoryPath.length > 0
                                onClicked: root.openLogsFolderRequested()
                            }

                            Controls.ThemedToolButton {
                                text: qsTr("Copy Diagnostics")
                                onClicked: root.copyDiagnosticsRequested()
                            }

                            Item {
                                Layout.fillWidth: true
                            }
                        }

                        Label {
                            text: qsTr("Authors")
                            color: Shell.Theme.textPrimary
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.RichText
                            text: "MaxCrazy (<a href=\"mailto:alex02newton@gmail.com\">alex02newton@gmail.com</a>)"
                            color: Shell.Theme.textPrimary
                            wrapMode: Text.Wrap
                            onLinkActivated: link => Qt.openUrlExternally(link)
                        }

                        Label {
                            text: qsTr("License")
                            color: Shell.Theme.textPrimary
                            font.bold: true
                        }

                        Label {
                            text: qsTr("Not specified")
                            color: Shell.Theme.textSecondary
                        }

                        Label {
                            text: qsTr("Repository")
                            color: Shell.Theme.textPrimary
                            font.bold: true
                        }

                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.RichText
                            text: "<a href=\"https://github.com/MaxCrazy1101/shatv\">github.com/MaxCrazy1101/shatv</a>"
                            color: Shell.Theme.textPrimary
                            wrapMode: Text.Wrap
                            onLinkActivated: link => Qt.openUrlExternally(link)
                        }

                        Item {
                            Layout.fillHeight: true
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: Shell.Theme.outline
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Shell.Theme.spacingSm

                Item {
                    Layout.fillWidth: true
                }

                Controls.ThemedToolButton {
                    text: qsTr("Close")
                    onClicked: root.close()
                }

                Controls.ThemedToolButton {
                    text: qsTr("Save")
                    onClicked: {
                        // 设置窗口统一提交“常规”页数据，关于页只负责只读展示。
                        root.submitted(userAgentField.text.trim(), epgUrlField.text.trim())
                        root.close()
                    }
                }
            }
        }
    }

    FileDialog {
        id: speechModelArchiveDialog
        title: qsTr("Install ASR Model Archive")
        options: FileDialog.DontUseNativeDialog
        nameFilters: [
            qsTr("Model Archives (*.tar.bz2 *.tar.gz *.tgz *.zip)"),
            qsTr("All Files (*)")
        ]
        onAccepted: root.installSpeechModelArchiveRequested(selectedFile)
    }
}
