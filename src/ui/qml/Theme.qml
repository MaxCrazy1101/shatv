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

    // Spacing
    readonly property int spacingXs: 4
    readonly property int spacingSm: 8
    readonly property int spacingMd: 12

    // Shape
    readonly property int radiusSm: 6
    readonly property int radiusMd: 10

    // Layout
    readonly property int sidebarPreferredWidth: 320
    readonly property int sidebarMinWidth: 280
    readonly property int sidebarMaxWidth: 440
    readonly property int videoMinHeight: 320
    readonly property int controlPanelHeight: 56
    readonly property int statusPanelHeight: 44
}
