#pragma once

// DiskSentinel: disk scanner manages discovery and scanning flow.

#include "modules/disksentinel/src/core/disk_node.hpp"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QThread>
#include <atomic>
#include <memory>

namespace wintools::disksentinel {

struct ScanStats {
    int    files      = 0;
    int    dirs       = 0;
    qint64 bytes      = 0;
    qint64 elapsedMs  = 0;
};

struct ScanState {
    std::atomic<bool>    cancel  {false};
    std::atomic<int>     files   {0};
    std::atomic<int>     dirs    {0};
    std::atomic<qint64>  bytes   {0};
};

class DiskScanner : public QObject {
    Q_OBJECT

public:
    explicit DiskScanner(QObject* parent = nullptr);
    ~DiskScanner() override;

    void startScan(const QString& rootPath);
    void cancel();
    bool isRunning() const;

    void setCacheEnabled(bool enabled)  { m_cacheEnabled = enabled; }
    void setCacheTtlSeconds(int secs)   { m_cacheTtlSecs = secs; }
    void clearCache()                   { m_cache.clear(); }
    bool isCacheEnabled() const         { return m_cacheEnabled; }
    int  cacheTtlSeconds() const        { return m_cacheTtlSecs; }

signals:
    void scanStarted(const QString& path);
    void progressUpdate(int totalFiles, qint64 totalBytes, const QString& currentPath);
    void scanFinished(std::shared_ptr<wintools::disksentinel::DiskNode> root);
    void scanStats(wintools::disksentinel::ScanStats stats);
    void scanCancelled();

private:

    static void threadMain(std::shared_ptr<ScanState> state,
                           QPointer<DiskScanner>       scanner,
                           QString                     rootPath);
    static void scanDir(DiskNode* root,
                        std::shared_ptr<ScanState>  state,
                        QPointer<DiskScanner>       scanner);

    QThread*                   m_thread = nullptr;

    std::shared_ptr<ScanState> m_state;

    std::shared_ptr<DiskNode>  m_pendingCacheRoot;
    ScanStats                  m_pendingStats;

    struct CacheEntry {
        std::shared_ptr<DiskNode> root;
        QDateTime                 timestamp;
        ScanStats                 stats;
    };
    QHash<QString, CacheEntry> m_cache;
    bool                       m_cacheEnabled = true;
    int                        m_cacheTtlSecs = 300;
};

}
