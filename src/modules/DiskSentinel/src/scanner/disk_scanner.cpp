#include "modules/disksentinel/src/scanner/disk_scanner.hpp"

#include "logger/logger.hpp"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QPointer>
#include <QThread>
#include <QThreadPool>
#include <QMutex>
#include <QFuture>
#include <QtConcurrent/QtConcurrentRun>

#include <vector>
#include <algorithm>

namespace wintools::disksentinel {

static constexpr const char* kLog = "DiskSentinel/Scanner";

static void sortTreeForDisplay(DiskNode* root) {
    if (!root) return;

    auto cmp = [](const std::shared_ptr<DiskNode>& a,
                  const std::shared_ptr<DiskNode>& b) -> bool {
        if (!a || !b) return !a && b;
        if (a->isDir != b->isDir) return a->isDir;
        if (a->size != b->size) return a->size > b->size;
        return a->name.toLower() < b->name.toLower();
    };

    std::vector<DiskNode*> stack;
    stack.push_back(root);
    while (!stack.empty()) {
        DiskNode* current = stack.back();
        stack.pop_back();
        if (!current || current->children.empty()) continue;

        std::sort(current->children.begin(), current->children.end(), cmp);
        for (const auto& child : current->children) {
            if (child && child->isDir) {
                stack.push_back(child.get());
            }
        }
    }
}

static void accumulateSizes(DiskNode* root) {
    if (!root) return;
    std::vector<DiskNode*> allNodes;
    allNodes.reserve(4096);

    std::vector<DiskNode*> stack;
    stack.push_back(root);
    while (!stack.empty()) {
        DiskNode* n = stack.back();
        stack.pop_back();
        allNodes.push_back(n);
        for (const auto& child : n->children) {
            if (child && child->isDir)
                stack.push_back(child.get());
        }
    }

    for (int i = static_cast<int>(allNodes.size()) - 1; i >= 0; --i) {
        DiskNode* n = allNodes[i];
        if (!n->isDir) continue;
        qint64 total = 0;
        for (const auto& child : n->children)
            total += child->size;
        n->size = total;
    }
}

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

    t->start(QThread::LowestPriority);
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

    QDir dir(rootPath);
    if (!dir.exists() || !dir.isReadable()) {
        root->scanError = true;
        if (scanner) emit scanner->scanFinished(root);
        return;
    }

    QVector<DiskNode*> topDirs;

    QDirIterator it(rootPath,
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
        QDirIterator::NoIteratorFlags);

    while (it.hasNext() && !state->cancel.load()) {
        it.next();
        const QFileInfo info = it.fileInfo();

        auto child    = std::make_shared<DiskNode>();
        child->name   = info.fileName();
        child->path   = info.absoluteFilePath();
        child->parent = root.get();

        if (info.isSymLink()) {
            child->isDir = info.isDir();
            child->size  = info.isDir() ? 0 : info.size();
            if (!child->isDir) {
                state->files++;
                state->bytes += child->size;
            }
        } else if (info.isDir()) {
            child->isDir = true;
            child->size  = 0;
            topDirs.push_back(child.get());
        } else {
            child->isDir  = false;
            child->size   = info.size();
            state->files++;
            state->bytes += child->size;
        }

        root->children.push_back(std::move(child));
    }
    root->itemCount = root->children.size();
    ++state->dirs;

    if (state->cancel.load()) {
        if (scanner) emit scanner->scanCancelled();
        return;
    }

    if (!topDirs.isEmpty()) {
        const int threadCount = qBound(2, QThread::idealThreadCount(), 8);
        QThreadPool pool;
        pool.setMaxThreadCount(threadCount);

        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Pass,
            QStringLiteral("[thread] Parallel scan: %1 top-level dirs, %2 workers.")
                .arg(topDirs.size()).arg(threadCount));

        QMutex progressMutex;
        QElapsedTimer progressTimer;
        progressTimer.start();
        qint64 lastProgressMs = 0;

        auto scanSubtree = [&state, &scanner, &progressMutex, &progressTimer, &lastProgressMs](DiskNode* subRoot) {
            if (state->cancel.load()) return;
            scanDir(subRoot, state, scanner);

            QMutexLocker lk(&progressMutex);
            const qint64 nowMs = progressTimer.elapsed();
            if ((nowMs - lastProgressMs) >= 200) {
                lastProgressMs = nowMs;
                if (scanner)
                    emit scanner->progressUpdate(
                        state->files.load(), state->bytes.load(), subRoot->path);
            }
        };

        QVector<QFuture<void>> futures;
        futures.reserve(topDirs.size());
        for (DiskNode* subDir : topDirs) {
            futures.push_back(QtConcurrent::run(&pool, scanSubtree, subDir));
        }

        for (auto& f : futures)
            f.waitForFinished();
    }

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

    accumulateSizes(root.get());
    sortTreeForDisplay(root.get());

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
    QElapsedTimer progressTimer;
    progressTimer.start();
    qint64 lastProgressMs = 0;

    while (!stack.empty() && !state->cancel.load()) {
        DiskNode* current = stack.back().node;
        stack.pop_back();

        allNodes.push_back(current);
        if (current->isDir) ++state->dirs;

        QDir dir(current->path);
        if (!dir.exists() || !dir.isReadable()) {
            current->scanError = true;
            ++accessErrors;
            continue;
        }

        int itemCount = 0;
        QDirIterator it(
            current->path,
            QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
            QDirIterator::NoIteratorFlags);

        while (it.hasNext()) {
            if (state->cancel.load()) break;

            it.next();
            const QFileInfo info = it.fileInfo();
            ++itemCount;

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

            const qint64 nowMs = progressTimer.elapsed();
            if (localFiles > 0 && (nowMs - lastProgressMs) >= 150) {
                state->files += localFiles;
                state->bytes += localBytes;
                localFiles    = 0;
                localBytes    = 0;
                lastProgressMs = nowMs;
                if (scanner)
                    emit scanner->progressUpdate(
                        state->files.load(), state->bytes.load(), current->path);
            }
        }

        current->itemCount = itemCount;
        current->children.squeeze();
    }

    state->files += localFiles;
    state->bytes += localBytes;

    if (accessErrors > 0)
        wintools::logger::Logger::log(kLog, wintools::logger::Severity::Warning,
            QStringLiteral("[thread] %1 unreadable director%2 encountered.")
                .arg(accessErrors)
                .arg(accessErrors == 1 ? "y" : "ies"));

    if (state->cancel.load()) return;

    accumulateSizes(scanRoot);
}

}
