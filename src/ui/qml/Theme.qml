pragma Singleton

import QtQuick

QtObject {
    // Surface hierarchy: base (darkest) → highest (lightest)
    readonly property color surface: "#181c26"
    readonly property color surfaceContainer: "#202534"
    readonly property color surfaceContainerHigh: "#252b3a"
    readonly property color surfaceContainerHighest: "#2f3a52"
    readonly property color surfaceDim: "#1f2430"
    readonly property color surfaceBright: "#242b3c"

    // Text
    readonly property color textPrimary: "#dbe3f4"
    readonly property color textSecondary: "#9eb1cf"
    readonly property color textDisabled: "#6d7f99"

    // Outline
    readonly property color outline: "#33415c"
    readonly property color focusRing: "#5a88ff"

    // Control surfaces
    readonly property color controlSurface: "#263047"
    readonly property color controlSurfaceHover: "#2e3953"
    readonly property color controlSurfacePressed: "#38486a"
    readonly property color controlSurfaceDisabled: "#232b3d"
    readonly property color controlBorder: "#41506d"
    readonly property color controlBorderStrong: "#5a88ff"
    readonly property color controlAccent: "#5a88ff"
    readonly property color controlAccentMuted: "#34486f"
    readonly property color listItemCurrent: "#334766"
    readonly property color listItemHover: "#29364d"

    // Spacing
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12

    // Shape
    readonly property int radiusSm: 6
    readonly property int radiusMd: 10

    // Layout
    readonly property int titleBarHeight: 42
    readonly property int windowButtonWidth: 44
    readonly property int sidebarPreferredWidth: 320
    readonly property int sidebarMinWidth: 280
    readonly property int sidebarMaxWidth: 440
    readonly property int videoMinHeight: 320
    readonly property int controlPanelHeight: 64
    readonly property int statusPanelHeight: 44
    readonly property int playbackBusyIndicatorSize: 44
    readonly property int playbackBusyTitlePixelSize: 18
    readonly property color playbackBusyOverlay: "#8A000000"

    // Title bar
    readonly property color titleBarCloseHover: "#c94f4f"
    readonly property color titleBarClosePressed: "#a63d3d"
}
