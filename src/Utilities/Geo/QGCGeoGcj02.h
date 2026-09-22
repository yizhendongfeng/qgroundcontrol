/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

/// @file
///     @brief GCJ-02（火星坐标）与 WGS84 之间的偏移纠偏。

#pragma once

#include <QtPositioning/QGeoCoordinate>

namespace QGCGeo {

// 国内的高德/腾讯底图，以及按它们标绘出来的图形，用的都是 GCJ-02（俗称火星坐标）；
// GPS、飞控、以及 QGC 内部（QGeoCoordinate）一律是 WGS84。同一个点在两套坐标里的读数
// 能差 300~600 米，把 GCJ-02 的数字直接当 WGS84 用，整张图就会朝一个方向偏——表现就是
// "平台上的图形和本地的对不上"。
//
// 算法与 DJI Cloud API 官方 Web 控制台一致（Cloud-API-Demo-Web/src/vendors/coordtransform.js）。
// GCJ-02 <-> WGS84 之间没有解析解，业界通用做法就是这一套近似算法；它和官方控制台的取值
// 逐位一致，所以两边算出来的图形能重合。
//
// 注意：这套偏移只在中国大陆生效，境外坐标原样返回（isOutOfChina 为真时不做任何事）。
// 参数一律经度在前、纬度在后（与 GeoJSON 一致），别搞反。

/// 该点是否在中国大陆范围之外（境外不做偏移）
bool isOutOfChina(double longitude, double latitude);

/// WGS84 -> GCJ-02，原地更新（传给高德/后端之前用）
void wgs84ToGcj02(double &longitude, double &latitude);

/// GCJ-02 -> WGS84，原地更新（从高德/后端取回来之后用）
void gcj02ToWgs84(double &longitude, double &latitude);

/// 坐标版本的 WGS84 -> GCJ-02
QGeoCoordinate wgs84ToGcj02(const QGeoCoordinate &coord);

/// 坐标版本的 GCJ-02 -> WGS84
QGeoCoordinate gcj02ToWgs84(const QGeoCoordinate &coord);

} // namespace QGCGeo
