#include <QDebug>
#include <QSettings>
#include <QNetworkDatagram>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QTimer>
#include <QDateTime>

#include "mavlink.h"
#include "../Vehicle/Vehicle.h"
#include "QGCApplication.h"
#include "SettingsManager.h"
#include "IIPSComm.h"

enum AltitudeMode {
    AltitudeModeNone,           // Being used as distance value unrelated to ground (for example distance to structure)
    AltitudeModeRelative,       // MAV_FRAME_GLOBAL_RELATIVE_ALT
    AltitudeModeAbsolute,       // MAV_FRAME_GLOBAL
    AltitudeModeAboveTerrain,   // Absolute altitude above terrain calculated from terrain data
    AltitudeModeTerrainFrame    // MAV_FRAME_GLOBAL_TERRAIN_ALT
};

IIPSComm::IIPSComm(QObject *parent) : QObject(parent)
{
    QSettings settings;
    settings.beginGroup("IIPS");
    if(settings.contains("iipsIp"))
        iipsIp = settings.value("iipsIp", "127.0.0.1").toString();
    else
    {
        iipsIp = "127.0.0.1";
        settings.setValue("iipsIp", iipsIp);
    }
    iipsAddress.setAddress(iipsIp);
    if(settings.contains("iipsPort"))
        iipsPort = static_cast<uint16_t>(settings.value("iipsPort", 8000).toUInt());
    else
    {
        iipsPort = 8000;
        settings.setValue("iipsPort", iipsPort);
    }
    if(settings.contains("qgcPort"))
        qgcPort = static_cast<uint16_t>(settings.value("qgcPort", 8001).toUInt());
    else
    {
        qgcPort = 8001;
        settings.setValue("qgcPort", qgcPort);
    }
    settings.endGroup();

    if(!udpSocket.bind(qgcPort, QAbstractSocket::ShareAddress))
        qWarning("IIPS udp bind to port %d failed", qgcPort);
    connect(&udpSocket, &QUdpSocket::readyRead, this, &IIPSComm::SlotReceiveData);

    sendPacket.dataLength = statusPacketId;
    sendPacket.id = statusPacketLength;

    QTimer* timerTest = new QTimer(this);
    connect(timerTest, &QTimer::timeout, this, &IIPSComm::SlotTestTimeout);
//    timerTest->start(1000);
    memset(&receiveBuffer, 0, sizeof(receiveBuffer));
    timerIipsConnect.setSingleShot(true);
    connect(&timerIipsConnect, &QTimer::timeout, this, [&](){
        iipsConnected = false;
        emit iipsConnectedChanged();
        qDebug() << "hide";
    });
}

IIPSComm::~IIPSComm()
{
    QSettings settings;
    settings.beginGroup("IIPS");
    settings.setValue("iipsIp", iipsIp);
    settings.setValue("iipsPort", iipsPort);
    settings.setValue("qgcPort", qgcPort);
    settings.endGroup();
}

int IIPSComm::SaveWaypointsToJsonFile(PacketId type)
{
    QString fileName;

    if (type == LINE)
        fileName = "/IIPSLine.plan";
    else if (type == REGION || type == SURVEY)
        fileName = "/IIPSSurvey.plan";
    else
        return -1;
    QFile jsonPlanFile(qgcApp()->toolbox()->settingsManager()->appSettings()->missionSavePath() + fileName);
    QJsonObject planJson;
    planJson["groundStation"] = "QGroundControl";
    planJson["fileType"] = "Plan";
    planJson["version"] = 1;

    QJsonObject missionJson;
    QJsonArray coordinateArray;
    Waypoint waypointHome = listWaypoints.first();
    coordinateArray << waypointHome.latitude << waypointHome.longitude << 0;
    QJsonValue jsonValueHome(coordinateArray);
    missionJson["plannedHomePosition"] = jsonValueHome;
    missionJson["firmwareType"] = vehicle ? vehicle->firmwareType() : MAV_AUTOPILOT::MAV_AUTOPILOT_PX4;
    missionJson["vehicleType"] = vehicle ? vehicle->vehicleType() : MAV_TYPE::MAV_TYPE_QUADROTOR;
    missionJson["cruiseSpeed"] = vehicle ? vehicle->defaultCruiseSpeed() : 15;
    missionJson["hoverSpeed"] = vehicle ? vehicle->defaultHoverSpeed() : 5;
    QJsonArray missionItems;
    if (type == LINE)
    {
        for (int i = 1; i < listWaypoints.size(); i++)
        {
            QJsonObject jsonObjectItem;
            jsonObjectItem["type"] = "SimpleItem";
            jsonObjectItem["frame"] =  MAV_FRAME::MAV_FRAME_GLOBAL_RELATIVE_ALT;
            jsonObjectItem["command"] = MAV_CMD::MAV_CMD_NAV_WAYPOINT;
            jsonObjectItem["autoContinue"] = true;
            jsonObjectItem["doJumpId"] = i;
            Waypoint waypointTemp = listWaypoints.at(i);
            QJsonArray rgParams =  { 0, 0, 0, QJsonValue(), waypointTemp.latitude, waypointTemp.longitude, waypointTemp.altitude};
            jsonObjectItem["params"] = rgParams;
            jsonObjectItem["AltitudeMode"] = AltitudeMode::AltitudeModeRelative;
            jsonObjectItem["Altitude"] =  50.0;
            jsonObjectItem["AMSLAltAboveTerrain"] = 30.0;
            missionItems.append(jsonObjectItem);
        }
    }
    else if (type == REGION || type == SURVEY)
    {
        if (listWaypoints.count() < 4)
            return -2;
        QJsonObject jsonObjectItem1;
        jsonObjectItem1["autoContinue"] = true;
        jsonObjectItem1["command"] = 530;
        jsonObjectItem1["doJumpId"] = 1;
        jsonObjectItem1["frame"] = MAV_FRAME::MAV_FRAME_MISSION;
        QJsonArray rgParams =  { 0, 2, QJsonValue(), QJsonValue(), QJsonValue(), QJsonValue(), QJsonValue()};
        jsonObjectItem1["params"] = rgParams;
        jsonObjectItem1["type"] = "SimpleItem";
        missionItems.append(jsonObjectItem1);

        QJsonObject jsonObjectItem2;
        QJsonObject jsonObjectItem2_1;
        QJsonObject jsonObjectItem2_1_1;
        jsonObjectItem2_1_1["AdjustedFootprintFrontal"] = 15.424999999999997;
        jsonObjectItem2_1_1["AdjustedFootprintSide"] = 20.566666666666663;
        jsonObjectItem2_1_1["CameraName"] = QStringLiteral("自定义相机");
        jsonObjectItem2_1_1["DistanceToSurface"] = 50;
        jsonObjectItem2_1_1["DistanceToSurfaceRelative"] = true;
        jsonObjectItem2_1_1["FixedOrientation"] = false;
        jsonObjectItem2_1_1["FocalLength"] = 4.5;
        jsonObjectItem2_1_1["FrontalOverlap"] = 70;
        jsonObjectItem2_1_1["ImageDensity"] = 1.7138888888888888;
        jsonObjectItem2_1_1["ImageHeight"] = 3000;
        jsonObjectItem2_1_1["ImageWidth"] = 4000;
        jsonObjectItem2_1_1["Landscape"] = true;
        jsonObjectItem2_1_1["MinTriggerInterval"] = 0;
        jsonObjectItem2_1_1["SensorHeight"] = 4.55;
        jsonObjectItem2_1_1["SensorWidth"] = 6.17;
        jsonObjectItem2_1_1["SideOverlap"] = 70;
        jsonObjectItem2_1_1["ValueSetIsDistance"] = true;
        jsonObjectItem2_1_1["version"] = 1;
        jsonObjectItem2_1["CameraCalc"] = jsonObjectItem2_1_1;

        jsonObjectItem2_1["CameraShots"] = 152;
        jsonObjectItem2_1["CameraTriggerInTurnAround"] = true;
        jsonObjectItem2_1["FollowTerrain"] = false;
        jsonObjectItem2_1["HoverAndCapture"] = false;
        jsonObjectItem2_1["Items"] = QJsonArray();
        jsonObjectItem2_1["Refly90Degrees"] = false;
        jsonObjectItem2_1["TurnAroundDistance"] = 20;
        jsonObjectItem2_1["VisualTransectPoints"] = QJsonArray();
        jsonObjectItem2_1["version"] = 1;
        jsonObjectItem2["TransectStyleComplexItem"] = jsonObjectItem2_1;

        jsonObjectItem2["angle"] = 40;
        jsonObjectItem2["complexItemType"] = "survey";
        jsonObjectItem2["entryLocation"] = 0;
        jsonObjectItem2["flyAlternateTransects"] = false;
        QJsonArray jsonObjectItem2_2;
        for (int i = 1; i < listWaypoints.count(); i++)
        {
            jsonObjectItem2_2.append(QJsonArray({listWaypoints.at(i).latitude, listWaypoints.at(i).longitude}));
        }
        jsonObjectItem2["polygon"] = jsonObjectItem2_2;
        jsonObjectItem2["splitConcavePolygons"] = false;
        jsonObjectItem2["type"] = "ComplexItem";
        jsonObjectItem2["version"] = 5;
        missionItems.append(jsonObjectItem2);
    }
    missionJson["items"] = missionItems;
    planJson["mission"] = missionJson;

    QJsonObject fenceJson;
    fenceJson["version"] = 2;
    QJsonArray jsonPolygonArray;
    fenceJson["polygons"] = jsonPolygonArray;
    QJsonArray jsonCircleArray;
    fenceJson["circles"] = jsonCircleArray;
    planJson["geoFence"] = fenceJson;

    QJsonObject rallyJson;
    rallyJson["version"] = 2;
    QJsonArray rgPoints;
    rallyJson["points"] = rgPoints;
    planJson["rallyPoints"] = rallyJson;

    QJsonDocument jsonDoc(planJson);
    bool opened = jsonPlanFile.open(QIODevice::WriteOnly);
    qDebug() << jsonPlanFile.fileName() << "open" << opened;
    if (!opened)
        return 1;
    qint64 bytesWroten = jsonPlanFile.write(jsonDoc.toJson());
    qDebug() << "bytesWroten:" << bytesWroten;
    jsonPlanFile.close();

    //发送到planview.qml中调用属性_planMasterController的“打开文件”函数
    emit sigSendPlanFile(jsonPlanFile.fileName());
    listWaypoints.clear();  //上传后清除
    return 0;
}

void IIPSComm::SlotSendData(const QGCStatusData& data)
{
    sysId = data.id;
    BinaryPacket statusPacket = {new uint8_t[statusPacketLength], statusPacketId, statusPacketLength};
    memcpy(statusPacket.data, &data, static_cast<size_t>(statusPacket.dataLength));
    BinaryBuffer binaryBuffer = IIPSProtocol::BinaryPacketEncode(statusPacket);
    delete []statusPacket.data;
    /*qDebug() << "writeDatagram:" << */udpSocket.writeDatagram((char*)(binaryBuffer.data), binaryBuffer.length, iipsAddress, iipsPort);
}

void IIPSComm::SlotReceiveData()
{
    while (udpSocket.hasPendingDatagrams())
    {
        QNetworkDatagram datagram = udpSocket.receiveDatagram(BinaryBufferLength - receiveBuffer.length);
//        qDebug() << "SlotReceiveData length:" << datagram.data().length() << datagram.senderAddress() << datagram.senderPort();
        memcpy(receiveBuffer.data + receiveBuffer.length, datagram.data().data(), datagram.data().length());
        receiveBuffer.length += datagram.data().size();
        while(true)
        {
            receivePacket = IIPSProtocol::BinaryBufferDecode(&receiveBuffer);
            if (receivePacket.id == IdError)
                return;
            switch (receivePacket.id)
            {
            case HEARTBEAT:
                if (!iipsConnected)
                {
                    iipsConnected = true;
                    emit iipsConnectedChanged();
                    qDebug() << "show";
                }
                timerIipsConnect.start(iipsTimeOut * 1000);
                break;
            case STANDBY:
                qgcApp()->showMessage("IIPS command:Standby!");
            case LINE:
                if (GetWayPoints(LINE))
                    SendAck();
                break;
            case SURVEY:
            case REGION:
                if (GetWayPoints(REGION))
                    SendAck();
                break;
            case FOLLOW:
                qgcApp()->showMessage("IIPS command: Follow target not ready!");
                break;
            default:
                break;
            }
            delete[] receivePacket.data; // Ensure that you free the packet when you done with it or you will leak memory
        }
    }
}

void IIPSComm::SlotTestTimeout()
{
    data.id = 1;
    data.timestamp = 2;
    data.status = 3;
    data.latitude = 4;
    data.longitude = 5;
    data.altitude = 6;
    data.roll = 7;
    data.pitch = 8;
    data.yaw = 9;
    data.velocityNorth = 10;
    data.velocityEast = 11;
    data.velocityDown = 12;
    data.gyroscopeX = 13;
    data.gyroscopeY = 14;
    data.gyroscopeZ = 15;
//    data.accelerometerX = 16;
//    data.accelerometerY = 17;
//    data.accelerometerZ = 18;
//    data.satellitesUsed = 19;
//    data.fixType = 20;
//    data.battery = 21;
    SlotSendData(data);
}

bool IIPSComm::getIipsConnected() const
{
    return iipsConnected;
}

void IIPSComm::SendAck()
{
    BinaryPacket responsePacket = {new uint8_t[11], 129, 11};
    memcpy(responsePacket.data, &sysId, sizeof(uint16_t));
    responsePacket.data[10] = RESPONSE;
    BinaryBuffer binaryBuffer = IIPSProtocol::BinaryPacketEncode(responsePacket);
    if(udpSocket.isValid())
        udpSocket.writeDatagram((char*)(binaryBuffer.data), binaryBuffer.length, iipsAddress, iipsPort);
    delete[] responsePacket.data;
}

bool IIPSComm::GetWayPoints(PacketId type)
{
    switch (receivePacket.data[10])
    {
    case START:
        listWaypoints.clear();
        break;
    case WAYPOINT:
        uint16_t id;
        Waypoint waypoint;
        memcpy(&id, receivePacket.data, sizeof(uint16_t));

        if (sysId != id)
        {
            qgcApp()->showMessage(QString("Receiving mission to not connected vehicle %1").arg(id));
            qWarning() << "sysId mismatch!";
            return false;
        }

        memcpy(&waypoint.timestamp, receivePacket.data + 2, sizeof(double));
        memcpy(&waypoint.latitude, receivePacket.data + 11, sizeof(double));
        memcpy(&waypoint.longitude, receivePacket.data + 19, sizeof(double));
        memcpy(&waypoint.altitude, receivePacket.data + 27, sizeof(float));
        waypoint.latitude = qRadiansToDegrees(waypoint.latitude);
        waypoint.longitude = qRadiansToDegrees(waypoint.longitude);
        if (listWaypoints.count() > 0 && listWaypoints.last().timestamp > waypoint.timestamp)
        {
            qWarning() << "new waypoint timestamp is smaller than last one";
            delete[] receivePacket.data;
        }
        listWaypoints.append(waypoint);
//        qDebug() << "listWaypoints.append(waypoint)" << listWaypoints.count();
        break;
    case END:
        SaveWaypointsToJsonFile(type);
//        qDebug() << "END SaveWaypointsToJsonFile(type)" << type;
        break;
    default:
        break;
    }
    return true;
}



Vehicle* IIPSComm::getVehicle() const
{
    return vehicle;
}

void IIPSComm::setVehicle(Vehicle* value)
{
    vehicle = value;
}

void IIPSComm::AppendVehicle(Vehicle* value)
{
    if (!vehicles.contains(value))
        vehicles.append(value);
}

void IIPSComm::RemoveVehicle(Vehicle* value)
{
    for (int i=0; i<vehicles.count(); i++)
    {
        if (vehicles[i] == value) {
            vehicles.removeAt(i);
            break;
        }
    }
}
