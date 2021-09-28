import QtQuick 2.0
import QtQuick.Shapes 1.12
import QGroundControl.Vehicle       1.0
import QGroundControl.ScreenTools           1.0
import QGroundControl.FlightMap             1.0
import QtQuick.Controls 2.0

Item {
    property var activeVehicle: null
    property var rollAngle: activeVehicle ? activeVehicle.roll.rawValue : 0
    property var pitchAngle: activeVehicle ? activeVehicle.pitch.rawValue : 0
    property var yawAngle: activeVehicle ? activeVehicle.heading.rawValue : 0
    property var altitude: activeVehicle ? activeVehicle.altitudeAMSL.rawValue : 0
    property var velocity: activeVehicle ? activeVehicle.groundSpeed.rawValue : 0
    property var satelliteUsed: activeVehicle ? activeVehicle.gps.count.rawValue : 0
    property var hdop: activeVehicle ? activeVehicle.gps.hdop.rawValue : -1
    property var vdop: activeVehicle ? activeVehicle.gps.vdop.rawValue : -1

    property var fontSize: height / 20 > ScreenTools.defaultFontPixelHeight ? ScreenTools.defaultFontPixelHeight : height / 20
    property var lineWidth: fontSize / 10  > 2 ? 2 : fontSize / 10
    property var rotationCenterY: height * 0.6

    RollWidget {
        id: rollWidget
        anchors.fill: parent
    }

    PitchWidget {
        id: pitchWidget
    }

    YawWidget {
        id: yawWidget

    }

//    AltitudeWidget {

//    }

//    VelocityWidget {

//    }

//    SatelliteWidget {

//    }

}
