#pragma once

#include <QHash>
#include <QString>
#include <QVector>
#include <memory>

namespace wintools::disksentinel {

struct DiskNode {
    QString  name;
    QString  path;
    bool     isDir     = false;
    qint64   size      = 0;
    int      itemCount = 0;
    bool     scanError = false;

    DiskNode*                              parent   = nullptr;
    QVector<std::shared_ptr<DiskNode>>     children;

    double fractionOfParent() const {
        if (!parent || parent->size == 0) return 0.0;
        return static_cast<double>(size) / static_cast<double>(parent->size);
    }

    static QString prettySize(qint64 bytes) {
        if (bytes < 0) return QStringLiteral("—");
        if (bytes < 1024LL)
            return QString::number(bytes) + QStringLiteral(" B");
        if (bytes < 1024LL * 1024)
            return QString::number(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
        if (bytes < 1024LL * 1024 * 1024)
            return QString::number(bytes / (1024.0 * 1024), 'f', 1) + QStringLiteral(" MB");
        if (bytes < 1024LL * 1024 * 1024 * 1024)
            return QString::number(bytes / (1024.0 * 1024 * 1024), 'f', 2) + QStringLiteral(" GB");
        return QString::number(bytes / (1024.0 * 1024 * 1024 * 1024), 'f', 2) + QStringLiteral(" TB");
    }

    static const QString& category(const QString& filename) {

        static const QHash<QString, QString> map = [] {
            QHash<QString, QString> h;
            h.reserve(80);
            static constexpr struct { const char* ext; const char* cat; } table[] = {

                {"jpg","image"},{"jpeg","image"},{"png","image"},{"gif","image"},
                {"bmp","image"},{"svg","image"},{"webp","image"},{"ico","image"},
                {"tiff","image"},{"heic","image"},{"raw","image"},{"arw","image"},

                {"mp4","video"},{"mkv","video"},{"avi","video"},{"mov","video"},
                {"wmv","video"},{"flv","video"},{"webm","video"},{"m4v","video"},
                {"mpg","video"},{"mpeg","video"},

                {"mp3","audio"},{"flac","audio"},{"wav","audio"},{"aac","audio"},
                {"ogg","audio"},{"m4a","audio"},{"wma","audio"},{"opus","audio"},
                {"aiff","audio"},

                {"pdf","document"},{"doc","document"},{"docx","document"},
                {"xls","document"},{"xlsx","document"},{"ppt","document"},
                {"pptx","document"},{"odt","document"},{"txt","document"},
                {"md","document"},{"rtf","document"},{"csv","document"},

                {"zip","archive"},{"rar","archive"},{"7z","archive"},
                {"tar","archive"},{"gz","archive"},{"bz2","archive"},
                {"xz","archive"},{"iso","archive"},{"cab","archive"},
                {"zst","archive"},{"lz4","archive"},

                {"cpp","code"},{"h","code"},{"hpp","code"},{"c","code"},
                {"cs","code"},{"py","code"},{"js","code"},
                {"java","code"},{"kt","code"},{"rs","code"},{"go","code"},
                {"rb","code"},{"php","code"},{"html","code"},{"css","code"},
                {"xml","code"},{"json","code"},{"yaml","code"},{"yml","code"},
                {"lua","code"},{"swift","code"},{"toml","code"},

                {"exe","executable"},{"dll","executable"},{"so","executable"},
                {"dylib","executable"},{"msi","executable"},{"apk","executable"},
                {"lib","executable"},{"a","executable"},
            };
            for (const auto& e : table)
                h.insert(QString::fromLatin1(e.ext), QString::fromLatin1(e.cat));
            return h;
        }();

        static const QString other = QStringLiteral("other");
        const QString ext = filename.section(QLatin1Char('.'), -1).toLower();
        auto it = map.constFind(ext);
        return it != map.constEnd() ? it.value() : other;
    }
};

}
