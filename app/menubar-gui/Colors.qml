import QtQuick 2.15
pragma Singleton

QtObject {
    id: colors
    property color primary: "#181818"
    property color accent: "#252525"
    property color secondary: "#636363"
    property color textSecondary: "#ADADAD"
    property color textPrimary: "#FFFFFF"
    property color warning: "#FFF7A1"
    property color button: "#3D3F3E"
    //Used for good looking color transitions from transparent
    property color primaryTransparent:  Qt.rgba(24 / 255, 24 / 255, 24 / 255, 0)
}