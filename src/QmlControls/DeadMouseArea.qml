import QtQuick 2.3
import QtQuick.Controls 1.2

MouseArea {
    preventStealing:true
    hoverEnabled:   true
    FocusScope {
        id: focusScope
        anchors.fill: parent
    }
    onWheel:        { wheel.accepted = true; }
    onPressed:      { mouse.accepted = true; focusScope.focus = true}
    onReleased:     { mouse.accepted = true; }
}
