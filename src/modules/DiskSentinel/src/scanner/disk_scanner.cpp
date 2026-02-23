#include "modules/disksentinel/src/scanner/disk_scanner.hpp"

#include "logger/logger.hpp"

#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QPointer>
#include <QThread>

#include <vector>

// DiskSentinel: disk scanner manages discovery and scanning flow.

namespace wintools::disksentinel {

static constexpr const char* kLog = "DiskSentinel/Scanner";

DiskScanner::DiskScanner(QObject* parent)
    : QObject(parent)
    , m_state(std::make_shared<ScanState>())
{
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                                   "DiskScanner created.");
}

DiskScanner::~DiskScanner()
{
    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                                   "DiskScanner destroying – detaching scan thread (non-blocking).");

    m_state->cancel = true;

    if (m_thread) {

        disconnect(m_thread, &QThread::finished, this, nullptr);
        m_thread = nullptr;
    }

}

bool DiskScanner::isRunning() const
{
    return m_thread && m_thread->isRunning();
}

void DiskScanner::cancel()
{

    m_state->cancel = true;

    if (!m_thread) return;

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
                                   "Scan cancelled by caller – waiting up to 500 ms.");

    m_thread->wait(500);
    if (m_thread && m_thread->isRunning()) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
                                       "Thread did not stop within 500 ms – terminating.");
        m_thread->terminate();
        m_thread->wait(300);
    }

    m_thread = nullptr;
}

void DiskScanner::startScan(const QString& rootPath)
{

    cancel();

    m_state = std::make_shared<ScanState>();
    m_pendingCacheRoot.reset();

    if (m_cacheEnabled) {
        auto it = m_cache.constFind(rootPath);
        if (it != m_cache.constEnd()) {
            const qint64 ageSecs =
                it->timestamp.secsTo(QDateTime::currentDateTime());
            if (ageSecs <= m_cacheTtlSecs) {
                wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                    QStringLiteral("Cache hit for '%1' (age %2 s, TTL %3 s).")
                        .arg(rootPath).arg(ageSecs).arg(m_cacheTtlSecs));
                emit scanStarted(rootPath);
                emit scanFinished(it->root);
                emit scanStats(it->stats);
                return;
            }
            wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                QStringLiteral("Cache expired for '%1' (age %2 s). Re-scanning.")
                    .arg(rootPath).arg(ageSecs));
            m_cache.remove(rootPath);
        }
    }

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("Starting background scan of '%1'.").arg(rootPath));

    emit scanStarted(rootPath);

    auto              state    = m_state;
    QPointer<DiskScanner> safeThis(this);

    QThread* t = QThread::create([state, safeThis, rootPath]() {
        threadMain(state, safeThis, rootPath);
    });
    m_thread = t;

    connect(t, &QThread::finished, t, &QObject::deleteLater);

    connect(t, &QThread::finished, this, [this, t, rootPath, state]() {
        if (m_cacheEnabled && m_pendingCacheRoot && !state->cancel.load()) {
            m_cache.insert(rootPath, { m_pendingCacheRoot,
                                       QDateTime::currentDateTime(),
                                       m_pendingStats });
            wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
                QStringLiteral("Scan result cached for '%1'.").arg(rootPath));
        }
        m_pendingCacheRoot.reset();
        if (m_thread == t) m_thread = nullptr;

    });

    t->start();
}

void DiskScanner::threadMain(std::shared_ptr<ScanState> state,
                              QPointer<DiskScanner>      scanner,
                              QString                    rootPath)
{
    QElapsedTimer timer;
    timer.start();

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("[thread] Started for '%1'.").arg(rootPath));

    auto root = std::make_shared<DiskNode>();
    const QString displayName = QFileInfo(rootPath).fileName();
    root->name  = displayName.isEmpty() ? rootPath : displayName;
    root->path  = rootPath;
    root->isDir = true;

    scanDir(root.get(), state, scanner);

    if (state->cancel.load()) {
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
            QStringLiteral("[thread] Scan of '%1' cancelled after %2 ms "
                           "(%3 files, %4 dirs).")
                .arg(rootPath)
                .arg(timer.elapsed())
                .arg(state->files.load())
                .arg(state->dirs.load()));

        if (scanner) emit scanner->scanCancelled();
        return;
    }

    const qint64 elapsed = timer.elapsed();
    ScanStats stats;
    stats.files     = state->files.load();
    stats.dirs      = state->dirs.load();
    stats.bytes     = state->bytes.load();
    stats.elapsedMs = elapsed;

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("[thread] Scan of '%1' complete: %2 files, %3 dirs, "
                       "%4 total, %5 ms.")
            .arg(rootPath)
            .arg(stats.files)
            .arg(stats.dirs)
            .arg(DiskNode::prettySize(stats.bytes))
            .arg(elapsed));

    if (scanner) {
        scanner->m_pendingStats      = stats;
        scanner->m_pendingCacheRoot  = root;
    }

    if (scanner) emit scanner->scanFinished(root);
    if (scanner) emit scanner->scanStats(stats);
}

void DiskScanner::scanDir(DiskNode*                   scanRoot,
                           std::shared_ptr<ScanState>  state,
                           QPointer<DiskScanner>       scanner)
{

    struct WorkItem { DiskNode* node; };
    std::vector<WorkItem>  stack;
    std::vector<DiskNode*> allNodes;

    stack.reserve(1024);
    allNodes.reserve(65536);
    stack.push_back({ scanRoot });

    int accessErrors = 0;
    int localFiles   = 0;
    qint64 localBytes = 0;

    while (!stack.empty() && !state->cancel.load()) {
        DiskNode* current = stack.back().node;
        stack.pop_back();

        allNodes.push_back(current);
        if (current->isDir) ++state->dirs;

        QDir dir(current->path);
        dir.setFilter(QDir::AllEntries | QDir::NoDotAndDotDot |
                      QDir::Hidden    | QDir::System);

        const QFileInfoList entries = dir.entryInfoList();

        if (entries.isEmpty() && current != scanRoot) {
            const QDir test(current->path);
            if (!test.isReadable()) {
                current->scanError = true;
                ++accessErrors;
                wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
                    QStringLiteral("[thread] Cannot read directory: '%1'")
                        .arg(current->path));
            }
        }

        current->itemCount = static_cast<int>(entries.size());
        current->children.reserve(current->itemCount);

        for (const QFileInfo& info : entries) {
            if (state->cancel.load()) break;

            auto child    = std::make_shared<DiskNode>();
            child->name   = info.fileName();
            child->path   = info.absoluteFilePath();
            child->parent = current;

            if (info.isSymLink()) {
                child->isDir = info.isDir();
                child->size  = info.isDir() ? 0 : info.size();
                if (!child->isDir) {
                    ++localFiles;
                    localBytes += child->size;
                }
            } else if (info.isDir()) {
                child->isDir = true;
                child->size  = 0;
                stack.push_back({ child.get() });
            } else {
                child->isDir  = false;
                child->size   = info.size();
                ++localFiles;
                localBytes   += info.size();
            }

            current->children.push_back(std::move(child));

            if (localFiles > 0 && localFiles % 500 == 0) {
                state->files += localFiles;
                state->bytes += localBytes;
                localFiles    = 0;
                localBytes    = 0;
                if (scanner)
                    emit scanner->progressUpdate(
                        state->files.load(), state->bytes.load(), current->path);
            }
        }
    }

    state->files += localFiles;
    state->bytes += localBytes;

    if (accessErrors > 0)
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
            QStringLiteral("[thread] %1 unreadable director%2 encountered.")
                .arg(accessErrors)
                .arg(accessErrors == 1 ? "y" : "ies"));

    if (state->cancel.load()) return;

    wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
        QStringLiteral("[thread] Phase 2: accumulating sizes for %1 nodes.")
            .arg(static_cast<int>(allNodes.size())));

    for (int i = static_cast<int>(allNodes.size()) - 1; i >= 0; --i) {
        DiskNode* n = allNodes[i];
        if (!n->isDir) continue;
        qint64 total = 0;
        for (const auto& child : n->children)
            total += child->size;
        n->size = total;
    }
}

}
