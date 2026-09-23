/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <QtCore/QAbstractListModel>
#include <QtCore/QHash>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QString>

/// DJI 云「获取航线列表」接口返回的 data.list 的模型封装。
///
/// 直接以服务端 JSON 为数据源，不再为每个字段另造结构体——列表接口的字段
/// 全部是只读展示用的，字段增减时这里只需补一个角色。
/// QML 通过角色名（name / favorited / updateTimeText ...）访问。
class DjiWaylineListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    explicit DjiWaylineListModel(QObject* parent = nullptr);

    enum Roles {
        WaylineIdRole = Qt::UserRole + 1,
        NameRole,
        FavoritedRole,
        DroneModelKeyRole,
        PayloadModelKeysRole,
        TemplateTypesRole,
        ActionTypeRole,
        UpdateTimeRole,     // 服务端毫秒时间戳
        UpdateTimeTextRole, // 已格式化为本地时间文本
        UserNameRole,
        StartLatitudeRole,
        StartLongitudeRole,
    };
    Q_ENUM(Roles)

    /// 用列表接口的 data.list 整体替换内容
    Q_INVOKABLE void setList(const QJsonArray& list);
    /// 收藏状态本地先行变更（乐观更新），失败时调用方再改回来
    Q_INVOKABLE void setFavorited(int row, bool favorited);

    Q_INVOKABLE QString idAt(int row) const;
    Q_INVOKABLE QString nameAt(int row) const;
    Q_INVOKABLE QJsonObject itemAt(int row) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

private:
    bool isValidRow(int row) const { return row >= 0 && row < _list.size(); }

    QJsonArray _list;
};
