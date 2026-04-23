import QtQuick

import ".." as Shell

Text {
    id: root

    property string symbolName: ""
    property bool filled: false
    property int iconSize: 18

    readonly property string outlinedFamily: outlinedFont.name
    readonly property string filledFamily: filledFont.name

    text: root.symbolName
    color: Shell.Theme.textPrimary
    font.family: root.filled && root.filledFamily.length > 0 ? root.filledFamily : root.outlinedFamily
    font.pixelSize: root.iconSize
    font.preferShaping: true
    font.features: { "liga": 1 }
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    textFormat: Text.PlainText

    FontLoader {
        id: outlinedFont
        source: "qrc:/qt/qml/fonts/MaterialSymbolsOutlined-Regular.ttf"
    }

    FontLoader {
        id: filledFont
        source: "qrc:/qt/qml/fonts/MaterialSymbolsOutlined_Filled-Regular.ttf"
    }
}
