import QtQuick 2.0

Item {
    width: parent.width / 5
    height: parent.height / 5
    anchors.right: parent.right
    anchors.top: parent.top
    anchors.topMargin: fontSize

    Row {
        spacing: fontSize / 3
        Image {
            width: fontSize
            height: fontSize
            id: name
            source: "/qmlimages/Gps.svg"
        }

        Text {
            text: satelliteUsed
            verticalAlignment: Text.AlignVCenter
            font.family: "Verdana"
            font.pixelSize: fontSize
            color: "white"
        }
        Text {
            text: qsTr("HDOP:") + hdop
            verticalAlignment: Text.AlignVCenter
            font.family: "Verdana"
            font.pixelSize: fontSize * 0.8
            color: "white"
        }
        Text {
            text: qsTr("VDOP:") + vdop
            verticalAlignment: Text.AlignVCenter
            font.family: "Verdana"
            font.pixelSize: fontSize * 0.8
            color: "white"
        }
    }


}
