import QtQuick 2.12
import QtQuick.Shapes 1.12


Item {
    visible: true
    anchors.fill: parent
    property var radius: height * 0.4

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        width: fontSize * 3
        height: fontSize
        y: rotationCenterY - parent.radius + fontSize
        color: Qt.rgba(155, 155, 155, 0.4)
        Text {
            anchors.centerIn: parent
            color: "red"
            font.family: "Verdana"
            font.pixelSize: fontSize
            text: rollAngle.toFixed(1)
        }
    }


    Canvas{
        property var rollAngleRadian: rollAngle * Math.PI / 180
        property var angleRange: Math.PI * 2 / 3
        antialiasing: true
        function drawTicksAndNumbers(context, centerX, centerY) {
            context.clearRect(0, 0, width, height)
            context.save()
            context.translate(centerX, centerY)
            // 画箭头
            context.save()
            context.fillStyle = "red"
            context.lineWidth = lineWidth
            context.translate(0, -1 * radius + fontSize / 5)
            context.beginPath()
            context.moveTo(0, 0)
            context.lineTo(-fontSize / 3, fontSize / 2)
            context.lineTo(fontSize / 3, fontSize / 2)
            context.closePath()
            context.fill()
            context.restore()

            context.beginPath()
            context.lineWidth = lineWidth
            context.strokeStyle = "white"
            context.arc(0, 0, radius, angleRange / 2 - Math.PI / 2 + rollAngleRadian , -1 * angleRange / 2 - Math.PI / 2 + rollAngleRadian, true)
            context.stroke()

            // 画刻度及角度，因字体朝向y轴负方向，所有以对y轴进行旋转
            context.rotate( -1 * angleRange / 2 + rollAngleRadian)
            var count = angleRange / (10 * Math.PI / 180)
            for (var i = 0; i <= count; i++) {
                context.save()
                context.translate(0, -1 * radius);
                context.moveTo(0, 0)
                context.lineTo(0, -1 * fontSize / 3)
                context.stroke()
                context.fillText(Math.abs(i - count / 2) * 10, 0, -1 * fontSize / 2)
                context.restore()
                context.rotate(angleRange / count)
            }
            context.restore();
        }

      anchors.fill: parent
      onPaint: {
          var ctx = getContext("2d");
          ctx.font = fontSize + "px Verdana"
          ctx.textAlign = "center";

          var centerX = parent.width / 2
          ctx.fillStyle="white"
          drawTicksAndNumbers(ctx, centerX, rotationCenterY)
      }
      onRollAngleRadianChanged: requestPaint()
    }
}
