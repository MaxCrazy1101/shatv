pragma Singleton

import QtQuick

QtObject {
    readonly property color windowBackground: "#1f2430"
    readonly property color sidebarBackground: "#202534"
    readonly property color contentBackground: "#181c26"
    readonly property color panelBackground: "#242b3c"
    readonly property color textPrimary: "#dbe3f4"

    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12

    readonly property int radiusMd: 10

    readonly property int sidebarPreferredWidth: 320
    readonly property int sidebarMinWidth: 280
    readonly property int sidebarMaxWidth: 440
    readonly property int videoMinHeight: 320
    readonly property int controlPanelHeight: 56
    readonly property int statusPanelHeight: 44
}
