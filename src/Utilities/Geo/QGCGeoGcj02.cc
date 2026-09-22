/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "QGCGeoGcj02.h"

#include <cmath>

namespace QGCGeo {

namespace {

// 常量与 DJI 官方 Web 控制台的 coordtransform.js 完全相同，逐个抄下来不要"化简"：
constexpr double PI = 3.1415926535897932384626;
constexpr double kEllipsoidSemiMajorAxis = 6378245.0;              // 克拉索夫斯基椭球长半轴（米）
constexpr double kEllipsoidEccentricitySq = 0.00669342162296594323;  // 第一偏心率平方

/// 参数是"相对于 (105°E, 35°N) 的经纬度差"，不是绝对方位
double transformLat(double lng, double lat)
{
    double ret = -100.0 + 2.0 * lng + 3.0 * lat + 0.2 * lat * lat + 0.1 * lng * lat
               + 0.2 * std::sqrt(std::abs(lng));
    ret += (20.0 * std::sin(6.0 * lng * PI) + 20.0 * std::sin(2.0 * lng * PI)) * 2.0 / 3.0;
    ret += (20.0 * std::sin(lat * PI) + 40.0 * std::sin(lat / 3.0 * PI)) * 2.0 / 3.0;
    ret += (160.0 * std::sin(lat / 12.0 * PI) + 320.0 * std::sin(lat * PI / 30.0)) * 2.0 / 3.0;
    return ret;
}

double transformLng(double lng, double lat)
{
    double ret = 300.0 + lng + 2.0 * lat + 0.1 * lng * lng + 0.1 * lng * lat
               + 0.1 * std::sqrt(std::abs(lng));
    ret += (20.0 * std::sin(6.0 * lng * PI) + 20.0 * std::sin(2.0 * lng * PI)) * 2.0 / 3.0;
    ret += (20.0 * std::sin(lng * PI) + 40.0 * std::sin(lng / 3.0 * PI)) * 2.0 / 3.0;
    ret += (150.0 * std::sin(lng / 12.0 * PI) + 300.0 * std::sin(lng / 30.0 * PI)) * 2.0 / 3.0;
    return ret;
}

/// 某个 WGS84 点位上的 GCJ-02 偏移量（度）。正负号按 JS 版原样：
/// 把偏移量加到 WGS84 上得到 GCJ-02，减回去得到 WGS84。
void gcj02Offset(double longitude, double latitude, double &offsetLongitude, double &offsetLatitude)
{
    const double deltaLat = transformLat(longitude - 105.0, latitude - 35.0);
    const double deltaLng = transformLng(longitude - 105.0, latitude - 35.0);

    const double radLat = latitude / 180.0 * PI;
    const double magic = 1.0 - kEllipsoidEccentricitySq * std::sin(radLat) * std::sin(radLat);
    const double sqrtMagic = std::sqrt(magic);

    offsetLatitude  = (deltaLat * 180.0) / ((kEllipsoidSemiMajorAxis * (1.0 - kEllipsoidEccentricitySq))
                                            / (magic * sqrtMagic) * PI);
    offsetLongitude = (deltaLng * 180.0) / (kEllipsoidSemiMajorAxis / sqrtMagic * std::cos(radLat) * PI);
}

}  // namespace

bool isOutOfChina(double longitude, double latitude)
{
    return (longitude < 72.004 || longitude > 137.8347) || (latitude < 0.8293 || latitude > 55.8271);
}

void wgs84ToGcj02(double &longitude, double &latitude)
{
    if (isOutOfChina(longitude, latitude)) {
        return;
    }

    double offsetLongitude = 0.0;
    double offsetLatitude = 0.0;
    gcj02Offset(longitude, latitude, offsetLongitude, offsetLatitude);

    longitude += offsetLongitude;
    latitude  += offsetLatitude;
}

void gcj02ToWgs84(double &longitude, double &latitude)
{
    if (isOutOfChina(longitude, latitude)) {
        return;
    }

    double offsetLongitude = 0.0;
    double offsetLatitude = 0.0;
    gcj02Offset(longitude, latitude, offsetLongitude, offsetLatitude);

    // 没有反函数解析式，官方控制台的做法是"把偏移量加倍再减掉"（一次近似），照搬即可：
    // 这样它算出来的 WGS84 和我们对它算出来的 GCJ-02 严格互逆，两边图形能重合。
    const double gcjLongitude = longitude + offsetLongitude;
    const double gcjLatitude  = latitude  + offsetLatitude;
    longitude = longitude * 2.0 - gcjLongitude;
    latitude  = latitude  * 2.0 - gcjLatitude;
}

QGeoCoordinate wgs84ToGcj02(const QGeoCoordinate &coord)
{
    if (!coord.isValid()) {
        return coord;
    }

    double longitude = coord.longitude();
    double latitude = coord.latitude();
    wgs84ToGcj02(longitude, latitude);

    // 注意 QGeoCoordinate 的构造函数是（纬度, 经度），和上面 double 版的顺序正好相反
    QGeoCoordinate result(latitude, longitude);
    if (coord.type() == QGeoCoordinate::Coordinate3D) {
        result.setAltitude(coord.altitude());   // 纠偏只动水平位置，高度原样带过去
    }
    return result;
}

QGeoCoordinate gcj02ToWgs84(const QGeoCoordinate &coord)
{
    if (!coord.isValid()) {
        return coord;
    }

    double longitude = coord.longitude();
    double latitude = coord.latitude();
    gcj02ToWgs84(longitude, latitude);

    // 同 wgs84ToGcj02：QGeoCoordinate 构造函数是（纬度, 经度）
    QGeoCoordinate result(latitude, longitude);
    if (coord.type() == QGeoCoordinate::Coordinate3D) {
        result.setAltitude(coord.altitude());
    }
    return result;
}

}  // namespace QGCGeo
