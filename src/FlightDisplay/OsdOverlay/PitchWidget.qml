import QtQuick 2.0
import QtQuick.Controls 2.5


Item {
    anchors.fill: parent

    Canvas {
        property var localPitchAngle: pitchAngle
        property var angleRange: 40
        property var ladderWidth: height / 3
        property var ratio: height / 3 / angleRange
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d");
            ctx.fillStyle = "lightgrey"
            ctx.lineWidth = lineWidth
            drawPitchWidget(ctx)
        }
        function drawPitchWidget(context) {
            context.clearRect(0, 0, width, height)
            context.save()
            context.fillStyle = "white"
            context.strokeStyle = "white"
            context.lineWidth = lineWidth
            context.font = fontSize + "px Verdana"

            var centerX = width / 2
            var centerY = height / 2
            var posX =  -1 * ladderWidth / 2
            var ladderPosY, angle
            context.translate(centerX, rotationCenterY)
            context.rotate(rollAngle * Math.PI / 180)

            for (var i = Math.ceil(pitchAngle) - angleRange / 2; i <= Math.ceil(pitchAngle) + angleRange / 2; i++) {
                ladderPosY =  -1 * (i - Math.ceil(pitchAngle)) * ratio
                if (i % 10 == 0) {
                    angle = i
                    if (angle > 90 || angle < -90)
                        continue
                    //绘制数字
                    context.textAlign = "right"
                    context.textBaseline = "bottom"
                    context.fillText(angle, posX - fontSize / 2, ladderPosY + fontSize / 2)
                    context.textAlign = "left"
                    context.fillText(angle, posX + fontSize / 2 + ladderWidth, ladderPosY + fontSize / 2)
                    // 绘制横线，0线为绿直线，0线上位白色直线，0线下为虚线
                    if (i > 0) {
                        context.beginPath()
                        context.moveTo(posX, ladderPosY)
                        context.lineTo(posX + ladderWidth, ladderPosY)
                        context.stroke()
                    } else if (i < 0) {
                        context.beginPath()
                        context.moveTo(posX, ladderPosY)
                        context.lineTo(posX + ladderWidth * 2 / 5, ladderPosY)
                        context.moveTo(posX + ladderWidth * 3 / 5, ladderPosY)
                        context.lineTo(posX + ladderWidth, ladderPosY)
                        context.stroke()
                    } else if (i === 0) {
                        context.save()
                        context.beginPath()
                        context.lineWidth = lineWidth
                        context.strokeStyle = "green"
                        context.moveTo(posX, ladderPosY)
                        context.lineTo(posX + ladderWidth, ladderPosY)
                        context.stroke()
                        context.restore()
                    }
                }
            }
            // 画0度基线
            context.save()
            context.strokeStyle = "red"
            context.fillStyle = "red"
            context.lineWidth = lineWidth
            context.beginPath()
            context.moveTo(-1 * ladderWidth * 1.5, 0)
            context.lineTo(-1 * ladderWidth * 1, 0)
            context.moveTo(-1 * ladderWidth * 0.4, ladderWidth * 0.1)
            context.lineTo(0, 0)
            context.lineTo(ladderWidth * 0.4, ladderWidth * 0.1)
            context.moveTo(ladderWidth * 1, 0)
            context.lineTo(ladderWidth * 1.5, 0)
            context.textAlign = "center"
            context.stroke()
            context.restore()
            context.restore()
        }

        onLocalPitchAngleChanged: requestPaint()
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        width: fontSize * 3
        height: fontSize
        y: rotationCenterY + fontSize
        color: Qt.rgba(155, 155, 155, 0.4)
        Text {
            font.pixelSize: fontSize
            font.family: "Verdana"
            color: "red"
            text: pitchAngle.toFixed(1)
            anchors.centerIn: parent
        }

    }
}
