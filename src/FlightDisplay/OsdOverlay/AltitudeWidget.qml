import QtQuick 2.0

Rectangle {
    width: parent.width / 20 > 60 ? 60 : parent.width / 20
    height: parent.height / 2
    color:  "transparent"
    anchors.right: parent.right
    anchors.verticalCenter: parent.verticalCenter
    property var altitudeRnge: 30   //显示30m的范围
    property var centerY: height / 2
    property var ratio: height * 0.9 / altitudeRnge
    Canvas {
        property var lacalAltitude: altitude
        onLacalAltitudeChanged: requestPaint()
        anchors.fill: parent
        onPaint: {
            var ctx = getContext("2d")
            ctx.fillStyle = "white"
            drawAltitudeWidget(ctx)
        }
        function drawAltitudeWidget(context) {
            context.clearRect(0, 0, width, height)
            context.save()
            context.translate(0, height / 2)
            context.lineWidth = 2
            context.strokeStyle = "white"
            context.textAlign = "center"
            context.textBaseline = "middle"
            context.font = fontSize + "px Verdana"
            for(var i = Math.ceil(altitude) - altitudeRnge / 2; i <= Math.ceil(altitude) + altitudeRnge / 2; i++) {
                var posY = -1 * (i - altitude) * ratio
                if (i % 5 === 0) {
                    context.beginPath()
                    context.moveTo(0, posY)
                    context.lineTo(fontSize / 2, posY)
                    context.stroke()
                    context.fillText(i, fontSize * 1.5, posY)
                } else  {
                    context.beginPath()
                    context.moveTo(0, posY)
                    context.lineTo(fontSize / 4, posY)
                    context.stroke()
                }

            }
            context.restore()
        }
    }
    Text {
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("Alt(m)")
        y: -fontSize
        font.pixelSize: fontSize * 0.6
        font.family: "Verdana"
        color: "white"
    }
    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        x: fontSize / 2
        width: parent.width - fontSize / 2
        height: fontSize
        color: Qt.rgba(255, 255, 255, 0.4)


        Text {
            x: (parent.width - width) / 2
            anchors.verticalCenter: parent.verticalCenter
            font.pixelSize: fontSize
            font.family: "Verdana"
            color: "red"
            verticalAlignment: Text.AlignVCenter
            text: altitude.toFixed(1)
        }
    }
}
