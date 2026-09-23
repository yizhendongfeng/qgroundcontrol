#pragma once

#include <QByteArray>
#include <QList>
#include <QString>

// ---------------------------------------------------------------------------
// ziputil.h
// 轻量 ZIP 归档读写（支持 store 与 deflate 两种压缩方法），仅依赖 QtCore。
// 用于 DJI KMZ（本质是 ZIP）的打包与解包，兼容 Qt5 / Qt6，无第三方依赖。
// ---------------------------------------------------------------------------

namespace ziputil {

struct Entry {
    QString name;          // 归档内路径，如 "waylines.wpml"
    QByteArray data;
};

// 创建 ZIP 文件（使用 deflate 压缩）；成功返回 true。
bool create(const QString& path, const QList<Entry>& entries, QString* err = nullptr);

// 读取 ZIP 文件全部条目；成功返回 true。
bool open(const QString& path, QList<Entry>& entries, QString* err = nullptr);

} // namespace ziputil
