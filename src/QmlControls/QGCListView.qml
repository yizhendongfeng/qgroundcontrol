import QtQuick 2.3
import QtQuick.Controls 2.5
import QGroundControl.Palette 1.0
import QGroundControl.ScreenTools 1.0

/// QGC version of ListVIew control that shows horizontal/vertial scroll indicators
ListView {
    id:             root
    boundsBehavior: Flickable.StopAtBounds
    clip: true
    property color indicatorColor: qgcPal.text

    QGCPalette { id: qgcPal; colorGroupEnabled: enabled }
    ScrollBar.vertical: ScrollBar {
        padding: 0
        policy: root.contentHeight > root.height ? ScrollBar.AlwaysOn:ScrollBar.AlwaysOff
        contentItem: Rectangle {
            implicitWidth: ScreenTools.defaultFontPixelWidth * 0.8
            radius: width / 2
            color: qgcPal.colorGrey
        }
        background: Rectangle {
            color: qgcPal.alertBorder
            radius: width / 2
        }
    }
//    ScrollBar.horizontal: ScrollBar { }

//    Component.onCompleted: {
//        var indicatorComponent = Qt.createComponent("QGCFlickableVerticalIndicator.qml")
//        indicatorComponent.createObject(root)
//        indicatorComponent = Qt.createComponent("QGCFlickableHorizontalIndicator.qml")
//        indicatorComponent.createObject(root)
//    }
}
