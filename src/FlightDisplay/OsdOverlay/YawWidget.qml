import QtQuick 2.0
import QtQuick.Controls 2.5

Item {
    anchors.fill: parent
    property var angleRange: 120        // 显示范围
    property var totalWidth: parent.height * 0.8  // 显示宽度
    property var bottomY: 5 + fontSize * 1.5      // 刻度线底部位置
    property var ratio: totalWidth / angleRange
    anchors.horizontalCenter: parent.horizontalCenter
    Canvas {
        property var localYawAngle: yawAngle
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            ctx.fillStyle = "lightgrey"
            drawYawWidget(ctx)
        }

        function limitToAngle(angle) {
            while (angle >= 360)
                angle -= 360
            while (angle < 0)
                angle += 360
            return angle
        }

        function drawYawWidget(context) {
            context.clearRect(0, 0, width, height)
            context.save()
            var centerX = width / 2
            context.translate(centerX, bottomY)
            context.strokeStyle = "white"
            context.fillStyle = "white"
            context.font = fontSize + "px Verdana"
            context.lineWidth = lineWidth
            context.save()
            context.fillStyle = "red"
            context.beginPath()
            context.moveTo(0 , 0)
            context.lineTo(-fontSize / 2, fontSize)
            context.lineTo(fontSize / 2, fontSize)
            context.closePath()
            context.fill()
            context.restore()
            context.textAlign = "center"
            context.textBaseline = "bottom"
            var tickPosX, angle, startAngle, endAngle, angleText
            startAngle = Math.ceil(yawAngle) - angleRange / 2
            endAngle = Math.ceil(yawAngle) + angleRange / 2
            for (var i = startAngle; i <= endAngle; i++) {
                tickPosX = (i - Math.ceil(yawAngle)) * ratio
                angle = limitToAngle(i)
                switch (angle) {
                case 0:
                    angleText = qsTr("N")
                    break
                case 45:
                    angleText = qsTr("NE")
                    break
                case 90:
                    angleText = qsTr("E")
                    break
                case 135:
                    angleText = qsTr("SE")
                    break
                case 180:
                    angleText = qsTr("S")
                    break
                case 225:
                    angleText = qsTr("SW")
                    break
                case 270:
                    angleText = qsTr("W")
                    break
                case 315:
                    angleText = qsTr("NW")
                    break
                default:
                    angleText = angle
                }

                //绘制数字及刻度
                if (i % 15 == 0) {
                    context.fillText(angleText, tickPosX , -fontSize * 2 / 3)
                    context.beginPath()
                    context.moveTo(tickPosX, 0)
                    context.lineTo(tickPosX, -fontSize * 2  / 3)
                    context.stroke()
                } else if (i % 5 == 0) {
                    context.beginPath()
                    context.moveTo(tickPosX, 0)
                    context.lineTo(tickPosX, -fontSize / 3)
                    context.stroke()
                }
            }
            context.restore()
        }

        onLocalYawAngleChanged: requestPaint()

    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        width: fontSize * 3
        height: fontSize
        y: 5 - fontSize / 3
        color: Qt.rgba(255, 255, 255, 0.4)
        Text {
            font.pixelSize: fontSize
            font.family: "Verdana"
            color: "red"
            text: yawAngle.toFixed(1)
            anchors.centerIn: parent
        }

    }
}
