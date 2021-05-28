
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
#include "SurvenlianceComm.h"

SurvenlianceComm::SurvenlianceComm(QObject *parent) : QObject(parent)
{
    fileLogger.setFileName(QTime::currentTime().toString("hh-mm-ss") + ".csv");
    qDebug() << "fileopen" << fileLogger.fileName() << fileLogger.open(QFile::WriteOnly);
    fileStream = new QTextStream(&fileLogger);
    *fileStream << "timestamp,lat,long,alt,vel\n";
    QSettings settings;
    settings.beginGroup("Survenliance");
    if(settings.contains("targetIp"))
        targetIp = settings.value("targetIp", "127.0.0.1").toString();
    else
    {
        targetIp = "127.0.0.1";
        settings.setValue("targetIp", targetIp);
    }
    targetAddress.setAddress(targetIp);
    if(settings.contains("targetPort"))
        targetPort = static_cast<uint16_t>(settings.value("targetPort", 8000).toUInt());
    else
    {
        targetPort = 8000;
        settings.setValue("targetPort", targetPort);
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
        qWarning("target udp bind to port %d failed", qgcPort);


    QTimer* timerTest = new QTimer(this);
    connect(timerTest, &QTimer::timeout, this, &SurvenlianceComm::SlotTestTimeout);
    timerTest->start(1000);

}

SurvenlianceComm::~SurvenlianceComm()
{
    QSettings settings;
    settings.beginGroup("Survenliance");
    settings.setValue("targetIp", targetIp);
    settings.setValue("targetPort", targetPort);
    settings.setValue("qgcPort", qgcPort);
    settings.endGroup();
    if (fileStream)
        fileStream->flush();
    fileLogger.close();
}

Vehicle* SurvenlianceComm::getVehicle() const
{
    return vehicle;
}

void SurvenlianceComm::setVehicle(Vehicle* value)
{
    vehicle = value;
}

void SurvenlianceComm::SlotTestTimeout()
{
//    uavStatusData.year = 1;           //年
//    uavStatusData.month = 2;          //月
//    uavStatusData.day = 3;            //日
//    uavStatusData.hour = 4;           //时
//    uavStatusData.minute = 5;         //分
//    uavStatusData.second = 6;         //秒

//    uavStatusData.longitude = 7.1234567 / 360.0 * qPow(2.0, 32);      // =经度（度.度）/360*pow(2.0,32)；
//    uavStatusData.latitude = 8.1234567 / 180.0 * qPow(2.0, 32);       // =纬度（度.度）/180*pow(2.0,32)；

//    uavStatusData.yaw = 9;                //航向（度）
//    uavStatusData.vehicleVelocity = 10;    //航速(m/s)
//    uavStatusData.windVelocityReal = 11;   //真风速(m/s)
//    uavStatusData.windDirectionReal = 12;  //真风向（度）

//    uavStatusData.airPressure = 13;        //气压（Pa）
//    uavStatusData.temperature = 14;        //温度（摄氏度）
//    uavStatusData.humidity = 15;           //湿度（%）
//    uavStatusData.windSpeedVisual = 16;    //视风速（m/s）
//    uavStatusData.windDirectionVisual = 17;//视风向（m/s）
//    uavStatusData.altitude = 18;           //高度(m)
//    uavStatusData.reserved4 = 19;
//    uavStatusData.reserved5 =20;
//    uavStatusData.reserved6 = 21;
//    uavStatusData.reserved7 = 22;
//    uavStatusData.reserved8 = 23;
    QDateTime dateTime = QDateTime::currentDateTime();
    uavStatusData.year = dateTime.date().year();           //年
    uavStatusData.month = dateTime.date().month();         //月
    uavStatusData.day = dateTime.date().day();             //日
    uavStatusData.hour = dateTime.time().hour();           //时
    uavStatusData.minute = dateTime.time().minute();       //分
    uavStatusData.second = dateTime.time().second();       //秒
    memcpy(sendBuffer + 7, &uavStatusData, 75);
    qDebug() << "writeDatagram:" << udpSocket.writeDatagram((char*)sendBuffer, 82, targetAddress, targetPort);

    *fileStream << QString::number(QDateTime::currentDateTime().toMSecsSinceEpoch()) + "," +
                   QString::number(uavStatusData.longitude * 360.0 / qPow(2.0, 32), 'f', 7) + "," +
                   QString::number(uavStatusData.latitude * 180.0 / qPow(2.0, 32), 'f', 7) + "," +
                   QString::number(uavStatusData.altitude, 'f', 1) + "," +
                   QString::number(uavStatusData.vehicleVelocity, 'f', 1) + "\n";
    fileStream->flush();
}

void SurvenlianceComm::SlotSendData(const UavStatusData& data)
{
    uavStatusData = data;
}
