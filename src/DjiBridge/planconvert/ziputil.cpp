#include "ziputil.h"

#include <QFile>
#include <QString>

// ---------------------------------------------------------------------------
// ziputil.cpp
// 基于 Qt 私有 ZIP 实现（qzipwriter / qzipreader）的轻量 ZIP 归档读写，
// 用于 DJI KMZ（本质是 ZIP）的打包与解包。支持 deflate 压缩，兼容 store，
// 仅依赖 QtCore，无第三方依赖（原独立项目实现依赖 miniz，已替换）。
// ---------------------------------------------------------------------------

#include <QtCore/private/qzipreader_p.h>
#include <QtCore/private/qzipwriter_p.h>

namespace ziputil {

bool create(const QString& path, const QList<Entry>& entries, QString* err)
{
    if (entries.isEmpty()) {
        if (err) *err = QStringLiteral("没有可写入的条目");
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (err) *err = QStringLiteral("无法创建 ZIP 文件: %1").arg(path);
        return false;
    }

    QZipWriter writer(&file);
    writer.setCompressionPolicy(QZipWriter::AutoCompress);

    for (const Entry& e : entries) {
        writer.addFile(e.name, e.data);
        if (writer.status() != QZipWriter::NoError) {
            if (err) *err = QStringLiteral("写入条目失败: %1").arg(e.name);
            writer.close();
            file.close();
            return false;
        }
    }

    writer.close();
    if (writer.status() != QZipWriter::NoError) {
        if (err) *err = QStringLiteral("ZIP 归档收尾失败");
        file.close();
        return false;
    }
    file.close();
    return true;
}

bool open(const QString& path, QList<Entry>& entries, QString* err)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("无法打开 ZIP 文件: %1").arg(path);
        return false;
    }

    QZipReader reader(&file);
    if (!reader.isReadable()) {
        if (err) *err = QStringLiteral("无法打开 ZIP 文件: %1").arg(path);
        file.close();
        return false;
    }

    const QList<QZipReader::FileInfo> infos = reader.fileInfoList();
    for (const QZipReader::FileInfo& info : infos) {
        // 跳过目录项与符号链接，只取普通文件条目。
        if (info.isDir || info.isSymLink)
            continue;

        // fileData 对 store 与 deflate 两种压缩方法都能读；解压/读取失败时返回空。
        // 条目声明非零大小但取到空数据，视为解压失败。
        const QByteArray data = reader.fileData(info.filePath);
        if (data.isEmpty() && info.size > 0) {
            if (err) *err = QStringLiteral("解压条目失败: %1").arg(info.filePath);
            reader.close();
            file.close();
            return false;
        }

        Entry e;
        e.name = info.filePath;
        e.data = data;
        entries.append(e);
    }

    reader.close();
    file.close();
    return true;
}

} // namespace ziputil
