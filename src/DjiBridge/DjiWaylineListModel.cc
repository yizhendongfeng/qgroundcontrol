/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "DjiWaylineListModel.h"

#include <QtCore/QDateTime>
#include <QtCore/QJsonValue>
#include <QtCore/QVariantList>

// 日志用默认类别（qInfo/qWarning），与本目录其他文件一致：
// QGCLogging::msgHandler 会把没有开启 debug 的自定义类别整条丢掉，
// 用 QGC_LOGGING_CATEGORY 建类别反而默认一条日志都看不到。

DjiWaylineListModel::DjiWaylineListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

void DjiWaylineListModel::setList(const QJsonArray& list)
{
    beginResetModel();
    _list = list;
    endResetModel();
}

void DjiWaylineListModel::setFavorited(int row, bool favorited)
{
    if (!isValidRow(row)) {
        return;
    }

    QJsonObject item = _list.at(row).toObject();
    if (item.value(QStringLiteral("favorited")).toBool() == favorited) {
        return;
    }
    item.insert(QStringLiteral("favorited"), favorited);
    _list.replace(row, item);

    const QModelIndex idx = index(row, 0);
    emit dataChanged(idx, idx, {FavoritedRole});
}

QString DjiWaylineListModel::idAt(int row) const
{
    return isValidRow(row) ? _list.at(row).toObject().value(QStringLiteral("id")).toString() : QString();
}

QString DjiWaylineListModel::nameAt(int row) const
{
    return isValidRow(row) ? _list.at(row).toObject().value(QStringLiteral("name")).toString() : QString();
}

QJsonObject DjiWaylineListModel::itemAt(int row) const
{
    return isValidRow(row) ? _list.at(row).toObject() : QJsonObject();
}

int DjiWaylineListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : _list.size();
}

QVariant DjiWaylineListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || !isValidRow(index.row())) {
        return {};
    }

    const QJsonObject item = _list.at(index.row()).toObject();

    switch (role) {
    case WaylineIdRole:
        return item.value(QStringLiteral("id")).toString();
    case NameRole:
        return item.value(QStringLiteral("name")).toString();
    case FavoritedRole:
        return item.value(QStringLiteral("favorited")).toBool();
    case DroneModelKeyRole:
        return item.value(QStringLiteral("drone_model_key")).toString();
    case PayloadModelKeysRole: {
        QStringList keys;
        for (const QJsonValue& v : item.value(QStringLiteral("payload_model_keys")).toArray()) {
            keys.append(v.toString());
        }
        return keys;
    }
    case TemplateTypesRole: {
        QVariantList types;
        for (const QJsonValue& v : item.value(QStringLiteral("template_types")).toArray()) {
            types.append(v.toInt());
        }
        return types;
    }
    case ActionTypeRole:
        return item.value(QStringLiteral("action_type")).toInt();
    case UpdateTimeRole: {
        const QJsonValue v = item.value(QStringLiteral("update_time"));
        return v.isDouble() ? QVariant(static_cast<qint64>(v.toDouble())) : QVariant();
    }
    case UpdateTimeTextRole: {
        const QJsonValue v = item.value(QStringLiteral("update_time"));
        if (!v.isDouble()) {
            return QStringLiteral("--.--");
        }
        return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(v.toDouble()))
            .toString(QStringLiteral("yyyy-MM-dd hh:mm"));
    }
    case UserNameRole:
        return item.value(QStringLiteral("user_name")).toString();
    case StartLatitudeRole:
        // 字段名照抄 DJI：start_wayline_point / start_lontitude（那个 lontitude
        // 是官方的拼写，别"修正"）。
        //
        // 注意：当前后台的列表接口**不回**这个字段（对过 /v3/api-docs），也
        // 没有 GET 详情的接口，所以这两个 role 现在恒为 0。留着是为了万一
        // 后台补上字段就能直接用；界面侧已经不依赖它了。
        return item.value(QStringLiteral("start_wayline_point")).toObject()
            .value(QStringLiteral("start_latitude")).toDouble();
    case StartLongitudeRole:
        return item.value(QStringLiteral("start_wayline_point")).toObject()
            .value(QStringLiteral("start_lontitude")).toDouble();
    default:
        return {};
    }
}

QHash<int, QByteArray> DjiWaylineListModel::roleNames() const
{
    return {
        {WaylineIdRole,       "waylineId"},
        {NameRole,            "name"},
        {FavoritedRole,       "favorited"},
        {DroneModelKeyRole,   "droneModelKey"},
        {PayloadModelKeysRole,"payloadModelKeys"},
        {TemplateTypesRole,   "templateTypes"},
        {ActionTypeRole,      "actionType"},
        {UpdateTimeRole,      "updateTime"},
        {UpdateTimeTextRole,  "updateTimeText"},
        {UserNameRole,        "userName"},
        {StartLatitudeRole,   "startLatitude"},
        {StartLongitudeRole,  "startLongitude"},
    };
}
