/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QObject>
#include <QString>
#include <QtQml>
#include <QTextStream>
#include <QJsonObject>
#include <QGeoCoordinate>

#include "QGCMAVLink.h"
#include "QGC.h"
#include "QmlObjectListModel.h"
#include "Fact.h"
#include "QGCLoggingCategory.h"
#include "QmlObjectListModel.h"

#include "ShenHangVehicleData.h"
#include "ShenHangProtocol.h"
Q_DECLARE_LOGGING_CATEGORY(ItemInfoSlotLog)
class SimpleMissionItem;
#ifdef UNITTEST_BUILD
    class MissionItemTest;
#endif

// Represents a infoslot item.
class ItemInfoSlot : public QObject
{
    Q_OBJECT
    
public:
    ItemInfoSlot(const WaypointInfoSlot& infoSlot, QObject* parent = nullptr);
//    ItemInfoSlot(const ItemInfoSlot& other, QObject* parent = nullptr);

    ~ItemInfoSlot();

//    const ItemInfoSlot& operator=(const ItemInfoSlot& other);

    void save(QJsonObject& json) const;
    static bool loadInfoSlot(const QJsonObject& json, WaypointInfoSlot* infoSlot, QString& errorString);
    bool load(const QJsonObject& json, int sequenceNumber, QString& errorString);

    Q_INVOKABLE void updateSlotInfoData();
    Q_PROPERTY(bool dirty READ dirty WRITE setDirty NOTIFY dirtyChanged)
    Q_PROPERTY(int idInfoSlot READ idInfoSlot WRITE setIdInfoSlot NOTIFY idInfoSlotChanged)
    Q_PROPERTY(int idWaypoint READ idWaypoint WRITE setIdWaypoint NOTIFY idWaypointChanged)
    Q_PROPERTY(Fact* majorType READ majorType CONSTANT)
    Q_PROPERTY(Fact* minorType READ minorType CONSTANT)
    Q_PROPERTY(Fact* infoslotCount READ infoslotCount CONSTANT)
    Q_PROPERTY(Fact* lon READ lon CONSTANT)
    Q_PROPERTY(Fact* lat READ lat CONSTANT)
    Q_PROPERTY(Fact* alt READ alt CONSTANT)
    Q_PROPERTY(Fact* radiusCh READ radiusCh CONSTANT)
    Q_PROPERTY(Fact* rVGnd READ rVGnd CONSTANT)
    Q_PROPERTY(Fact* rRocDoc READ rRocDoc CONSTANT)
    Q_PROPERTY(Fact* swTd READ swTd CONSTANT)
    Q_PROPERTY(Fact* swVGnd READ swVGnd CONSTANT)
    Q_PROPERTY(Fact* rHdgDeg READ rHdgDeg CONSTANT)
    Q_PROPERTY(Fact* actDeptMainEngine READ actDeptMainEngine CONSTANT)
    Q_PROPERTY(Fact* actDeptDecelerate READ actDeptDecelerate CONSTANT)
    Q_PROPERTY(Fact* actDeptLandingGear READ actDeptLandingGear CONSTANT)
    Q_PROPERTY(Fact* actDeptPayload READ actDeptPayload CONSTANT)
    Q_PROPERTY(Fact* actDeptVehicle READ actDeptVehicle CONSTANT)
    Q_PROPERTY(Fact* actDeptMnvr READ actDeptMnvr CONSTANT)
    Q_PROPERTY(Fact* actArrvlMainEngine READ actArrvlMainEngine CONSTANT)
    Q_PROPERTY(Fact* actArrvlDecelerate READ actArrvlDecelerate CONSTANT)
    Q_PROPERTY(Fact* actArrvlLandingGear READ actArrvlLandingGear CONSTANT)
    Q_PROPERTY(Fact* actArrvlPayload READ actArrvlPayload CONSTANT)
    Q_PROPERTY(Fact* actArrvlVehicle READ actArrvlVehicle CONSTANT)
    Q_PROPERTY(Fact* actArrvlMnvr READ actArrvlMnvr CONSTANT)
    Q_PROPERTY(Fact* swCondAuto READ swCondAuto CONSTANT)
    Q_PROPERTY(Fact* swCondSpeedSource READ swCondSpeedSource CONSTANT)
    Q_PROPERTY(Fact* swCondHorizontalError READ swCondHorizontalError CONSTANT)
    Q_PROPERTY(Fact* swCondHeight READ swCondHeight CONSTANT)
    Q_PROPERTY(Fact* swCondSpeed READ swCondSpeed CONSTANT)
    Q_PROPERTY(Fact* swCondLongitudinalMotion READ swCondLongitudinalMotion CONSTANT)
    Q_PROPERTY(Fact* swCondReserved READ swCondReserved CONSTANT)
    Q_PROPERTY(Fact* actInFlt READ actInFlt CONSTANT)
    Q_PROPERTY(Fact* hgtCtrlMode READ hgtCtrlMode CONSTANT)
    Q_PROPERTY(Fact* hdgCtrlMode READ hdgCtrlMode CONSTANT)
    Q_PROPERTY(Fact* swAngDeg READ swAngDeg CONSTANT)
    Q_PROPERTY(Fact* swTurns READ swTurns CONSTANT)
    Q_PROPERTY(Fact* gdMode READ gdMode CONSTANT)
    Q_PROPERTY(Fact* mnvrStyEndMode READ mnvrStyEndMode CONSTANT)
    Q_PROPERTY(Fact* mnvrStyTurn READ mnvrStyTurn CONSTANT)
    Q_PROPERTY(Fact* mnvrStyReserved READ mnvrStyReserved CONSTANT)
    Q_PROPERTY(Fact* transSty READ transSty CONSTANT)
    Q_PROPERTY(Fact* reserved0 READ reserved0 CONSTANT)
    Q_PROPERTY(Fact* reserved1 READ reserved1 CONSTANT)

    const bool dirty() const {return _dirty;}
    int idInfoSlot()    {return _infoSlot.idInfoSlot;}
    int idWaypoint()    {return _infoSlot.idWp;}
    int idBank()        {return _infoSlot.idBank;}
    int tyInfoSlot()    {return _infoSlot.typeInfoSlot;}

    void setDirty(bool dirty);
    void setIdInfoSlot(uint16_t id);
    void setIdWaypoint(uint16_t index);
    void setIdBank(uint16_t index);
    void setTyInfoSlot(uint16_t type)   {_infoSlot.typeInfoSlot = type;}

    Fact* majorType()               { return &_majorType;}
    Fact* minorType()               { return &_minorType;}
    Fact* infoslotCount()           { return &_infoslotCount;}
    Fact* lon()                     { return &_lon;}
    Fact* lat()                     { return &_lat;}
    Fact* alt()                     { return &_alt;}
    Fact* radiusCh()                { return &_radiusCh;}
    Fact* rVGnd()                   { return &_rVGnd;}
    Fact* rRocDoc()                 { return &_rRocDoc;}
    Fact* swTd()                    { return &_swTd;}
    Fact* swVGnd()                  { return &_swVGnd;}
    Fact* rHdgDeg()                 { return &_rHdgDeg;}
    Fact* actDeptMainEngine()       { return &_actDeptMainEngine;}
    Fact* actDeptDecelerate()       { return &_actDeptDecelerate;}
    Fact* actDeptLandingGear()      { return &_actDeptLandingGear;}
    Fact* actDeptPayload()          { return &_actDeptPayload;}
    Fact* actDeptVehicle()          { return &_actDeptVehicle;}
    Fact* actDeptMnvr()             { return &_actDeptMnvr;}
    Fact* actArrvlMainEngine()      { return &_actArrvlMainEngine;}
    Fact* actArrvlDecelerate()      { return &_actArrvlDecelerate;}
    Fact* actArrvlLandingGear()     { return &_actArrvlLandingGear;}
    Fact* actArrvlPayload()         { return &_actArrvlPayload;}
    Fact* actArrvlVehicle()         { return &_actArrvlVehicle;}
    Fact* actArrvlMnvr()            { return &_actArrvlMnvr;}
    Fact* swCondAuto()              { return &_swCondAuto;}
    Fact* swCondSpeedSource()       { return &_swCondSpeedSource;}
    Fact* swCondHorizontalError()   { return &_swCondHorizontalError;}
    Fact* swCondHeight()            { return &_swCondHeight;}
    Fact* swCondSpeed()             { return &_swCondSpeed;}
    Fact* swCondLongitudinalMotion(){ return &_swCondLongitudinalMotion;}
    Fact* swCondReserved()          { return &_swCondReserved;}
    Fact* actInFlt()                { return &_actInFlt;}
    Fact* hgtCtrlMode()             { return &_hgtCtrlMode;}
    Fact* hdgCtrlMode()             { return &_hdgCtrlMode;}
    Fact* swAngDeg()                { return &_swAngDeg;}
    Fact* swTurns()                 { return &_swTurns;}
    Fact* gdMode()                  { return &_gdMode;}
    Fact* mnvrStyEndMode()          { return &_mnvrStyEndMode;}
    Fact* mnvrStyTurn()             { return &_mnvrStyTurn;}
    Fact* mnvrStyReserved()         { return &_mnvrStyReserved;}
    Fact* transSty()                { return &_transSty;}
    Fact* reserved0()               { return &_reserved0;}
    Fact* reserved1()               { return &_reserved1;}

    WaypointInfoSlot infoSlot() const;

signals:
    void idInfoSlotChanged();
    void idWaypointChanged();

    void isCurrentItemChanged       (bool isCurrentItem);
    void sequenceNumberChanged      (int sequenceNumber);
    void specifiedFlightSpeedChanged(double flightSpeed);
    void specifiedGimbalYawChanged  (double gimbalYaw);
    void specifiedGimbalPitchChanged(double gimbalPitch);
    void dirtyChanged               (bool dirty);
public slots:
    void _setDirty(bool dirty);
private slots:
#if 0
    /******************** infoslot类型 ty_infosl ********************/
    void _setMajorType();
    void _setMinorType();

    /******************** 离开动作字段，双字节无符号整型 act_dept ********************/
    void _setActDeptMainEngine();
    void _setActDeptDecelerate();
    void _setActDeptLandingGear();
    void _setActDeptPayload();
    void _setActDeptVehicle();
    void _setActDeptMnvr();

    /******************** 离开动作字段，双字节无符号整型 act_arrvl ********************/
    void _setActArrvlMainEngine();
    void _setActArrvlDecelerate();
    void _setActArrvlLandingGear();
    void _setActArrvlPayload();
    void _setActArrvlVehicle();
    void _setActArrvlMnvr();

    /******************** 切换条件设置，双字节无符号整型 sw_cond_set ********************/
    void _setSwCondAuto();
    void _setSwCondSpeedSource();
    void _setSwCondHorizontalError();
    void _setSwCondHeight();
    void _setSwCondSpeed();
    void _setSwCondLongitudinalMotion();
    void _setSwCondReserved();

    /******************** 高度和航向控制模式字段 hgt_hdg_ctrl_mode ********************/
    void _setHgtCtrlMode();
    void _setHdgCtrlMode();

    /******************** 机动风格字段，单字节无符号整型，定义如下 mnvr_sty ********************/
    void _setMnvrStyEndMode();
    void _setMnvrStyTurn();
    void _setMnvrStyReserved();
#endif

private:
    void _connectSignals        (void);
    void _setupMetaData         (void);

    int     _sequenceNumber;
    int     _doJumpId;

    /******************** infoslot类型，定义如下表所示 ********************/
    Fact    _majorType;        // bit0：3。0：非法；1：主要infoslot；2：补充infoslot；2~15：保留
    Fact    _minorType;        // bit4：11。_majorType为主要时，0：基本；1：动态
    Fact    _infoslotCount;    // bit12：15。航点占用infoslot总数

    Fact    _lon;               // 航路点所在位置对应的经度，双精度浮点数，单位为度，取值为+-180之间
    Fact    _lat;               // 航路点所在位置对应的纬度，双精度浮点数，单位为度，取值为+-90之间。
    Fact    _alt;               // 航路点所在位置对应的海拔高度，单精度浮点数，单位为米。
    Fact    _radiusCh;         // 特征圆半径，单精度浮点数，单位为米。对于直线型航线的航路点，该圆半径为切换误差半径，取正值；对于盘旋型航路点，该半径为盘旋半径，设为正值时俯视顺时针盘旋；设为负值时俯视逆时针盘旋。
    Fact    _rVGnd;           // 参考飞行速度，单精度浮点数，单位为米每秒，非负。
    Fact    _rRocDoc;         // 参考爬升速度或梯度，单精度浮点数，正值爬升，负值下降，作为爬升速度时单位为米每秒，作为爬升梯度时无量纲。
    Fact    _swTd;             // 切换条件延迟时长，单精度浮点数，非负。
    Fact    _swVGnd;      // 切换条件速度阈值，双字节无符号整型，一位小数，单位米每秒。
    Fact    _rHdgDeg;    // 切换方位角条件阈值，双字节有号整型，两位小数，单位为度。

    /******************** 离开动作字段，双字节无符号整型，定义如下 ********************/
    Fact    _actDeptMainEngine;      // bit0：1。主引擎操作,00：保持当前状态/取消加力；01：关机；10：启动；11：加力
    Fact    _actDeptDecelerate;       // bit2：3。减速机构操作,00：保持当前状态；01：收起；10：展开；11：展开并开伞
    Fact    _actDeptLandingGear;     // bit4：5。起落装置,00：保持当前状态；01：收起；10：展开；11：展开并刹车
    Fact    _actDeptPayload;          // bit6：7。默认载荷操作,00：保持当前状态；01：打开；10：关闭；11：触发
    Fact    _actDeptVehicle;          // bit8：10。载具操作,000：保持当前状态；其他：保留
    Fact    _actDeptMnvr;         // bit11：15。00000：保持当前状态；其他：保留

    /******************** 离开动作字段，双字节无符号整型，定义如下 ********************/
    Fact    _actArrvlMainEngine;      // bit0：1。主引擎操作,00：保持当前状态/取消加力；01：关机；10：启动；11：加力
    Fact    _actArrvlDecelerate;       // bit2：3。减速机构操作,00：保持当前状态；01：收起；10：展开；11：展开并开伞
    Fact    _actArrvlLandingGear;     // bit4：5。起落装置,00：保持当前状态；01：收起；10：展开；11：展开并刹车
    Fact    _actArrvlPayload;          // bit6：7。默认载荷操作,00：保持当前状态；01：打开；10：关闭；11：触发
    Fact    _actArrvlVehicle;          // bit8：10。载具操作,000：保持当前状态；其他：保留
    Fact    _actArrvlMnvr;         // bit11：15。00000：保持当前状态；其他：保留

    /******************** 切换条件设置，双字节无符号整型，定义如下 ********************/
    Fact    _swCondAuto;       // bit0。自动切换开关，0：自动切换关；1：自动切换开
    Fact    _swCondSpeedSource;      // bit1。速度信息源选择，0：使用地速进行判断；1：使用空速进行判断
    Fact    _swCondHorizontalError;  // bit2：3。平面误差条件，00：平面误差判断不启用；01：在误差圆内切换（直线型航线）或双侧切换（盘旋航线）；10：进入误差圆后切换（直线型航线）或单侧切换（盘旋航线）；11：绊马绳
    Fact    _swCondHeight;            // bit4：5。高度条件，00：高度判断条件不启用；01：低于预设高度切换；10：高于预设高度切换；11：预设高度保持误差在预设阈值内切换
    Fact    _swCondSpeed;             // bit6：7。速度条件，00：速度判断条件不启用；01：低于预设速度切换；10：高于预设速度切换；11：预设速度保持误差在预设阈值内切换
    Fact    _swCondLongitudinalMotion;// bit8：9。纵向运动条件，00：纵向运动判断条件不启用；01：爬升率/梯度大于设置值切换；10：爬升率/梯度小于设置值切换；11：爬升率/梯度误差保持在
    Fact    _swCondReserved;          // bit10：15。保留字段

    Fact    _actInFlt;                 // 飞行中动作字段，单字节无符号整型。


    /******************** 高度和航向控制模式字段，单字节无符号整型，定义如下 ********************/
    Fact _hgtCtrlMode; // bit0：3。高度控制模式。0000：高度控制不起效；0001：保持当前参考高度；0010：最快速过渡至航路点高度；0011：插值过渡；0100：瞄准；0101：动态目标跟踪；0110：爬升率控制；0111：爬升梯度控制；1000：滑翔速度保持（仅固定翼）；其他：保留；
    Fact _hdgCtrlMode; // bit4：7。航向控制模式。0000：航向控制不起效或尾锁；0001：对准路径；0010：对准目标航点；0011：对准航迹；0100：对准其他目标点；0101：依航路点信息保持航向角；0110：依照航路点信息保持指向特征圆外

    Fact _swAngDeg;   // 盘旋方位角切换条件字段，单字节有符号整型，单位为度。
    Fact _swTurns;     // 盘旋切换圈数字段，单字节无符号整型，单位为圈。
    Fact _gdMode;      // 制导模式字段，单字节无符号整型。0：to-point；1：follow line；2：loiter

    /******************** 机动风格字段，单字节无符号整型，定义如下 ********************/
    Fact _mnvrStyEndMode;        // bit0。结束模式，0：正常；1：直前；
    Fact _mnvrStyTurn;            // bit1。转弯方式	0：btt；1：stt；
    Fact _mnvrStyReserved;        // bit2：7。保留

    Fact _transSty;                // 过渡风格字段，单字节无符号整型。0：直接过渡；1：dubins过渡；其他：保留；
    Fact _reserved0;                // 保留变量，单字节无符号整型。
    Fact _reserved1;                // 保留变量，单字节无符号整型。


    // Keys for Json save
    static const char*  _jsonIdBank;
    static const char*  _jsonIdWp;
    static const char*  _jsonIdInfosl;
    static const char*  _jsonTypeInfosl;
    static const char*  _jsonLon;
    static const char*  _jsonLat;
    static const char*  _jsonAlt;
    static const char*  _jsonRadiusCh;
    static const char*  _jsonRVGnd;
    static const char*  _jsonRRocDoc;
    static const char*  _jsonSwTd;
    static const char*  _jsonSwVGnd10x;
    static const char*  _jsonRHdgDeg100x;
    static const char*  _jsonActDept;
    static const char*  _jsonActArrvl;
    static const char*  _jsonSwCondSet;
    static const char*  _jsonActInFlt;
    static const char*  _jsonHgtHdgCtrlMode;
    static const char*  _jsonSwAngDeg;
    static const char*  _jsonSwTurns;
    static const char*  _jsonGdMode;
    static const char*  _jsonMnvrSty;
    static const char*  _jsonTransSty;
    static const char*  _jsonReserved0;
    static const char*  _jsonReserved1;

    friend class SimpleMissionItem;

    WaypointInfoSlot _infoSlot;
    bool _dirty = false;
#ifdef UNITTEST_BUILD
    friend class MissionItemTest;
#endif

};
