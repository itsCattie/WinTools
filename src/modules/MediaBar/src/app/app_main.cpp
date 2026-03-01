#include "app_main.hpp"

#include "config.hpp"
#include "debug_logger.hpp"
#include "lyrics_timing.hpp"
#include "common/themes/theme_helper.hpp"
#include "common/themes/color_utils.hpp"

#include <QApplication>
#include <QComboBox>
#include <QAction>
#include <QDateTime>
#include <QCheckBox>
#include <QColor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFrame>
#include <QHash>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QMessageBox>
#include <QIcon>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QPixmap>
#include <QPainter>
#include <QScreen>
#include <QDir>
#include <QDirIterator>
#include <QPushButton>
#include <QThreadPool>
#include <QTimer>
#include <QTextEdit>
#include <QStyle>
#include <QSet>
#include <QMap>
#include <QVBoxLayout>

#include <QProcess>
#include <QStandardPaths>
#include <QDesktopServices>
#include <functional>
#include <cmath>

namespace {

using wintools::themes::blendColor;
using wintools::themes::compositeOver;
using wintools::themes::contrastRatio;
using wintools::themes::bestTextColorFor;
using wintools::themes::tintedIcon;

QString sanitizeOverlayTitle(QString title) {
    title = title.trimmed();
    while (!title.isEmpty() && !title.at(0).isLetterOrNumber()) {
        title.remove(0, 1);
    }
    return title.trimmed();
}

QString overlayTitleIconPathFor(const QString& titleText) {
    const QString key = titleText.toLower();
    if (key.contains("spotify library") || key.contains("local library")) {
        return ":/icons/mediabar/library.svg";
    }
    if (key.contains("search")) {
        return ":/icons/mediabar/search.svg";
    }
    if (key.contains("devices")) {
        return ":/icons/mediabar/devices.svg";
    }
    if (key.contains("queue")) {
        return ":/icons/mediabar/queue.svg";
    }
    if (key.contains("mini player style") || key.contains("appearance")) {
        return ":/icons/mediabar/appearance.svg";
    }
    if (key.contains("library path")) {
        return ":/icons/mediabar/library_path.svg";
    }
    if (key.contains("debug")) {
        return ":/icons/mediabar/debug.svg";
    }
    return ":/icons/modules/mediabar.svg";
}

QString buildMiniPopupMenuQss(const wintools::themes::ThemePalette& palette) {

    const QColor menuBg  = palette.cardBackground;
    const QColor hoverBg = compositeOver(palette.cardBackground, palette.hoverBackground);

    return QStringLiteral(
        "QMenu { background-color: %1; color: %2; border: 1px solid %3; border-radius: 8px; padding: 4px; }"
        "QMenu::item { padding: 6px 24px 6px 12px; border-radius: 4px; }"
        "QMenu::item:selected { background-color: %4; }"
        "QMenu::item:disabled { color: %5; }"
        "QMenu::separator { height: 1px; background: %3; margin: 4px 8px; }")
        .arg(menuBg.name(),
            palette.foreground.name(),
            palette.cardBorder.name(),
            hoverBg.name(),
            palette.mutedForeground.name());
}

struct BoolResetGuard {
    bool& flag;
    explicit BoolResetGuard(bool& target) : flag(target) {
        flag = true;
    }
    ~BoolResetGuard() {
        flag = false;
    }
};

void ensureSpotifyRunning() {
    QProcess check;
    check.setProgram("tasklist");
    check.setArguments({"/FI", "IMAGENAME eq Spotify.exe", "/NH"});
    check.start();
    check.waitForFinished(3000);
    const QString output = QString::fromLocal8Bit(check.readAllStandardOutput());
    if (output.contains("Spotify.exe", Qt::CaseInsensitive)) {
        debuglog::info("SpotifyLaunch", "Spotify is already running.");
        return;
    }

    const QStringList candidates = {
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + "/../Spotify/Spotify.exe",
        QDir::homePath() + "/AppData/Roaming/Spotify/Spotify.exe",
        "C:/Program Files/Spotify/Spotify.exe",
        "C:/Program Files (x86)/Spotify/Spotify.exe",
        QStandardPaths::findExecutable("Spotify"),
    };

    for (const QString& path : candidates) {
        if (!path.isEmpty() && QFile::exists(path)) {
            if (QProcess::startDetached(path, {"--minimized"})) {
                debuglog::info("SpotifyLaunch",
                    QString("Launched Spotify from: %1").arg(path));
                return;
            }
        }
    }

    QDesktopServices::openUrl(QUrl("spotify:"));
    debuglog::info("SpotifyLaunch", "Attempted spotify: URI launch (fallback).");
}

QPoint menuPointAboveWidget(QWidget* owner, QWidget* anchor, const QSize& menuSize, int extraLift) {
    if (!anchor) {
        return QPoint();
    }

    QPoint global = anchor->mapToGlobal(QPoint(0, 0));
    int x = global.x();
    int y = global.y() - menuSize.height() - std::max(8, extraLift);

    QScreen* screen = owner ? owner->screen() : QGuiApplication::primaryScreen();
    const QRect bounds = screen ? screen->geometry() : QRect(0, 0, 1920, 1080);

    if (x + menuSize.width() > bounds.right()) {
        x = bounds.right() - menuSize.width() - 2;
    }
    if (x < bounds.left()) {
        x = bounds.left() + 2;
    }
    if (y < bounds.top()) {
        y = bounds.top() + 2;
    }

    return QPoint(x, y);
}

QVBoxLayout* createOverlayPanel(QDialog& dialog, QWidget* parent, const QString& title, int panelWidth, int panelMinHeight = 220) {
    const auto palette = wintools::themes::ThemeHelper::currentPalette();
    const bool isDark = palette.windowBackground.lightness() < 128;
    const QColor primaryTextColor = bestTextColorFor(palette.accent, palette.foreground, palette);
    const QColor listHoverBg = blendColor(palette.cardBackground, palette.hoverBackground, isDark ? 0.30f : 0.18f);
    const QColor listSelectedBg = blendColor(palette.cardBackground, palette.accent, isDark ? 0.28f : 0.16f);
    const QColor listHoverText = bestTextColorFor(listHoverBg, palette.foreground, palette);
    const QColor listSelectedText = bestTextColorFor(listSelectedBg, palette.foreground, palette);
    QColor scrim = palette.shadow;
    scrim.setAlpha(160);

    dialog.setParent(parent);
    dialog.setModal(true);
    dialog.setWindowModality(Qt::WindowModal);
    dialog.setWindowFlag(Qt::Dialog, true);
    dialog.setWindowFlag(Qt::FramelessWindowHint, true);
    dialog.setAttribute(Qt::WA_TranslucentBackground, true);
    dialog.setStyleSheet(QString(
        "QDialog { background-color: %10; }"
        "QFrame#overlayPanel { background-color: %1; border: 1px solid %2; border-radius: 16px; }"
        "QLabel#overlayTitle { color: %3; font-size: 15pt; font-weight: 700; padding-bottom: 4px; background: transparent; }"
        "QLabel#overlayBody { color: %4; font-size: 10pt; line-height: 1.4; background: transparent; }"
        "QLabel#overlaySection { color: %4; font-size: 8pt; font-weight: 600; letter-spacing: 1px; padding-top: 8px; background: transparent; }"
        "QPushButton { background-color: %7; color: %3; border: none; border-radius: 10px; padding: 9px 16px; font-size: 10pt; }"
        "QPushButton:hover { background-color: %5; }"
        "QPushButton#primary { background-color: %6; color: %9; border: none; border-radius: 10px; font-weight: 700; }"
        "QPushButton#primary:hover { background-color: %8; }"
        "QListWidget { background-color: %7; color: %3; border: none; border-radius: 10px; padding: 4px; outline: none; }"
        "QListWidget::item { padding: 10px 12px; border-radius: 8px; }"
        "QListWidget::item:hover { background-color: %11; color: %12; }"
        "QListWidget::item:selected { background-color: %13; color: %14; border: none; outline: none; }"
        "QLineEdit, QTextEdit, QDoubleSpinBox { background-color: %7; color: %3; border: 1px solid %2; border-radius: 10px; padding: 9px 12px; font-size: 10pt; }"
        "QLineEdit:focus, QTextEdit:focus { border-color: %6; }"
        "QCheckBox { color: %3; font-size: 10pt; spacing: 8px; }"
        "QCheckBox::indicator { width: 18px; height: 18px; border: 2px solid %2; border-radius: 4px; background-color: %7; }"
        "QCheckBox::indicator:checked { background-color: %6; border-color: %6; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 4px 1px; border-radius: 3px; }"
        "QScrollBar::handle:vertical { background: %2; min-height: 28px; border-radius: 3px; }"
        "QScrollBar::handle:vertical:hover { background: %4; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }")
           .arg(palette.cardBackground.name(),
               palette.cardBorder.name(),
               palette.foreground.name(),
               palette.mutedForeground.name(),
               palette.hoverBackground.name(),
               palette.accent.name(),
               palette.windowBackground.name(),
               palette.accent.lighter(115).name(),
               primaryTextColor.name(),
               scrim.name(QColor::HexArgb),
               listHoverBg.name(),
               listHoverText.name(),
               listSelectedBg.name(),
               listSelectedText.name()));

    const QSize fallbackSize(panelWidth + 80, panelMinHeight + 100);
    dialog.resize(parent ? parent->size() : fallbackSize);

    auto* root = new QVBoxLayout(&dialog);
    root->setContentsMargins(24, 24, 24, 24);
    root->addStretch(1);

    auto* row = new QHBoxLayout();
    row->addStretch(1);

    auto* panel = new QFrame(&dialog);
    panel->setObjectName("overlayPanel");
    panel->setMinimumWidth(panelWidth);
    panel->setMinimumHeight(panelMinHeight);
    panel->setMaximumWidth(panelWidth);

    auto* panelLayout = new QVBoxLayout(panel);
    panelLayout->setContentsMargins(20, 20, 20, 20);
    panelLayout->setSpacing(12);

    const QString cleanTitle = sanitizeOverlayTitle(title);
    auto* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 0, 0);
    titleRow->setSpacing(10);

    auto* titleIcon = new QLabel(panel);
    const QString iconPath = overlayTitleIconPathFor(cleanTitle);
    const QIcon icon(iconPath);
    titleIcon->setFixedSize(26, 26);
    if (!icon.isNull()) {
        titleIcon->setPixmap(tintedIcon(icon, QSize(24, 24), palette.foreground).pixmap(24, 24));
    }

    auto* titleLabel = new QLabel(cleanTitle, panel);
    titleLabel->setObjectName("overlayTitle");
    titleRow->addWidget(titleIcon);
    titleRow->addWidget(titleLabel, 1);
    panelLayout->addLayout(titleRow);

    row->addWidget(panel);
    row->addStretch(1);

    root->addLayout(row);
    root->addStretch(1);
    return panelLayout;
}

void showOverlayMessage(QWidget* parent, const QString& title, const QString& message) {
    QDialog dialog(parent);
    auto* layout = createOverlayPanel(dialog, parent, title, 560, 220);

    auto* body = new QLabel(message, &dialog);
    body->setObjectName("overlayBody");
    body->setWordWrap(true);
    layout->addWidget(body);
    layout->addStretch(1);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    auto* closeButton = new QPushButton("Close", &dialog);
    closeButton->setObjectName("primary");
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttons->addWidget(closeButton);
    layout->addLayout(buttons);

    dialog.exec();
}

std::optional<QString> showOverlayTextInput(QWidget* parent, const QString& title, const QString& prompt, const QString& primaryText, const QString& initial = QString()) {
    QDialog dialog(parent);
    auto* layout = createOverlayPanel(dialog, parent, title, 620, 230);

    auto* promptLabel = new QLabel(prompt, &dialog);
    promptLabel->setObjectName("overlayBody");
    promptLabel->setWordWrap(true);
    layout->addWidget(promptLabel);

    auto* input = new QLineEdit(&dialog);
    input->setText(initial);
    layout->addWidget(input);
    input->setFocus();

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    auto* cancel = new QPushButton("Cancel", &dialog);
    auto* accept = new QPushButton(primaryText, &dialog);
    accept->setObjectName("primary");
    accept->setEnabled(!input->text().trimmed().isEmpty());
    QObject::connect(input, &QLineEdit::textChanged, &dialog, [accept](const QString& text) {
        accept->setEnabled(!text.trimmed().isEmpty());
    });
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(accept, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(input, &QLineEdit::returnPressed, &dialog, [accept]() {
        if (accept->isEnabled()) {
            accept->click();
        }
    });
    buttons->addWidget(cancel);
    buttons->addWidget(accept);
    layout->addLayout(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }

    const QString value = input->text().trimmed();
    if (value.isEmpty()) {
        return std::nullopt;
    }
    return value;
}

int showOverlaySelectionDialog(QWidget* parent,
                               const QString& title,
                               const QString& prompt,
                               const QStringList& items,
                               const QString& primaryText) {
    if (items.isEmpty()) {
        return -1;
    }

    QDialog dialog(parent);
    auto* layout = createOverlayPanel(dialog, parent, title, 720, 420);

    auto* promptLabel = new QLabel(prompt, &dialog);
    promptLabel->setObjectName("overlayBody");
    promptLabel->setWordWrap(true);
    layout->addWidget(promptLabel);

    auto* list = new QListWidget(&dialog);
    list->addItems(items);
    list->setFocusPolicy(Qt::NoFocus);
    list->setCurrentRow(0);
    layout->addWidget(list, 1);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    auto* cancel = new QPushButton("Close", &dialog);
    auto* accept = new QPushButton(primaryText, &dialog);
    accept->setObjectName("primary");
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(accept, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(list, &QListWidget::itemDoubleClicked, &dialog, [&dialog](QListWidgetItem*) {
        dialog.accept();
    });
    buttons->addWidget(cancel);
    buttons->addWidget(accept);
    layout->addLayout(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return -1;
    }
    return list->currentRow();
}

int showOverlaySelectionDialogLazy(QWidget* parent,
                                   const QString& title,
                                   const QString& loadingText,
                                   const QString& prompt,
                                   const QString& primaryText,
                                   const std::function<QStringList()>& loader) {
    QDialog dialog(parent);
    auto* layout = createOverlayPanel(dialog, parent, title, 720, 420);

    auto* promptLabel = new QLabel(loadingText, &dialog);
    promptLabel->setObjectName("overlayBody");
    promptLabel->setWordWrap(true);
    layout->addWidget(promptLabel);

    auto* list = new QListWidget(&dialog);
    list->addItem("Loading...");
    list->setFocusPolicy(Qt::NoFocus);
    list->setEnabled(false);
    layout->addWidget(list, 1);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);
    auto* cancel = new QPushButton("Close", &dialog);
    auto* accept = new QPushButton(primaryText, &dialog);
    accept->setObjectName("primary");
    accept->setEnabled(false);
    QObject::connect(cancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(accept, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(list, &QListWidget::itemDoubleClicked, &dialog, [&dialog](QListWidgetItem*) {
        dialog.accept();
    });
    buttons->addWidget(cancel);
    buttons->addWidget(accept);
    layout->addLayout(buttons);

    QTimer::singleShot(0, &dialog, [&dialog, loader, list, promptLabel, accept, prompt]() {
        const QStringList items = loader();
        list->clear();
        if (items.isEmpty()) {
            promptLabel->setText("No items available.");
            list->addItem("(No items available)");
            list->setEnabled(false);
            accept->setEnabled(false);
            return;
        }

        promptLabel->setText(prompt);
        list->addItems(items);
        list->setEnabled(true);
        list->setCurrentRow(0);
        accept->setEnabled(true);
    });

    if (dialog.exec() != QDialog::Accepted) {
        return -1;
    }
    return list->currentRow();
}

enum class CatalogActionChoice {
    PlayNow,
    Queue,
    Browse
};

struct CatalogDisplayItem {
    QString title;
    QString subtitle;
    QString imageUrl;
    QString badge;
};

struct CatalogPickResult {
    int index = -1;
    CatalogActionChoice action = CatalogActionChoice::PlayNow;
};

QIcon fallbackCircleIcon(const QString& glyph) {
    const auto palette = wintools::themes::ThemeHelper::currentPalette();
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(palette.foreground);
    QFont font = painter.font();
    font.setPointSize(14);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, glyph);
    return QIcon(pixmap);
}

QIcon iconForImageUrl(QNetworkAccessManager& network, const QString& imageUrl, const QString& fallbackGlyph) {

    static QHash<QString, QIcon> iconCache;
    static constexpr int kMaxCacheEntries = 200;

    const QString normalized = imageUrl.trimmed();
    if (normalized.isEmpty()) {
        return fallbackCircleIcon(fallbackGlyph);
    }

    if (iconCache.contains(normalized)) {
        return iconCache.value(normalized);
    }

    QNetworkReply* reply = network.get(QNetworkRequest(QUrl(normalized)));
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    const QByteArray bytes = reply->readAll();
    const auto err = reply->error();
    reply->deleteLater();
    if (err != QNetworkReply::NoError) {
        return fallbackCircleIcon(fallbackGlyph);
    }

    QPixmap pix;
    if (!pix.loadFromData(bytes)) {
        return fallbackCircleIcon(fallbackGlyph);
    }

    QIcon icon(pix.scaled(40, 40, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));

    if (iconCache.size() >= kMaxCacheEntries) {
        iconCache.clear();
    }
    iconCache.insert(normalized, icon);

    return icon;
}

std::optional<CatalogPickResult> showOverlayCatalogPicker(
    QWidget* parent,
    const QString& title,
    const QString& loadingText,
    const QString& prompt,
    const std::function<QVector<CatalogDisplayItem>(const std::function<void(const QString&)>& reportProgress)>& loader,
    const QString& playNowLabel,
    const QString& queueLabel,
    const QString& browseLabel = QString()) {
    QDialog dialog(parent);
    auto* layout = createOverlayPanel(dialog, parent, title, 760, 520);

    auto* promptLabel = new QLabel(loadingText, &dialog);
    promptLabel->setObjectName("overlayBody");
    promptLabel->setWordWrap(true);
    layout->addWidget(promptLabel);

    auto* filterInput = new QLineEdit(&dialog);
    filterInput->setPlaceholderText("Filter results...");
    filterInput->setEnabled(false);
    layout->addWidget(filterInput);

    auto* list = new QListWidget(&dialog);
    list->setIconSize(QSize(40, 40));
    list->setFocusPolicy(Qt::NoFocus);
    list->setEnabled(false);
    layout->addWidget(list, 1);

    auto* actions = new QHBoxLayout();
    actions->addStretch(1);
    auto* closeButton = new QPushButton("Close", &dialog);
    auto* queueButton = new QPushButton(queueLabel, &dialog);
    QPushButton* browseButton = nullptr;
    if (!browseLabel.isEmpty()) {
        browseButton = new QPushButton(browseLabel, &dialog);
        browseButton->setEnabled(false);
    }
    auto* playButton = new QPushButton(playNowLabel, &dialog);
    playButton->setObjectName("primary");
    queueButton->setEnabled(false);
    playButton->setEnabled(false);
    actions->addWidget(closeButton);
    if (browseButton) actions->addWidget(browseButton);
    actions->addWidget(queueButton);
    actions->addWidget(playButton);
    layout->addLayout(actions);

    std::optional<CatalogPickResult> result;
    QVector<CatalogDisplayItem> allItems;
    QNetworkAccessManager imageNetwork;

    const auto rebuildList = [&]() {
        const QString filter = filterInput->text().trimmed();
        const QString filterLower = filter.toLower();
        list->clear();

        for (int i = 0; i < allItems.size(); ++i) {
            const auto& entry = allItems[i];
            const QString haystack = (entry.title + " " + entry.subtitle + " " + entry.badge).toLower();
            if (!filterLower.isEmpty() && !haystack.contains(filterLower)) {
                continue;
            }

            QString line = entry.title;
            if (!entry.badge.trimmed().isEmpty()) {
                line += QString("\n%1").arg(entry.badge.trimmed());
            }
            if (!entry.subtitle.trimmed().isEmpty()) {
                line += QString("\n%1").arg(entry.subtitle.trimmed());
            }

            auto* item = new QListWidgetItem(line);
            item->setData(Qt::UserRole, i);
            const QString fallbackGlyph = entry.badge.left(1).toUpper();
            item->setIcon(iconForImageUrl(imageNetwork, entry.imageUrl, fallbackGlyph));
            list->addItem(item);
        }

        const bool hasItems = list->count() > 0;
        queueButton->setEnabled(hasItems);
        playButton->setEnabled(hasItems);
        if (browseButton) browseButton->setEnabled(hasItems);
        if (hasItems) {
            list->setCurrentRow(0);
        }
    };

    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(filterInput, &QLineEdit::textChanged, &dialog, [rebuildList](const QString&) {
        rebuildList();
    });

    QObject::connect(playButton, &QPushButton::clicked, &dialog, [&]() {
        auto* item = list->currentItem();
        if (!item) {
            return;
        }
        result = CatalogPickResult{item->data(Qt::UserRole).toInt(), CatalogActionChoice::PlayNow};
        dialog.accept();
    });
    QObject::connect(queueButton, &QPushButton::clicked, &dialog, [&]() {
        auto* item = list->currentItem();
        if (!item) {
            return;
        }
        result = CatalogPickResult{item->data(Qt::UserRole).toInt(), CatalogActionChoice::Queue};
        dialog.accept();
    });
    if (browseButton) {
        QObject::connect(browseButton, &QPushButton::clicked, &dialog, [&]() {
            auto* item = list->currentItem();
            if (!item) {
                return;
            }
            result = CatalogPickResult{item->data(Qt::UserRole).toInt(), CatalogActionChoice::Browse};
            dialog.accept();
        });
    }
    QObject::connect(list, &QListWidget::itemDoubleClicked, &dialog, [&](QListWidgetItem* item) {
        if (!item) {
            return;
        }
        result = CatalogPickResult{item->data(Qt::UserRole).toInt(), CatalogActionChoice::PlayNow};
        dialog.accept();
    });

    QTimer::singleShot(0, &dialog, [&]() {
        const auto reportProgress = [&](const QString& status) {
            promptLabel->setText(status);
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        };

        allItems = loader(reportProgress);
        if (allItems.isEmpty()) {
            promptLabel->setText("No items available.");
            list->addItem("(No items available)");
            list->setEnabled(false);
            filterInput->setEnabled(false);
            queueButton->setEnabled(false);
            playButton->setEnabled(false);
            return;
        }

        promptLabel->setText(prompt);
        list->setEnabled(true);
        filterInput->setEnabled(true);
        rebuildList();
    });

    if (dialog.exec() != QDialog::Accepted) {
        return std::nullopt;
    }
    return result;
}

std::optional<QString> showQueueTargetPicker(QWidget* parent) {
    const QStringList options{
        "Spotify Queue",
        "Sonos Queue"
    };

    const int index = showOverlaySelectionDialog(parent,
        "Queue Target",
        "Choose where to queue this item:",
        options,
        "Select");
    if (index < 0 || index >= options.size()) {
        return std::nullopt;
    }
    return index == 0 ? QString("spotify") : QString("sonos");
}

}

LyricsApp::LyricsApp() = default;

void LyricsApp::initInProcess() {
    debuglog::info("App", "Starting MediaBar in-process.");

    mainWindow_ = new LyricsWindow();
    mainWindow_->setWindowIcon(QIcon(":/icons/modules/mediabar.svg"));
    miniPlayerWindow_ = new MiniPlayer(nullptr);

    const QString initialSourceMode = config::settingString("playback_source", "spotify");
    client_.setSourceModeFromString(initialSourceMode);
    mainWindow_->setSourceMode(client_.sourceModeToString());

    QObject::connect(mainWindow_->sourceModeCombo(), &QComboBox::currentIndexChanged, [this]() {
        const QString mode = mainWindow_->sourceMode();
        client_.setSourceModeFromString(mode);
        auto settings = config::loadSettings();
        settings.insert("playback_source", client_.sourceModeToString());
        config::saveSettings(settings);
    });

    mainWindow_->show();
    wireMainWindowActions();

    QObject::connect(mainWindow_, &LyricsWindow::prevRequested, [this]() {
        client_.previousTrack();
    });
    QObject::connect(mainWindow_, &LyricsWindow::playPauseRequested, [this]() {
        client_.playPause();
    });
    QObject::connect(mainWindow_, &LyricsWindow::nextRequested, [this]() {
        client_.nextTrack();
    });
    QObject::connect(mainWindow_, &LyricsWindow::shuffleRequested, [this]() {
        const auto toggled = client_.toggleShuffle();
        if (toggled.has_value()) {
            currentShuffleEnabled_ = toggled;
            currentShuffleSource_ = "spotify";
            mainWindow_->setShuffleState(toggled.value(), true);
        }
    });
    QObject::connect(mainWindow_, &LyricsWindow::repeatRequested, [this]() {

        const bool target = !currentRepeatEnabled_.value_or(false);
        const auto result = client_.setRepeatState(target);
        if (result.has_value()) {
            currentRepeatEnabled_ = result.value();

            currentShuffleSource_ = "spotify";
            mainWindow_->setRepeatState(result.value(), true);
            if (miniPlayerWindow_) {
                miniPlayerWindow_->setRepeatState(result.value(), true);
            }
        } else {

            const bool newVal = target;
            currentRepeatEnabled_ = newVal;
            mainWindow_->setRepeatState(newVal, false);
            if (miniPlayerWindow_) {
                miniPlayerWindow_->setRepeatState(newVal, false);
            }
        }
    });
    QObject::connect(mainWindow_, &LyricsWindow::miniModeRequested, [this]() {
        toggleMiniMode();
    });
    QObject::connect(mainWindow_, &LyricsWindow::favoriteRequested, [this]() {
        toggleCurrentTrackLike();
    });

    QObject::connect(miniPlayerWindow_->prevButton(), &QPushButton::clicked, [this]() {
        client_.previousTrack();
    });
    QObject::connect(miniPlayerWindow_->playPauseButton(), &QPushButton::clicked, [this]() {
        client_.playPause();
    });
    QObject::connect(miniPlayerWindow_->nextButton(), &QPushButton::clicked, [this]() {
        client_.nextTrack();
    });
    QObject::connect(miniPlayerWindow_->heartButton(), &QPushButton::clicked, [this]() {
        toggleCurrentTrackLike();
    });
    QObject::connect(miniPlayerWindow_->sourceButton(), &QPushButton::clicked, [this]() {
        showMiniSourceMenu();
    });
    QObject::connect(miniPlayerWindow_->artButton(), &QPushButton::clicked, [this]() {
        showMiniTopItemsMenu();
    });

    pollingTimer_ = new QTimer(mainWindow_);
    pollingTimer_->setTimerType(Qt::PreciseTimer);
    QObject::connect(pollingTimer_, &QTimer::timeout, [this]() {
        refreshPlayback();
    });

    pollingTimer_->start(kPlaybackPollIntervalMs_);

    uiTimer_ = new QTimer(mainWindow_);
    uiTimer_->setTimerType(Qt::PreciseTimer);
    QObject::connect(uiTimer_, &QTimer::timeout, [this]() {
        updateUi();
    });

    uiTimer_->start(kUiRefreshIntervalMs_);

    QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, [this]() {
        if (miniPlayerWindow_) {
            miniPlayerWindow_->hideDesktopMini();
            miniPlayerWindow_->deleteLater();
            miniPlayerWindow_ = nullptr;
        }
        if (mainWindow_) {
            mainWindow_->deleteLater();
            mainWindow_ = nullptr;
        }
    });

    refreshPlayback();
    debuglog::info("App", "MediaBar in-process init complete.");

    if (initialSourceMode == "spotify") {
        ensureSpotifyRunning();
    }
}

void LyricsApp::showMainWindow() {
    if (!mainWindow_) return;
    exitMiniMode();
    mainWindow_->show();
    mainWindow_->raise();
    mainWindow_->activateWindow();
}

void LyricsApp::showMiniPlayer() {
    if (!miniPlayerWindow_) return;
    enterMiniMode();
}

int LyricsApp::run(int argc, char** argv) {
    debuglog::info("App", QString("Starting MediaBar argc=%1").arg(argc));
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/icons/modules/mediabar.svg"));

    mainWindow_ = new LyricsWindow();
    mainWindow_->setWindowIcon(QIcon(":/icons/modules/mediabar.svg"));
    miniPlayerWindow_ = new MiniPlayer(nullptr);
    setupTray();

    const QString initialSourceMode = config::settingString("playback_source", "spotify");
    client_.setSourceModeFromString(initialSourceMode);
    mainWindow_->setSourceMode(client_.sourceModeToString());

    QObject::connect(mainWindow_->sourceModeCombo(), &QComboBox::currentIndexChanged, [this]() {
        const QString mode = mainWindow_->sourceMode();
        client_.setSourceModeFromString(mode);

        auto settings = config::loadSettings();
        settings.insert("playback_source", client_.sourceModeToString());
        config::saveSettings(settings);
    });

    mainWindow_->show();
    wireMainWindowActions();

    QObject::connect(mainWindow_, &LyricsWindow::ipcCommand,
                     mainWindow_, [this](int cmd) {
        if      (cmd == static_cast<int>(MediaBarIpcCmd::EnterMini))  enterMiniMode();
        else if (cmd == static_cast<int>(MediaBarIpcCmd::ExitMini))   exitMiniMode();
        else if (cmd == static_cast<int>(MediaBarIpcCmd::ToggleMini)) toggleMiniMode();
    });

    {
        const QStringList args = QApplication::arguments();
        if (args.contains("--mini")) {
            QTimer::singleShot(0, mainWindow_, [this]() { enterMiniMode(); });
        } else if (args.contains("--full")) {
            QTimer::singleShot(0, mainWindow_, [this]() { exitMiniMode(); });
        }
    }

    QObject::connect(mainWindow_, &LyricsWindow::prevRequested, [this]() {
        client_.previousTrack();
    });
    QObject::connect(mainWindow_, &LyricsWindow::playPauseRequested, [this]() {
        client_.playPause();
    });
    QObject::connect(mainWindow_, &LyricsWindow::nextRequested, [this]() {
        client_.nextTrack();
    });
    QObject::connect(mainWindow_, &LyricsWindow::shuffleRequested, [this]() {
        const auto toggled = client_.toggleShuffle();
        if (toggled.has_value()) {
            currentShuffleEnabled_ = toggled;
            currentShuffleSource_ = "spotify";
            mainWindow_->setShuffleState(toggled.value(), true);
        }
    });

    QObject::connect(mainWindow_, &LyricsWindow::miniModeRequested, [this]() {
        toggleMiniMode();
    });
    QObject::connect(mainWindow_, &LyricsWindow::favoriteRequested, [this]() {
        toggleCurrentTrackLike();
    });

    QObject::connect(miniPlayerWindow_->prevButton(), &QPushButton::clicked, [this]() {
        client_.previousTrack();
    });
    QObject::connect(miniPlayerWindow_->playPauseButton(), &QPushButton::clicked, [this]() {
        client_.playPause();
    });
    QObject::connect(miniPlayerWindow_->nextButton(), &QPushButton::clicked, [this]() {
        client_.nextTrack();
    });
    QObject::connect(miniPlayerWindow_->heartButton(), &QPushButton::clicked, [this]() {
        toggleCurrentTrackLike();
    });
    QObject::connect(miniPlayerWindow_->sourceButton(), &QPushButton::clicked, [this]() {
        showMiniSourceMenu();
    });
    QObject::connect(miniPlayerWindow_->artButton(), &QPushButton::clicked, [this]() {
        showMiniTopItemsMenu();
    });

    pollingTimer_ = new QTimer(mainWindow_);
    pollingTimer_->setTimerType(Qt::PreciseTimer);
    QObject::connect(pollingTimer_, &QTimer::timeout, [this]() {
        refreshPlayback();
    });
    pollingTimer_->start(kPlaybackPollIntervalMs_);

    uiTimer_ = new QTimer(mainWindow_);
    uiTimer_->setTimerType(Qt::PreciseTimer);
    QObject::connect(uiTimer_, &QTimer::timeout, [this]() {
        updateUi();
    });
    uiTimer_->start(kUiRefreshIntervalMs_);

    QObject::connect(&app, &QCoreApplication::aboutToQuit, [this]() {
        if (trayIcon_) {
            trayIcon_->hide();
            trayIcon_->deleteLater();
            trayIcon_ = nullptr;
        }
        if (trayMenu_) {
            trayMenu_->deleteLater();
            trayMenu_ = nullptr;
        }
        if (miniPlayerWindow_) {
            miniPlayerWindow_->hideDesktopMini();
            miniPlayerWindow_->deleteLater();
            miniPlayerWindow_ = nullptr;
        }
        if (mainWindow_) {
            mainWindow_->deleteLater();
            mainWindow_ = nullptr;
        }
    });

    refreshPlayback();
    debuglog::info("App", "Entering Qt event loop.");
    return app.exec();
}

void LyricsApp::refreshPlayback() {
    if (refreshInFlight_) {
        return;
    }
    BoolResetGuard refreshGuard(refreshInFlight_);

    auto playback = client_.getCurrentPlayback();
    if (!playback.has_value()) {
        bool hadCachedPlayback = false;
        {
            QMutexLocker locker(&playbackMutex_);
            hadCachedPlayback = cachedPlayback_.has_value();
        }

        ++consecutivePlaybackMisses_;
        if (hadCachedPlayback && consecutivePlaybackMisses_ < kTransientPlaybackMissLimit_) {
            debuglog::warn("Playback", QString("Transient playback miss (%1/%2); retaining previous state.")
                .arg(consecutivePlaybackMisses_)
                .arg(kTransientPlaybackMissLimit_));
            return;
        }

        {
            QMutexLocker locker(&playbackMutex_);
            cachedPlayback_ = std::nullopt;
        }

        currentTrackId_.clear();
        QMutexLocker locker(&lyricsMutex_);
        currentLyrics_ = std::nullopt;
        return;
    }

    consecutivePlaybackMisses_ = 0;
    std::optional<PlaybackInfo> prevPlayback;
    {
        QMutexLocker locker(&playbackMutex_);
        prevPlayback = cachedPlayback_;
        cachedPlayback_ = playback;
    }
    lastPlaybackSnapshotAtMs_ = QDateTime::currentMSecsSinceEpoch();

    if (prevPlayback.has_value() && playback->trackId != prevPlayback->trackId
        && currentRepeatEnabled_.has_value() && currentRepeatEnabled_.value()) {
        const auto& prev = prevPlayback.value();
        const qint64 marginMs = 1500;
        if (prev.isPlaying && prev.durationMs > 0 && (prev.progressMs + marginMs >= prev.durationMs)) {
            debuglog::info("Playback", QString("Repeat requested: restarting previous track (prevId=%1)").arg(prev.trackId));

            if (client_.previousTrack()) {
                client_.seekToPosition(0);
                return;
            }

            client_.seekToPosition(0);
            return;
        }
    }

    if (playback->trackId.isEmpty() || playback->trackId == currentTrackId_) {
        debuglog::trace("Playback", QString("Skipping lyrics refresh trackId=%1").arg(playback->trackId));
        return;
    }

    currentTrackId_ = playback->trackId;
    debuglog::info("Playback", QString("Track changed trackId=%1 source=%2").arg(playback->trackId, playback->source));
    fetchLyricsAsync(playback.value());
}

void LyricsApp::fetchLyricsAsync(const PlaybackInfo& playback) {
    const QString track = playback.trackName;
    const QString artist = playback.artistName;
    const QString expectedTrackId = playback.trackId;

    QThreadPool::globalInstance()->start([this, track, artist, expectedTrackId]() {
        auto lyrics = lyricsFetcher_.fetchLyrics(track, artist);

        {
            QMutexLocker playbackLocker(&playbackMutex_);

            if (!cachedPlayback_.has_value() || cachedPlayback_->trackId != expectedTrackId) {
                return;
            }
        }

        QMutexLocker locker(&lyricsMutex_);
        currentLyrics_ = lyrics;
    });
}

void LyricsApp::enterMiniMode() {
        debuglog::info("MiniMode", "Entering mini mode.");
    if (miniMode_ || !mainWindow_ || !miniPlayerWindow_) {
        return;
    }

    miniMode_ = true;
    mainWindow_->hide();
    miniPlayerWindow_->showDesktopMini();
    if (trayIcon_) {
        trayIcon_->show();
    }
}

void LyricsApp::exitMiniMode() {
        debuglog::info("MiniMode", "Exiting mini mode.");
    if (!miniMode_ || !mainWindow_ || !miniPlayerWindow_) {
        return;
    }

    miniMode_ = false;
    miniPlayerWindow_->hideDesktopMini();
    mainWindow_->showNormal();
    mainWindow_->raise();
    mainWindow_->activateWindow();
    if (trayIcon_) {
        trayIcon_->hide();
    }
}

void LyricsApp::toggleMiniMode() {
    if (miniMode_) {
        exitMiniMode();
    } else {
        enterMiniMode();
    }
}

void LyricsApp::updateMiniPlayer(const PlaybackInfo& playback, const QString& sourceName) {
        debuglog::trace("MiniPlayer", QString("Update track=%1 artist=%2 source=%3 playing=%4")
            .arg(playback.trackName, playback.artistName, sourceName, playback.isPlaying ? "1" : "0"));
    if (!miniPlayerWindow_) {
        return;
    }

    latestPlayback_ = playback;
    latestSourceName_ = sourceName;

    miniPlayerWindow_->setTrackText(playback.trackName, playback.artistName);
    miniPlayerWindow_->setSourceText(sourceName);
    miniPlayerWindow_->setPlaying(playback.isPlaying);
    miniPlayerWindow_->setAlbumArtUrl(playback.albumArt);
    miniPlayerWindow_->setLikedState(currentTrackLiked_, playback.source == "spotify");

    if (trayIcon_) {
        trayIcon_->setToolTip(QString("%1\n%2\n[%3]")
                                  .arg(playback.trackName.isEmpty() ? "No track" : playback.trackName,
                                       playback.artistName.isEmpty() ? "Unknown" : playback.artistName,
                                       sourceName.isEmpty() ? "Unknown" : sourceName));
    }
}

void LyricsApp::setupTray() {
    trayIcon_ = nullptr;
    trayMenu_ = nullptr;
}

void LyricsApp::wireMainWindowActions() {
    QObject::connect(mainWindow_, &LyricsWindow::searchRequested, [this]() {
        showSearchDialog();
    });
    QObject::connect(mainWindow_, &LyricsWindow::spotifyLibraryRequested, [this]() {
        if (client_.sourceModeToString() == "sonos") {
            showSonosFavouritesDialog();
        } else {
            showSpotifyLibraryDialog();
        }
    });
    QObject::connect(mainWindow_, &LyricsWindow::spotifyDevicesRequested, [this]() {

        if (client_.sourceModeToString() == "sonos") {
            showSonosZonesDialog();
        } else {
            showSpotifyDevicesDialog();
        }
    });
    QObject::connect(mainWindow_, &LyricsWindow::localBrowseRequested, [this]() {
        showLocalLibraryDialog();
    });
    QObject::connect(mainWindow_, &LyricsWindow::queueRequested, [this]() {
        showQueueDialog();
    });
    QObject::connect(mainWindow_, &LyricsWindow::appearanceRequested, [this]() {
        showAppearanceDialog();
    });
    QObject::connect(mainWindow_, &LyricsWindow::libraryPathRequested, [this]() {
        showLibraryPathDialog();
    });
    QObject::connect(mainWindow_, &LyricsWindow::debugRequested, [this]() {
        showDebugDialog();
    });
    QObject::connect(mainWindow_, &LyricsWindow::volumeChanged, [this](int percent) {
        if (!client_.setVolumePercent(percent)) {
            debuglog::warn("Volume", QString("setVolumePercent failed value=%1 source=%2")
                .arg(percent)
                .arg(client_.getSourceName()));
        }
    });
}

void LyricsApp::showSearchDialog() {

    if (client_.sourceModeToString() == "sonos") {
        showSonosFavouritesDialog();
        return;
    }

    const auto queryOpt = showOverlayTextInput(mainWindow_, "Search", "Search for tracks, artists, or albums:", "Search");
    if (!queryOpt.has_value()) {
        return;
    }
    const QString query = queryOpt.value();

    const auto results = client_.spotify().searchCatalog(query, 30);
    if (results.isEmpty()) {
        showOverlayMessage(mainWindow_, "Search", "No results found or Spotify unavailable.");
        return;
    }

    auto pick = showOverlayCatalogPicker(
        mainWindow_,
        "Search Results",
        "Loading results...",
        "Choose an item, then Play now or Add to queue:",
        [&results](const std::function<void(const QString&)>&) {
            QVector<CatalogDisplayItem> out;
            out.reserve(results.size());
            for (const auto& result : results) {
                CatalogDisplayItem item;
                item.title = result.name;
                item.subtitle = result.subtitle;
                item.imageUrl = result.imageUrl;
                item.badge = result.type == "artist" ? "Artist" : result.type == "album" ? "Album" : "Track";
                out.push_back(item);
            }
            return out;
        },
        "Play now",
        "Add to queue");
    if (!pick.has_value() || pick->index < 0 || pick->index >= results.size()) {
        return;
    }

    const SpotifyCatalogItem selected = results[pick->index];
    if (pick->action == CatalogActionChoice::PlayNow) {
        if (!playSpotifyCatalogItemNow(selected)) {
            showOverlayMessage(mainWindow_, "Search", "Could not start playback for the selected item.");
        }
        return;
    }

    const auto target = showQueueTargetPicker(mainWindow_);
    if (!target.has_value()) {
        return;
    }
    if (!queueSpotifyCatalogItem(selected, target.value())) {
        showOverlayMessage(mainWindow_, "Search", QString("Could not add '%1' to the %2.").arg(selected.name, target.value() == "spotify" ? "Spotify queue" : "Sonos queue"));
        return;
    }
    showOverlayMessage(mainWindow_, "Search", QString("Queued '%1' in %2.").arg(selected.name, target.value() == "spotify" ? "Spotify" : "Sonos"));
}

void LyricsApp::showSpotifyLibraryDialog() {
    const QStringList sections{
        "Saved Artists",
        "Saved Albums",
        "Playlists",
        "Liked Songs"
    };

    const int sectionIndex = showOverlaySelectionDialog(
        mainWindow_,
        "Spotify Library",
        "Choose a library section:",
        sections,
        "Open");
    if (sectionIndex < 0 || sectionIndex >= sections.size()) {
        return;
    }

    QVector<SpotifyCatalogItem> items;
    QString sectionTitle = sections[sectionIndex];

    const bool browseSupported = (sectionIndex != 3);

    auto pick = showOverlayCatalogPicker(
        mainWindow_,
        QString("Spotify Library • %1").arg(sectionTitle),
        "Loading library items...",
        browseSupported
            ? "Select an item and Browse for tracks, Play now, or Add to queue:"
            : "Use filter to search, then choose Play now or Add to queue:",
        [this, sectionIndex, &items](const std::function<void(const QString&)>& reportProgress) {
            items.clear();
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            if (spotifyLibraryCache_.contains(sectionIndex)) {
                const auto& cached = spotifyLibraryCache_[sectionIndex];
                if (!cached.items.isEmpty() && (nowMs - cached.fetchedAtMs) < kSpotifyLibraryCacheTtlMs) {
                    items = cached.items;
                    reportProgress(QString("Loaded %1 cached item(s)").arg(items.size()));
                }
            }

            if (items.isEmpty()) {
                if (sectionIndex == 0) {
                    reportProgress("Loading artists...");
                    items = client_.spotify().getFollowedArtists(50);
                } else if (sectionIndex == 1) {
                    reportProgress("Loading albums...");
                    items = client_.spotify().getSavedAlbums(50);
                } else if (sectionIndex == 2) {
                    reportProgress("Loading playlists...");
                    items = client_.spotify().getUserPlaylists(50);
                } else {
                    reportProgress("Loading liked songs...");
                    const auto liked = client_.spotify().getSavedTracks(50,
                        [&reportProgress](int fetchedCount) {
                            reportProgress(QString("Loading liked songs... %1").arg(fetchedCount));
                        });
                    items.reserve(liked.size());
                    for (const auto& track : liked) {
                        SpotifyCatalogItem item;
                        item.id = track.id;
                        item.name = track.name;
                        item.subtitle = QString("%1%2%3")
                            .arg(track.artist)
                            .arg(track.artist.isEmpty() || track.album.isEmpty() ? "" : " • ")
                            .arg(track.album);
                        item.uri = track.uri;
                        item.imageUrl = track.albumArtUrl;
                        item.type = "track";
                        items.push_back(item);
                    }
                }

                if (!items.isEmpty()) {
                    spotifyLibraryCache_.insert(sectionIndex, SpotifyLibraryCacheEntry{
                        .items = items,
                        .fetchedAtMs = nowMs,
                    });
                }
            }

            QVector<CatalogDisplayItem> out;
            out.reserve(items.size());
            for (const auto& item : items) {
                CatalogDisplayItem display;
                display.title = item.name;
                display.subtitle = item.subtitle;
                display.imageUrl = item.imageUrl;
                display.badge = item.type.left(1).toUpper() + item.type.mid(1).toLower();
                out.push_back(display);
            }
            return out;
        },
        "Play now",
        "Add to queue",
        browseSupported ? "Browse" : QString());
    if (!pick.has_value() || pick->index < 0 || pick->index >= items.size()) {
        return;
    }

    const auto selected = items[pick->index];

    if (pick->action == CatalogActionChoice::Browse) {
        QVector<SpotifyTrackItem> tracks;
        const QString type = selected.type.trimmed().toLower();

        auto trackPick = showOverlayCatalogPicker(
            mainWindow_,
            QString("Spotify • %1").arg(selected.name),
            "Loading tracks...",
            "Select a track to play or queue:",
            [this, &tracks, &selected, &type](const std::function<void(const QString&)>& reportProgress) {
                reportProgress("Fetching tracks...");
                if (type == "playlist") {
                    tracks = client_.spotify().getPlaylistTracks(selected.id, 100);
                } else if (type == "album") {
                    tracks = client_.spotify().getAlbumTracks(selected.id, 50);
                } else if (type == "artist") {
                    tracks = client_.spotify().getArtistTopTracks(selected.id, 20);
                }

                QVector<CatalogDisplayItem> out;
                out.reserve(tracks.size());
                for (const auto& t : tracks) {
                    CatalogDisplayItem d;
                    d.title    = t.name;
                    d.subtitle = t.artist;
                    if (!t.album.isEmpty())
                        d.subtitle += QString(" • %1").arg(t.album);
                    d.imageUrl = t.albumArtUrl;
                    d.badge    = "Track";
                    out.push_back(d);
                }
                return out;
            },
            "Play now",
            "Add to queue");

        if (!trackPick.has_value() || trackPick->index < 0
            || trackPick->index >= tracks.size()) {
            return;
        }

        const auto& chosenTrack = tracks[trackPick->index];
        if (trackPick->action == CatalogActionChoice::PlayNow) {

            if (type == "playlist" || type == "album") {
                if (!client_.spotify().startPlaybackContext(selected.uri, chosenTrack.uri)) {
                    client_.spotify().startPlaybackUri(chosenTrack.uri);
                }
            } else {
                client_.spotify().startPlaybackUri(chosenTrack.uri);
            }
        } else {
            const auto target = showQueueTargetPicker(mainWindow_);
            if (!target.has_value()) return;
            bool ok = false;
            if (target.value() == "spotify")
                ok = client_.spotify().addToQueue(chosenTrack.uri);
            else if (target.value() == "sonos")
                ok = client_.sonos().addToQueueUri(chosenTrack.uri, true);
            if (!ok) {
                showOverlayMessage(mainWindow_, "Spotify Library",
                    QString("Could not add '%1' to the queue.").arg(chosenTrack.name));
            } else {
                showOverlayMessage(mainWindow_, "Spotify Library",
                    QString("Queued '%1'.").arg(chosenTrack.name));
            }
        }
        return;
    }

    if (pick->action == CatalogActionChoice::PlayNow) {
        if (sectionIndex == 3 && selected.type.compare("track", Qt::CaseInsensitive) == 0) {
            if (!client_.spotify().startPlaybackContext("spotify:collection", selected.uri)) {
                if (!client_.spotify().startPlaybackUri(selected.uri)) {
                    showOverlayMessage(mainWindow_, "Spotify Library", "Could not start playback for the selected item.");
                }
                return;
            }
            return;
        }

        if (!playSpotifyCatalogItemNow(selected)) {
            showOverlayMessage(mainWindow_, "Spotify Library", "Could not start playback for the selected item.");
        }
        return;
    }

    const auto target = showQueueTargetPicker(mainWindow_);
    if (!target.has_value()) {
        return;
    }
    if (!queueSpotifyCatalogItem(selected, target.value())) {
        showOverlayMessage(mainWindow_, "Spotify Library", QString("Could not add '%1' to the %2.").arg(selected.name, target.value() == "spotify" ? "Spotify queue" : "Sonos queue"));
        return;
    }
    showOverlayMessage(mainWindow_, "Spotify Library", QString("Queued '%1' in %2.").arg(selected.name, target.value() == "spotify" ? "Spotify" : "Sonos"));
}

void LyricsApp::showSpotifyDevicesDialog() {
    QVector<SpotifyDeviceItem> devices;
    const int index = showOverlaySelectionDialogLazy(
        mainWindow_,
        "Devices",
        "Loading devices...",
        "Transfer playback to a device:",
        "Transfer",
        [this, &devices]() {
            devices = client_.spotify().getDevices();
            QStringList choices;
            choices.reserve(devices.size());
            for (const auto& device : devices) {
                QString label = device.name;
                if (device.isActive) {
                    label += " (active)";
                }
                if (device.isRestricted) {
                    label += " (restricted)";
                }
                choices.push_back(label);
            }
            return choices;
        });
    if (index < 0 || index >= devices.size()) {
        return;
    }

    client_.spotify().transferPlaybackToDevice(devices[index].id, true);
}

void LyricsApp::showSonosZonesDialog() {
    QDialog dialog(mainWindow_);
    auto* layout = createOverlayPanel(dialog, mainWindow_, "Sonos Zones", 720, 420);

    auto* statusLabel = new QLabel("Discovering Sonos zones...", &dialog);
    statusLabel->setObjectName("overlayBody");
    layout->addWidget(statusLabel);

    auto* zoneList = new QListWidget(&dialog);
    layout->addWidget(zoneList, 1);

    QCoreApplication::processEvents();

    const auto groups = client_.sonos().getZoneGroups();

    struct FlatEntry {
        QString label;
        QString ip;
        bool isGroup = false;
    };
    QVector<FlatEntry> flatEntries;

    if (groups.isEmpty()) {
        statusLabel->setText("No Sonos zones found. Make sure speakers are on the same network.");
        zoneList->setEnabled(false);
    } else {
        const QString currentIp = client_.sonos().targetSpeakerIp();
        statusLabel->setText(QString("Found %1 zone group(s). Select a speaker to control:").arg(groups.size()));

        for (const auto& grp : groups) {

            QString header;
            if (grp.members.size() > 1) {
                QStringList names;
                for (const auto& m : grp.members) names.push_back(m.name);
                header = QString("Group: %1").arg(names.join(" + "));
            } else {
                header = grp.coordinatorName;
            }

            for (const auto& member : grp.members) {
                const bool isCurrent = (member.ip == currentIp);
                QString label = QString("   %1%2").arg(member.name, isCurrent ? "  (current)" : "");
                if (member.isCoordinator && grp.members.size() > 1) {
                    label += "  [coordinator]";
                }
                auto* item = new QListWidgetItem(label, zoneList);
                item->setData(Qt::UserRole, member.ip);
                if (isCurrent) {
                    item->setSelected(true);
                    zoneList->setCurrentItem(item);
                }
                flatEntries.push_back({label, member.ip, false});
            }
        }
    }

    auto* actions = new QHBoxLayout();
    auto* selectBtn = new QPushButton("Select", &dialog);
    auto* closeBtn = new QPushButton("Close", &dialog);
    selectBtn->setObjectName("primary");
    actions->addWidget(selectBtn);
    actions->addStretch(1);
    actions->addWidget(closeBtn);
    layout->addLayout(actions);

    QObject::connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    QObject::connect(selectBtn, &QPushButton::clicked, &dialog, [this, zoneList, &flatEntries, statusLabel, &dialog]() {
        const int row = zoneList->currentRow();
        if (row < 0 || row >= flatEntries.size()) {
            statusLabel->setText("Select a speaker first.");
            return;
        }
        const QString ip = flatEntries[row].ip;
        if (!ip.isEmpty()) {
            client_.sonos().setTargetSpeakerIp(ip);
            statusLabel->setText(QString("Now controlling: %1 (%2)").arg(flatEntries[row].label.trimmed(), ip));
        }
    });

    dialog.exec();
}

void LyricsApp::showSonosFavouritesDialog() {
    QDialog dialog(mainWindow_);
    auto* layout = createOverlayPanel(dialog, mainWindow_, "Sonos Favourites", 720, 480);

    auto* statusLabel = new QLabel("Loading Sonos favourites...", &dialog);
    statusLabel->setObjectName("overlayBody");
    layout->addWidget(statusLabel);

    auto* list = new QListWidget(&dialog);
    layout->addWidget(list, 1);

    QCoreApplication::processEvents();

    const auto favs = client_.sonos().getFavourites();

    if (favs.isEmpty()) {
        statusLabel->setText("No Sonos favourites found.");
        list->addItem("No favourites available");
        list->setEnabled(false);
    } else {
        statusLabel->setText(QString("%1 favourite(s). Select one to play or add to queue:").arg(favs.size()));
        for (int i = 0; i < favs.size(); ++i) {
            const auto& f = favs[i];
            QString label = f.title;
            if (!f.type.isEmpty() && f.type != "other") {
                label += QString("  (%1)").arg(f.type);
            }
            auto* item = new QListWidgetItem(label, list);
            item->setData(Qt::UserRole, i);
        }
    }

    auto* actions = new QHBoxLayout();
    auto* playBtn = new QPushButton("Play Now", &dialog);
    auto* queueBtn = new QPushButton("Add to Queue", &dialog);
    auto* closeBtn = new QPushButton("Close", &dialog);
    playBtn->setObjectName("primary");
    actions->addWidget(playBtn);
    actions->addWidget(queueBtn);
    actions->addStretch(1);
    actions->addWidget(closeBtn);
    layout->addLayout(actions);

    QObject::connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    QObject::connect(playBtn, &QPushButton::clicked, &dialog, [this, list, &favs, statusLabel]() {
        const int row = list->currentRow();
        if (row < 0 || row >= favs.size()) {
            statusLabel->setText("Select a favourite first.");
            return;
        }
        const auto& fav = favs[row];
        const bool ok = client_.sonos().playUriNow(fav.uri);
        statusLabel->setText(ok ? QString("Playing '%1'.").arg(fav.title)
                                : QString("Failed to play '%1'.").arg(fav.title));
    });

    QObject::connect(queueBtn, &QPushButton::clicked, &dialog, [this, list, &favs, statusLabel]() {
        const int row = list->currentRow();
        if (row < 0 || row >= favs.size()) {
            statusLabel->setText("Select a favourite first.");
            return;
        }
        const auto& fav = favs[row];
        const bool ok = client_.sonos().addToQueueUri(fav.uri, false);
        statusLabel->setText(ok ? QString("Added '%1' to Sonos queue.").arg(fav.title)
                                : QString("Failed to queue '%1'.").arg(fav.title));
    });

    dialog.exec();
}

void LyricsApp::showLocalLibraryDialog() {
    auto settings = config::loadSettings();
    QString libraryPath = settings.value("music_library_path").toString();
    if (libraryPath.trimmed().isEmpty()) {
        libraryPath = QDir::homePath() + "/Music";
    }

    if (!QDir(libraryPath).exists()) {
        showOverlayMessage(mainWindow_, "Local Library", "Library path does not exist. Set it in Settings > Library Path first.");
        return;
    }

    const int modeIndex = showOverlaySelectionDialog(
        mainWindow_,
        "Local Library",
        "Browse local files by:",
        QStringList{"Albums", "Artists"},
        "Open");
    if (modeIndex < 0 || modeIndex > 1) {
        return;
    }

    struct LocalTrackItem {
        QString absolutePath;
        QString relativePath;
        QString artist;
        QString album;
    };

    QVector<LocalTrackItem> tracks;
    QStringList filters{"*.mp3", "*.m4a", "*.flac", "*.wav", "*.aac", "*.ogg", "*.wma", "*.alac"};
    QDirIterator it(libraryPath, filters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext() && tracks.size() < 1000) {
        const QString abs = it.next();
        const QString rel = QDir(libraryPath).relativeFilePath(abs);
        const QStringList parts = rel.split('/', Qt::SkipEmptyParts);

        LocalTrackItem item;
        item.absolutePath = abs;
        item.relativePath = rel;
        item.artist = parts.size() >= 2 ? parts[0] : "Unknown Artist";
        item.album = parts.size() >= 2 ? parts[parts.size() - 2] : "Unknown Album";
        tracks.push_back(item);
    }

    if (tracks.isEmpty()) {
        showOverlayMessage(mainWindow_, "Local Library", "No audio files were found in the selected library path.");
        return;
    }

    QMap<QString, QVector<LocalTrackItem>> grouped;
    for (const auto& track : tracks) {
        const QString key = modeIndex == 0 ? track.album : track.artist;
        grouped[key].push_back(track);
    }

    QStringList groups = grouped.keys();
    groups.sort(Qt::CaseInsensitive);

    const int groupIndex = showOverlaySelectionDialog(
        mainWindow_,
        modeIndex == 0 ? "Local Library • Albums" : "Local Library • Artists",
        "Choose a group:",
        groups,
        "Open");
    if (groupIndex < 0 || groupIndex >= groups.size()) {
        return;
    }

    const QString selectedGroup = groups[groupIndex];
    const auto groupedTracks = grouped.value(selectedGroup);

    QStringList choices;
    choices.reserve(groupedTracks.size());
    for (const auto& track : groupedTracks) {
        choices.push_back(QFileInfo(track.relativePath).completeBaseName());
    }

    const int trackIndex = showOverlaySelectionDialog(
        mainWindow_,
        QString("Local Library • %1").arg(selectedGroup),
        "Select a track:",
        choices,
        "Select");
    if (trackIndex < 0 || trackIndex >= groupedTracks.size()) {
        return;
    }

    showOverlayMessage(mainWindow_, "Local Library", QString("Selected:\n%1\n\nLocal playback routing can now be targeted from queue actions.").arg(groupedTracks[trackIndex].relativePath));
}

void LyricsApp::showQueueDialog() {
    QDialog dialog(mainWindow_);
    auto* layout = createOverlayPanel(dialog, mainWindow_, "Queue", 760, 540);

    auto* npSection = new QLabel("NOW PLAYING", &dialog);
    npSection->setObjectName("overlaySection");
    layout->addWidget(npSection);

    auto* npLabel = new QLabel(
        QString("<b>%1</b><br><span style='color:%3'>%2</span>")
            .arg(latestPlayback_.trackName.isEmpty() ? "No track" : latestPlayback_.trackName.toHtmlEscaped(),
                 latestPlayback_.artistName.isEmpty() ? "Unknown" : latestPlayback_.artistName.toHtmlEscaped(),
                 config::INACTIVE_TEXT_COLOR),
        &dialog);
    npLabel->setStyleSheet(QString("background-color: %1; border-radius: 10px; padding: 12px; font-size: 10pt;").arg(config::CARD_COLOR));
    npLabel->setTextFormat(Qt::RichText);
    layout->addWidget(npLabel);

    auto* sourceSection = new QLabel("SOURCE", &dialog);
    sourceSection->setObjectName("overlaySection");
    layout->addWidget(sourceSection);

    auto* sourceRow = new QHBoxLayout();
    auto* spotifySourceBtn = new QPushButton("Spotify", &dialog);
    auto* sonosSourceBtn = new QPushButton("Sonos", &dialog);
    spotifySourceBtn->setCheckable(true);
    sonosSourceBtn->setCheckable(true);
    spotifySourceBtn->setChecked(true);
    spotifySourceBtn->setObjectName("primary");
    sourceRow->addWidget(spotifySourceBtn);
    sourceRow->addWidget(sonosSourceBtn);
    sourceRow->addStretch(1);
    layout->addLayout(sourceRow);

    QObject::connect(spotifySourceBtn, &QPushButton::clicked, &dialog, [spotifySourceBtn, sonosSourceBtn]() {
        spotifySourceBtn->setChecked(true);
        sonosSourceBtn->setChecked(false);
        spotifySourceBtn->setObjectName("primary");
        sonosSourceBtn->setObjectName("");
        spotifySourceBtn->style()->unpolish(spotifySourceBtn);
        spotifySourceBtn->style()->polish(spotifySourceBtn);
        sonosSourceBtn->style()->unpolish(sonosSourceBtn);
        sonosSourceBtn->style()->polish(sonosSourceBtn);
    });
    QObject::connect(sonosSourceBtn, &QPushButton::clicked, &dialog, [spotifySourceBtn, sonosSourceBtn]() {
        sonosSourceBtn->setChecked(true);
        spotifySourceBtn->setChecked(false);
        sonosSourceBtn->setObjectName("primary");
        spotifySourceBtn->setObjectName("");
        spotifySourceBtn->style()->unpolish(spotifySourceBtn);
        spotifySourceBtn->style()->polish(spotifySourceBtn);
        sonosSourceBtn->style()->unpolish(sonosSourceBtn);
        sonosSourceBtn->style()->polish(sonosSourceBtn);
    });

    auto* queueSection = new QLabel("QUEUE", &dialog);
    queueSection->setObjectName("overlaySection");
    layout->addWidget(queueSection);

    auto* queueList = new QListWidget(&dialog);
    queueList->setDragDropMode(QAbstractItemView::InternalMove);
    queueList->setDefaultDropAction(Qt::MoveAction);
    queueList->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(queueList, 1);

    auto* statusLabel = new QLabel("", &dialog);
    statusLabel->setObjectName("overlayBody");
    layout->addWidget(statusLabel);

    struct QueueEntry {
        QString name;
        QString artist;
        QString uri;
    };
    auto* entries = new QVector<QueueEntry>();

    auto loadQueue = [this, queueList, statusLabel, spotifySourceBtn, entries]() {
        queueList->clear();
        entries->clear();

        if (spotifySourceBtn->isChecked()) {
            statusLabel->setText("Loading Spotify queue...");
            QCoreApplication::processEvents();
            const auto tracks = client_.spotify().getQueue();
            if (tracks.isEmpty()) {
                queueList->addItem("Queue is empty");
                queueList->setEnabled(false);
                statusLabel->setText("No tracks in Spotify queue.");
                return;
            }
            queueList->setEnabled(true);

            for (int i = 0; i < tracks.size(); ++i) {
                const auto& t = tracks[i];
                const QString prefix = (i == 0) ? QString::fromUtf8("\u25B6 ") : QString("%1.  ").arg(i);
                auto* item = new QListWidgetItem(QString("%1%2 — %3").arg(prefix, t.name, t.artist));
                item->setData(Qt::UserRole, i);
                if (i == 0) {

                    item->setFlags(item->flags() & ~Qt::ItemIsDragEnabled);
                }
                queueList->addItem(item);
                entries->push_back({t.name, t.artist, t.uri});
            }
            statusLabel->setText(QString("%1 track(s) in Spotify queue.").arg(tracks.size()));
        } else {
            statusLabel->setText("Loading Sonos queue...");
            QCoreApplication::processEvents();
            const auto items = client_.sonos().getQueue();
            if (items.isEmpty()) {
                queueList->addItem("Queue is empty (or Sonos unavailable)");
                queueList->setEnabled(false);
                statusLabel->setText("No tracks in Sonos queue.");
                return;
            }
            queueList->setEnabled(true);
            for (int i = 0; i < items.size(); ++i) {
                const auto& t = items[i];
                auto* item = new QListWidgetItem(QString("%1.  %2 — %3").arg(i + 1).arg(t.title, t.artist));
                item->setData(Qt::UserRole, i);
                queueList->addItem(item);
                entries->push_back({t.title, t.artist, t.uri});
            }
            statusLabel->setText(QString("%1 track(s) in Sonos queue.").arg(items.size()));
        }
    };

    QObject::connect(spotifySourceBtn, &QPushButton::clicked, &dialog, [loadQueue]() { loadQueue(); });
    QObject::connect(sonosSourceBtn, &QPushButton::clicked, &dialog, [loadQueue]() { loadQueue(); });

    loadQueue();

    auto* actions = new QHBoxLayout();
    auto* removeBtn = new QPushButton("Remove", &dialog);
    auto* refreshBtn = new QPushButton("Refresh", &dialog);
    auto* closeButton = new QPushButton("Close", &dialog);
    removeBtn->setToolTip("Remove the selected track from the queue");
    actions->addWidget(removeBtn);
    actions->addWidget(refreshBtn);
    actions->addStretch(1);
    actions->addWidget(closeButton);

    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);

    QObject::connect(refreshBtn, &QPushButton::clicked, &dialog, [loadQueue]() { loadQueue(); });

    QObject::connect(removeBtn, &QPushButton::clicked, &dialog, [this, queueList, statusLabel, spotifySourceBtn, entries, loadQueue]() {
        const int row = queueList->currentRow();
        if (row < 0 || row >= entries->size()) {
            statusLabel->setText("Select a track to remove.");
            return;
        }

        if (spotifySourceBtn->isChecked()) {

            if (row == 0) {
                statusLabel->setText("Cannot remove the currently playing track.");
                return;
            }
            statusLabel->setText("Spotify API does not support removing individual queue items. Use skip (→) instead.");
            return;
        } else {

            const bool ok = client_.sonos().removeFromQueue(row);
            if (ok) {
                statusLabel->setText(QString("Removed '%1' from Sonos queue.").arg((*entries)[row].name));
                loadQueue();
            } else {
                statusLabel->setText(QString("Failed to remove '%1'.").arg((*entries)[row].name));
            }
        }
    });

    layout->addLayout(actions);
    dialog.exec();

    delete entries;
}

bool LyricsApp::playSpotifyCatalogItemNow(const SpotifyCatalogItem& item) {
    const QString type = item.type.trimmed().toLower();
    if (type == "track") {
        return client_.spotify().startPlaybackUri(item.uri);
    }
    return client_.spotify().startPlaybackContext(item.uri);
}

bool LyricsApp::queueSpotifyCatalogItem(const SpotifyCatalogItem& item, const QString& target) {
    const QString normalizedTarget = target.trimmed().toLower();
    const QString type = item.type.trimmed().toLower();

    QStringList uris;
    if (type == "track") {
        if (!item.uri.trimmed().isEmpty()) {
            uris.push_back(item.uri.trimmed());
        }
    } else if (type == "album") {
        const auto tracks = client_.spotify().getAlbumTracks(item.id, 25);
        for (const auto& track : tracks) {
            if (!track.uri.trimmed().isEmpty()) {
                uris.push_back(track.uri.trimmed());
            }
        }
    } else if (type == "playlist") {
        const auto tracks = client_.spotify().getPlaylistTracks(item.id, 25);
        for (const auto& track : tracks) {
            if (!track.uri.trimmed().isEmpty()) {
                uris.push_back(track.uri.trimmed());
            }
        }
    } else if (type == "artist") {
        const auto tracks = client_.spotify().getArtistTopTracks(item.id, 10);
        for (const auto& track : tracks) {
            if (!track.uri.trimmed().isEmpty()) {
                uris.push_back(track.uri.trimmed());
            }
        }
    }

    if (uris.isEmpty()) {
        return false;
    }

    int successCount = 0;
    for (const auto& uri : uris) {
        bool ok = false;
        if (normalizedTarget == "spotify") {
            ok = client_.spotify().addToQueue(uri);
        } else if (normalizedTarget == "sonos") {
            ok = client_.sonos().addToQueueUri(uri, true);
        }
        if (ok) {
            ++successCount;
        }
    }

    return successCount > 0;
}

void LyricsApp::showAppearanceDialog() {
    auto settings = config::loadSettings();
    const auto palette = wintools::themes::ThemeHelper::currentPalette();

    QDialog dialog(mainWindow_);
    auto* panel = createOverlayPanel(dialog, mainWindow_, "Mini Player Style", 640, 640);
    dialog.setStyleSheet(dialog.styleSheet() +
        "QFrame#overlayPanel { border-radius: 24px; }"
        "QLabel#overlayTitle { letter-spacing: 0.4px; }");

    auto* miniSection = new QLabel("MINI PLAYER", &dialog);
    miniSection->setObjectName("overlaySection");
    panel->addWidget(miniSection);

    auto* miniCard = new QFrame(&dialog);
    miniCard->setStyleSheet(QString("QFrame { background-color: %1; border-radius: 10px; padding: 4px; }").arg(palette.cardBackground.name()));
    auto* miniLayout = new QVBoxLayout(miniCard);
    miniLayout->setContentsMargins(14, 10, 14, 10);
    miniLayout->setSpacing(8);

    auto* miniHint = new QLabel("Use hex colors or leave values as theme defaults. Turn on transparency for a glass-like mini player.", miniCard);
    miniHint->setObjectName("overlayBody");
    miniHint->setWordWrap(true);
    miniLayout->addWidget(miniHint);

    auto* transparentMini = new QCheckBox("Transparent mini player background", miniCard);
    transparentMini->setChecked(settings.value("mini_player_transparent").toBool(false));
    miniLayout->addWidget(transparentMini);

    auto* miniBgRow = new QHBoxLayout();
    auto* miniBgLabel = new QLabel("Background color", miniCard);
    miniBgLabel->setStyleSheet(QString("color: %1; font-size: 10pt;").arg(palette.foreground.name()));
    auto* miniBgColor = new QLineEdit(miniCard);
    miniBgColor->setPlaceholderText(palette.cardBackground.name());
    miniBgColor->setText(settings.value("mini_player_bg_color").toString(palette.cardBackground.name()));
    miniBgRow->addWidget(miniBgLabel);
    miniBgRow->addStretch(1);
    miniBgRow->addWidget(miniBgColor);
    miniLayout->addLayout(miniBgRow);

    auto* miniControlRow = new QHBoxLayout();
    auto* miniControlLabel = new QLabel("Controls color", miniCard);
    miniControlLabel->setStyleSheet(QString("color: %1; font-size: 10pt;").arg(palette.foreground.name()));
    auto* miniControlColor = new QLineEdit(miniCard);
    miniControlColor->setPlaceholderText(palette.windowBackground.name());
    miniControlColor->setText(settings.value("mini_player_control_color").toString(palette.windowBackground.name()));
    miniControlRow->addWidget(miniControlLabel);
    miniControlRow->addStretch(1);
    miniControlRow->addWidget(miniControlColor);
    miniLayout->addLayout(miniControlRow);

    auto* miniTextRow = new QHBoxLayout();
    auto* miniTextLabel = new QLabel("Text color", miniCard);
    miniTextLabel->setStyleSheet(QString("color: %1; font-size: 10pt;").arg(palette.foreground.name()));
    auto* miniTextColor = new QLineEdit(miniCard);
    miniTextColor->setPlaceholderText(palette.foreground.name());
    miniTextColor->setText(settings.value("mini_player_text_color").toString(palette.foreground.name()));
    miniTextRow->addWidget(miniTextLabel);
    miniTextRow->addStretch(1);
    miniTextRow->addWidget(miniTextColor);
    miniLayout->addLayout(miniTextRow);

    auto* opacityRow = new QHBoxLayout();

    auto* opacityLabel = new QLabel("Opacity", miniCard);
    opacityLabel->setStyleSheet(QString("color: %1; font-size: 10pt;").arg(palette.foreground.name()));
    auto* miniOpacity = new QDoubleSpinBox(miniCard);
    miniOpacity->setRange(0.3, 1.0);
    miniOpacity->setSingleStep(0.05);
    miniOpacity->setValue(settings.value("mini_player_opacity").toDouble(0.9));
    opacityRow->addWidget(opacityLabel);
    opacityRow->addStretch(1);
    opacityRow->addWidget(miniOpacity);
    miniLayout->addLayout(opacityRow);

    panel->addWidget(miniCard);

    auto* previewToggle = new QCheckBox("Show mini player preview", &dialog);
    previewToggle->setChecked(true);
    panel->addWidget(previewToggle);

    auto* previewFrame = new QFrame(&dialog);
    previewFrame->setObjectName("miniPreviewFrame");
    auto* previewLayout = new QVBoxLayout(previewFrame);
    previewLayout->setContentsMargins(12, 10, 12, 10);
    previewLayout->setSpacing(8);

    auto* previewTitle = new QLabel("LIVE MINI PREVIEW", previewFrame);
    previewTitle->setObjectName("overlaySection");
    previewLayout->addWidget(previewTitle);

    auto* miniPreview = new QFrame(previewFrame);
    miniPreview->setObjectName("miniPreviewBar");
    miniPreview->setMinimumHeight(44);
    auto* miniPreviewRow = new QHBoxLayout(miniPreview);
    miniPreviewRow->setContentsMargins(8, 4, 8, 4);
    miniPreviewRow->setSpacing(8);

    auto* previewArt = new QLabel(QString::fromUtf8("♪"), miniPreview);
    previewArt->setMinimumWidth(28);
    previewArt->setAlignment(Qt::AlignCenter);
    previewArt->setObjectName("miniPreviewText");

    auto* previewTrack = new QLabel("Example Track  •  Example Artist", miniPreview);
    previewTrack->setObjectName("miniPreviewText");
    previewTrack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    auto* previewSource = new QLabel(QString::fromUtf8("▶ Spotify"), miniPreview);
    previewSource->setObjectName("miniPreviewText");

    auto* previewHeart = new QLabel(QString::fromUtf8("♡"), miniPreview);
    previewHeart->setAlignment(Qt::AlignCenter);
    previewHeart->setMinimumWidth(24);
    previewHeart->setObjectName("miniPreviewControl");

    auto* previewPrev = new QLabel(QString::fromUtf8("⏮"), miniPreview);
    previewPrev->setAlignment(Qt::AlignCenter);
    previewPrev->setMinimumWidth(24);
    previewPrev->setObjectName("miniPreviewControl");

    auto* previewPlay = new QLabel(QString::fromUtf8("⏯"), miniPreview);
    previewPlay->setAlignment(Qt::AlignCenter);
    previewPlay->setMinimumWidth(24);
    previewPlay->setObjectName("miniPreviewControl");

    auto* previewNext = new QLabel(QString::fromUtf8("⏭"), miniPreview);
    previewNext->setAlignment(Qt::AlignCenter);
    previewNext->setMinimumWidth(24);
    previewNext->setObjectName("miniPreviewControl");

    miniPreviewRow->addWidget(previewArt);
    miniPreviewRow->addWidget(previewTrack, 1);
    miniPreviewRow->addWidget(previewSource);
    miniPreviewRow->addWidget(previewHeart);
    miniPreviewRow->addWidget(previewPrev);
    miniPreviewRow->addWidget(previewPlay);
    miniPreviewRow->addWidget(previewNext);
    previewLayout->addWidget(miniPreview);

    panel->addWidget(previewFrame);

    const auto updateColorInputState = [&palette](QLineEdit* input, bool valid) {
        if (!input) {
            return;
        }
        if (valid) {
            input->setStyleSheet(QString());
            return;
        }
        input->setStyleSheet(QString("QLineEdit { border: 1px solid %1; border-radius: 10px; }").arg(palette.dangerRed.name()));
    };

    const auto updateMiniPreview = [&]() {
        const QString bgText = miniBgColor->text().trimmed();
        const QString controlText = miniControlColor->text().trimmed();
        const QString textText = miniTextColor->text().trimmed();

        const QColor parsedBgColor(bgText);
        const QColor parsedControlColor(controlText);
        const QColor parsedTextColor(textText);

        const bool bgValid = parsedBgColor.isValid();
        const bool controlValid = parsedControlColor.isValid();
        const bool textValid = parsedTextColor.isValid();

        updateColorInputState(miniBgColor, bgValid);
        updateColorInputState(miniControlColor, controlValid);
        updateColorInputState(miniTextColor, textValid);

        const QColor bgColor = bgValid ? parsedBgColor : palette.cardBackground;
        const QColor controlColor = controlValid ? parsedControlColor : palette.windowBackground;
        const QColor textColor = textValid ? parsedTextColor : palette.foreground;
        const bool transparent = transparentMini->isChecked();

        const double opacityValue = std::clamp(miniOpacity->value(), 0.0, 1.0);
        QColor barColor = transparent ? QColor(0, 0, 0, 0) : bgColor;
        barColor.setAlphaF(std::clamp(opacityValue, 0.0, 1.0));

        QColor controlBg = transparent ? palette.hoverBackground : controlColor;
        if (transparent) {
            controlBg.setAlpha(70);
        }
        controlBg.setAlphaF(std::clamp(opacityValue, 0.0, 1.0));

        const QString style = QString(
            "QFrame#miniPreviewFrame { background-color: %1; border-radius: 16px; border: 1px solid %2; }"
            "QFrame#miniPreviewBar { background-color: %3; border-radius: 14px; border: 1px solid %2; }"
            "QLabel#miniPreviewText { color: %4; font-size: 9pt; }"
            "QLabel#miniPreviewControl { color: %4; background-color: %5; border-radius: 8px; padding: 2px 6px; }")
              .arg(palette.cardBackground.name(),
                  palette.cardBorder.name(),
                 barColor.name(QColor::HexArgb),
                 textColor.name(),
                 controlBg.name(QColor::HexArgb));

        previewFrame->setStyleSheet(style);
        previewFrame->setVisible(previewToggle->isChecked());
    };

    QObject::connect(previewToggle, &QCheckBox::toggled, &dialog, [&](bool) {
        updateMiniPreview();
    });
    QObject::connect(transparentMini, &QCheckBox::toggled, &dialog, [&](bool) {
        updateMiniPreview();
    });
    QObject::connect(miniBgColor, &QLineEdit::textChanged, &dialog, [&](const QString&) {
        updateMiniPreview();
    });
    QObject::connect(miniControlColor, &QLineEdit::textChanged, &dialog, [&](const QString&) {
        updateMiniPreview();
    });
    QObject::connect(miniTextColor, &QLineEdit::textChanged, &dialog, [&](const QString&) {
        updateMiniPreview();
    });
    QObject::connect(miniOpacity, &QDoubleSpinBox::valueChanged, &dialog, [&](double) {
        updateMiniPreview();
    });

    updateMiniPreview();

    panel->addStretch(1);

    auto* actions = new QHBoxLayout();
    actions->addStretch(1);
    auto* cancelButton = new QPushButton("Cancel", &dialog);
    auto* saveButton = new QPushButton("Save", &dialog);
    saveButton->setObjectName("primary");
    QObject::connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    QObject::connect(saveButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    actions->addWidget(cancelButton);
    actions->addWidget(saveButton);
    panel->addLayout(actions);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString bgColor = miniBgColor->text().trimmed();
    const QString controlColor = miniControlColor->text().trimmed();
    const QString textColor = miniTextColor->text().trimmed();
    const QColor parsedBgColor(bgColor);
    const QColor parsedControlColor(controlColor);
    const QColor parsedTextColor(textColor);
    if (!parsedBgColor.isValid() || !parsedControlColor.isValid() || !parsedTextColor.isValid()) {
        showOverlayMessage(mainWindow_, "Appearance", "Please enter valid color values (example: #2d2d2d).\nTip: use the theme-colored defaults shown in the placeholders.");
        return;
    }

    settings.insert("mini_player_transparent", transparentMini->isChecked());
    settings.insert("mini_player_bg_color", bgColor);
    settings.insert("mini_player_control_color", controlColor);
    settings.insert("mini_player_text_color", textColor);
    settings.insert("mini_player_opacity", miniOpacity->value());
    config::saveSettings(settings);

    if (miniMode_ && miniPlayerWindow_) {
        miniPlayerWindow_->showDesktopMini();
    }

    showOverlayMessage(mainWindow_, "Appearance", "Appearance settings saved successfully.");
}

void LyricsApp::showLibraryPathDialog() {
    auto settings = config::loadSettings();
    QString currentPath = settings.value("music_library_path").toString();
    if (currentPath.trimmed().isEmpty()) {
        currentPath = QDir::homePath() + "/Music";
    }

    const auto pathOpt = showOverlayTextInput(mainWindow_,
        "Library Path",
        "Enter the folder path where your music files are stored:",
        "Save",
        currentPath);
    if (!pathOpt.has_value()) {
        return;
    }

    const QString path = pathOpt.value();
    if (path.trimmed().isEmpty()) {
        return;
    }

    if (!QDir(path).exists()) {
        showOverlayMessage(mainWindow_, "Library Path", "That folder path does not exist. Please check and try again.");
        return;
    }

    settings.insert("music_library_path", path);
    config::saveSettings(settings);
    showOverlayMessage(mainWindow_, "Library Path", "Music library path updated successfully.");
}

void LyricsApp::showDebugDialog() {
    QDialog dialog(mainWindow_);
    auto* layout = createOverlayPanel(dialog, mainWindow_, "Debug", 780, 540);
    auto settings = config::loadSettings();
    QTextEdit* settingsDump = nullptr;

    auto* statusSection = new QLabel("STATUS", &dialog);
    statusSection->setObjectName("overlaySection");
    layout->addWidget(statusSection);

    auto* statusCard = new QFrame(&dialog);
    statusCard->setStyleSheet(QString("QFrame { background-color: %1; border-radius: 10px; padding: 4px; }").arg(config::CARD_COLOR));
    auto* statusLayout = new QVBoxLayout(statusCard);
    statusLayout->setContentsMargins(14, 10, 14, 10);
    statusLayout->setSpacing(6);

    auto addStatusRow = [&](const QString& label, const QString& value) {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(label, statusCard);
        lbl->setStyleSheet(QString("color: %1; font-size: 9pt;").arg(config::INACTIVE_TEXT_COLOR));
        auto* val = new QLabel(value, statusCard);
        val->setStyleSheet(QString("color: %1; font-size: 9pt; font-weight: 600;").arg(config::TEXT_COLOR));
        row->addWidget(lbl);
        row->addStretch(1);
        row->addWidget(val);
        statusLayout->addLayout(row);
    };

    addStatusRow("Source Mode", client_.sourceModeToString());
    addStatusRow("Active Source", latestSourceName_.isEmpty() ? "Unknown" : latestSourceName_);
    addStatusRow("Mini Mode", miniMode_ ? "Active" : "Inactive");
    addStatusRow("Spotify", client_.spotify().isAvailable() ? QString("Connected") : QString("Disconnected"));
    addStatusRow("Sonos", client_.sonos().isAvailable() ? QString("Connected") : QString("Disconnected"));
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 snapshotAgeMs = lastPlaybackSnapshotAtMs_ > 0 ? std::max<qint64>(0, now - lastPlaybackSnapshotAtMs_) : -1;
    addStatusRow("Playback Snapshot Age", snapshotAgeMs >= 0 ? QString("%1 ms").arg(snapshotAgeMs) : QString("N/A"));
    addStatusRow("Playback Misses", QString::number(consecutivePlaybackMisses_));
    addStatusRow("Stale Tick Count", QString::number(stalePlaybackTickCount_));
    addStatusRow("Current Track", latestPlayback_.trackName.isEmpty() ? "None" : latestPlayback_.trackName);
    addStatusRow("Track ID", latestPlayback_.trackId.isEmpty() ? "N/A" : latestPlayback_.trackId);

    layout->addWidget(statusCard);

    auto* spotifySection = new QLabel("SPOTIFY", &dialog);
    spotifySection->setObjectName("overlaySection");
    layout->addWidget(spotifySection);

    auto* spotifyCard = new QFrame(&dialog);
    spotifyCard->setStyleSheet(QString("QFrame { background-color: %1; border-radius: 10px; padding: 4px; }").arg(config::CARD_COLOR));
    auto* spotifyLayout = new QVBoxLayout(spotifyCard);
    spotifyLayout->setContentsMargins(14, 10, 14, 10);
    spotifyLayout->setSpacing(8);

    auto* spotifyHint = new QLabel(
        "Update the Spotify Client ID used by MediaBar OAuth.\n"
        "If SPOTIFY_CLIENT_ID is set as an environment variable, it overrides this value.",
        spotifyCard);
    spotifyHint->setObjectName("overlayBody");
    spotifyHint->setWordWrap(true);
    spotifyLayout->addWidget(spotifyHint);

    auto* clientIdInput = new QLineEdit(spotifyCard);
    clientIdInput->setPlaceholderText("Spotify Client ID");
    clientIdInput->setText(settings.value("spotify_client_id").toString().trimmed());
    spotifyLayout->addWidget(clientIdInput);

    auto* spotifyActions = new QHBoxLayout();
    spotifyActions->addStretch(1);
    auto* saveClientIdButton = new QPushButton("Save Client ID", spotifyCard);
    saveClientIdButton->setObjectName("primary");
    spotifyActions->addWidget(saveClientIdButton);
    spotifyLayout->addLayout(spotifyActions);

    QObject::connect(saveClientIdButton, &QPushButton::clicked, &dialog, [this, clientIdInput, &dialog, &settingsDump]() {
        auto updated = config::loadSettings();
        updated.insert("spotify_client_id", clientIdInput->text().trimmed());
        if (!config::saveSettings(updated)) {
            showOverlayMessage(mainWindow_, "Debug", "Could not save Spotify Client ID.");
            return;
        }

        if (settingsDump) {
            settingsDump->setPlainText(QString::fromUtf8(QJsonDocument(updated).toJson(QJsonDocument::Indented)));
        }

        showOverlayMessage(
            mainWindow_,
            "Debug",
            "Spotify Client ID saved. If SPOTIFY_CLIENT_ID is set in your environment, that value takes precedence.");
    });

    layout->addWidget(spotifyCard);

    auto* settingsSection = new QLabel("RAW SETTINGS", &dialog);
    settingsSection->setObjectName("overlaySection");
    layout->addWidget(settingsSection);

    auto* text = new QTextEdit(&dialog);
    text->setReadOnly(true);
    text->setPlainText(QString::fromUtf8(QJsonDocument(settings).toJson(QJsonDocument::Indented)));
    text->setStyleSheet(text->styleSheet() + "font-family: 'Consolas', 'Courier New', monospace; font-size: 9pt;");
    layout->addWidget(text, 1);
    settingsDump = text;

    auto* actions = new QHBoxLayout();
    actions->addStretch(1);
    auto* closeButton = new QPushButton("Close", &dialog);
    closeButton->setObjectName("primary");
    QObject::connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    actions->addWidget(closeButton);
    layout->addLayout(actions);

    dialog.exec();
}

void LyricsApp::showMiniSourceMenu() {
    if (!miniPlayerWindow_ || !mainWindow_) {
        return;
    }

    QMenu menu(miniPlayerWindow_);
    menu.setStyleSheet(buildMiniPopupMenuQss(wintools::themes::ThemeHelper::currentPalette()));

    auto* spotifyAction = menu.addAction("Spotify");
    auto* sonosAction = menu.addAction("Sonos");

    spotifyAction->setCheckable(true);
    sonosAction->setCheckable(true);

    const QString mode = client_.sourceModeToString();
    spotifyAction->setChecked(mode == "spotify");
    sonosAction->setChecked(mode == "sonos");

    miniPlayerWindow_->beginPopupInteraction();
    QAction* selected = menu.exec(menuPointAboveWidget(miniPlayerWindow_, miniPlayerWindow_->sourceButton(), menu.sizeHint(), 18));
    miniPlayerWindow_->endPopupInteraction();
    if (!selected) {
        return;
    }

    QString nextMode = mode;
    if (selected == spotifyAction) {
        nextMode = "spotify";
    } else if (selected == sonosAction) {
        nextMode = "sonos";
    }

    client_.setSourceModeFromString(nextMode);
    mainWindow_->setSourceMode(nextMode);
    auto settings = config::loadSettings();
    settings.insert("playback_source", client_.sourceModeToString());
    config::saveSettings(settings);
}

void LyricsApp::toggleCurrentTrackLike() {
    if (latestPlayback_.source != "spotify" || latestPlayback_.trackId.trimmed().isEmpty()) {
        return;
    }

    const bool currentlyLiked = currentTrackLiked_.has_value() && currentTrackLiked_.value();
    bool ok = false;
    if (currentlyLiked) {
        ok = client_.spotify().removeTrack(latestPlayback_.trackId);
    } else {
        ok = client_.spotify().saveTrack(latestPlayback_.trackId);
    }

    if (ok) {
        currentTrackLiked_ = !currentlyLiked;
        currentLikedTrackId_ = latestPlayback_.trackId;
        miniPlayerWindow_->setLikedState(currentTrackLiked_, true);
        if (mainWindow_) {
            mainWindow_->setFavoriteState(currentTrackLiked_, true);
        }
    }
}

void LyricsApp::showMiniPinnedMenu() {
    if (!miniPlayerWindow_) {
        return;
    }

    QMenu menu(miniPlayerWindow_);
    menu.setStyleSheet(buildMiniPopupMenuQss(wintools::themes::ThemeHelper::currentPalette()));
    auto* pinCurrent = menu.addAction("+ Track");
    auto* pinContext = menu.addAction("+ Context");
    auto* addLink = menu.addAction("+ Link...");
    auto* openFull = menu.addAction("Show Full Window");
    menu.addSeparator();

    auto settings = config::loadSettings();
    QJsonArray favorites = settings.value("favorite_items").toArray();

    QVector<QString> favoriteUris;
    QSet<QString> favoriteSet;
    for (const auto& value : favorites) {
        const QJsonObject obj = value.toObject();
        const QString name = obj.value("name").toString();
        const QString uri = obj.value("uri").toString();
        if (name.isEmpty() || uri.isEmpty()) {
            continue;
        }
        favoriteUris.push_back(uri);
        favoriteSet.insert(uri);

        auto* playAction = menu.addAction(QString("▶ %1").arg(name));
        QObject::connect(playAction, &QAction::triggered, [this, uri]() {
            client_.spotify().startPlaybackUri(uri);
        });

        auto* removeAction = menu.addAction(QString("   ✕ Remove %1").arg(name));
        QObject::connect(removeAction, &QAction::triggered, [uri]() {
            auto settingsInner = config::loadSettings();
            QJsonArray list = settingsInner.value("favorite_items").toArray();
            QJsonArray next;
            for (const auto& item : list) {
                if (item.toObject().value("uri").toString() != uri) {
                    next.append(item);
                }
            }
            settingsInner.insert("favorite_items", next);
            config::saveSettings(settingsInner);
        });
    }

    if (favoriteUris.isEmpty()) {
        auto* empty = menu.addAction("(No pinned items)");
        empty->setEnabled(false);
    }

    miniPlayerWindow_->beginPopupInteraction();
    QAction* selected = menu.exec(menuPointAboveWidget(miniPlayerWindow_, miniPlayerWindow_->listButton(), menu.sizeHint(), 18));
    miniPlayerWindow_->endPopupInteraction();
    if (!selected) {
        return;
    }

    if (selected == openFull) {
        exitMiniMode();
        return;
    }

    if (selected == addLink) {
        bool ok = false;
        const QString raw = QInputDialog::getText(mainWindow_, "Add Spotify Link", "Paste Spotify link or URI:", QLineEdit::Normal, "", &ok);
        if (!ok || raw.trimmed().isEmpty()) {
            return;
        }

        const QString uri = normalizeSpotifyUri(raw);
        if (uri.isEmpty()) {
            QMessageBox::warning(mainWindow_, "My 5", "Could not parse a Spotify URI from that input.");
            return;
        }

        if (favoriteSet.contains(uri)) {
            return;
        }

        QJsonObject item;
        item.insert("name", uri.section(':', -2));
        item.insert("uri", uri);
        item.insert("type", uri.section(':', 1, 1));
        favorites.append(item);
        while (favorites.size() > 5) {
            favorites.removeFirst();
        }
        settings.insert("favorite_items", favorites);
        config::saveSettings(settings);
        return;
    }

    if (selected == pinContext) {
        if (latestPlayback_.source != "spotify" || latestPlayback_.albumUri.trimmed().isEmpty()) {
            return;
        }

        const QString contextUri = latestPlayback_.albumUri.trimmed();
        if (favoriteSet.contains(contextUri)) {
            return;
        }

        QJsonObject ctx;
        ctx.insert("name", latestPlayback_.albumName.isEmpty() ? QString("Current Context") : latestPlayback_.albumName);
        ctx.insert("uri", contextUri);
        ctx.insert("type", "album");
        favorites.append(ctx);
        while (favorites.size() > 5) {
            favorites.removeFirst();
        }
        settings.insert("favorite_items", favorites);
        config::saveSettings(settings);
        return;
    }

    if (selected != pinCurrent) {
        return;
    }

    const QString uri = latestPlayback_.trackUri.trimmed();
    if (latestPlayback_.source != "spotify" || uri.isEmpty() || favoriteSet.contains(uri)) {
        return;
    }

    QJsonObject item;
    item.insert("name", QString("%1 — %2").arg(latestPlayback_.trackName, latestPlayback_.artistName));
    item.insert("uri", uri);
    item.insert("type", "track");
    favorites.append(item);
    while (favorites.size() > 5) {
        favorites.removeFirst();
    }
    settings.insert("favorite_items", favorites);
    config::saveSettings(settings);
}

void LyricsApp::showMiniTopItemsMenu() {
    if (!miniPlayerWindow_) {
        return;
    }

    QMenu menu(miniPlayerWindow_);
    menu.setStyleSheet(buildMiniPopupMenuQss(wintools::themes::ThemeHelper::currentPalette()));

    QIcon placeholder;
    {
        const auto palette = wintools::themes::ThemeHelper::currentPalette();
        QPixmap placeholderPix(26, 26);
        placeholderPix.fill(Qt::transparent);
        QPainter p(&placeholderPix);
        p.setRenderHint(QPainter::Antialiasing, true);
        QFont font = p.font();
        font.setPointSize(11);
        font.setBold(true);
        p.setFont(font);
        p.setPen(palette.mutedForeground);
        p.drawText(placeholderPix.rect(), Qt::AlignCenter, QString::fromUtf8("♪"));
        placeholder = QIcon(placeholderPix);
    }

    auto* artLoader = new QNetworkAccessManager(&menu);

    const auto topTracks = client_.spotify().getTopTracks(10);
    if (topTracks.isEmpty()) {
        auto* empty = menu.addAction("(No top items available)");
        empty->setEnabled(false);
    } else {
        for (const auto& track : topTracks) {
            auto* action = menu.addAction(QString("%1 — %2").arg(track.name, track.artist));
            action->setIcon(placeholder);
            if (!track.albumArtUrl.trimmed().isEmpty()) {
                QNetworkReply* reply = artLoader->get(QNetworkRequest(QUrl(track.albumArtUrl)));
                QPointer<QNetworkReply> safeReply(reply);
                QObject::connect(reply, &QNetworkReply::finished, &menu, [&menu, action, reply]() {
                    const QByteArray data = reply->readAll();
                    const auto err = reply->error();
                    reply->deleteLater();

                    if (err != QNetworkReply::NoError || data.isEmpty()) {
                        return;
                    }

                    QPixmap art;
                    if (!art.loadFromData(data)) {
                        return;
                    }

                    action->setIcon(QIcon(art.scaled(26, 26, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
                    if (menu.isVisible()) {
                        menu.update();
                    }
                });
                QTimer::singleShot(2500, &menu, [safeReply]() {
                    if (safeReply && safeReply->isRunning()) {
                        safeReply->abort();
                    }
                });
                }
            QObject::connect(action, &QAction::triggered, [this, track]() {
                client_.spotify().startPlaybackUri(track.uri);
            });
        }
    }

    miniPlayerWindow_->beginPopupInteraction();
    menu.exec(menuPointAboveWidget(miniPlayerWindow_, miniPlayerWindow_->artButton(), menu.sizeHint(), 28));
    miniPlayerWindow_->endPopupInteraction();
}

QString LyricsApp::normalizeSpotifyUri(const QString& input) const {
    const QString raw = input.trimmed();
    if (raw.startsWith("spotify:")) {
        return raw;
    }

    QRegularExpression re(R"(open\.spotify\.com/(track|album|playlist|artist)/([A-Za-z0-9]+))");
    const auto match = re.match(raw);
    if (!match.hasMatch()) {
        return QString();
    }
    const QString type = match.captured(1);
    const QString id = match.captured(2);
    if (type.isEmpty() || id.isEmpty()) {
        return QString();
    }
    return QString("spotify:%1:%2").arg(type, id);
}

void LyricsApp::refreshLikeState(const PlaybackInfo& playback) {
    if (playback.source != "spotify" || playback.trackId.trimmed().isEmpty()) {
        currentLikedTrackId_.clear();
        currentTrackLiked_ = std::nullopt;
        return;
    }

    if (playback.trackId == currentLikedTrackId_ && currentTrackLiked_.has_value()) {
        return;
    }

    const auto liked = client_.spotify().isTrackSaved(playback.trackId);
    currentLikedTrackId_ = playback.trackId;
    currentTrackLiked_ = liked;
}

void LyricsApp::updateUi() {
    if (!mainWindow_) {
        return;
    }

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();

    std::optional<PlaybackInfo> playback;
    {
        QMutexLocker locker(&playbackMutex_);
        playback = cachedPlayback_;
    }

    if (playback.has_value()) {
        const QString sourceName = client_.getSourceName();
        const QString sourceId = playback->source.trimmed().toLower();
        refreshLikeState(playback.value());
        mainWindow_->updateTrackInfo(playback.value(), sourceName);
        mainWindow_->updatePlayPauseButton(playback->isPlaying);

        qint64 displayProgressMs = std::max<qint64>(0, playback->progressMs);
        if (playback->isPlaying && playback->durationMs > 0 && lastPlaybackSnapshotAtMs_ > 0) {

            const qint64 elapsedMs = std::max<qint64>(0, nowMs - lastPlaybackSnapshotAtMs_);
            displayProgressMs = std::min<qint64>(playback->durationMs, displayProgressMs + elapsedMs);
        }

        if (lastPlaybackSnapshotAtMs_ > 0) {
            const qint64 playbackSnapshotAgeMs = std::max<qint64>(0, nowMs - lastPlaybackSnapshotAtMs_);
            if (playbackSnapshotAgeMs >= kStalePlaybackWarnMs_) {
                ++stalePlaybackTickCount_;
                if ((nowMs - lastPlaybackHealthLogAtMs_) >= kPlaybackHealthLogThrottleMs_) {
                    lastPlaybackHealthLogAtMs_ = nowMs;
                    debuglog::warn("PlaybackHealth", QString("Stale playback snapshot age=%1ms misses=%2 staleTicks=%3 source=%4 track=%5")
                        .arg(playbackSnapshotAgeMs)
                        .arg(consecutivePlaybackMisses_)
                        .arg(stalePlaybackTickCount_)
                        .arg(playback->source)
                        .arg(playback->trackName));
                }
            } else {
                stalePlaybackTickCount_ = 0;
            }
        }

        mainWindow_->updateProgress(displayProgressMs, playback->durationMs);
        mainWindow_->setFavoriteState(currentTrackLiked_, playback->source == "spotify");
        updateMiniPlayer(playback.value(), sourceName);

        const bool volumeSupported = (sourceId == "spotify" || sourceId == "sonos");
        mainWindow_->setVolumeControlState(playback->volumePercent, volumeSupported);

        const bool shuffleSupported = (sourceId == "spotify");
        if (!shuffleSupported) {
            currentShuffleSource_.clear();
            currentShuffleEnabled_ = std::nullopt;
            mainWindow_->setShuffleState(false, false);
        } else {
            if (currentShuffleSource_ != sourceId || !currentShuffleEnabled_.has_value()) {
                currentShuffleEnabled_ = client_.getShuffleState();
                currentShuffleSource_ = sourceId;
            }
            mainWindow_->setShuffleState(currentShuffleEnabled_.value_or(false), true);
        }

        const bool repeatSupported = (sourceId == "spotify");
        if (!repeatSupported) {
            currentRepeatEnabled_ = std::nullopt;
            mainWindow_->setRepeatState(false, false);
        } else {

            if (!currentRepeatEnabled_.has_value()) {
                currentRepeatEnabled_ = false;
            }
            mainWindow_->setRepeatState(currentRepeatEnabled_.value_or(false), true);
        }

        std::optional<LyricsList> lyrics;
        {
            QMutexLocker locker(&lyricsMutex_);
            lyrics = currentLyrics_;
        }

        if (lyrics.has_value()) {
            mainWindow_->displayLyrics(lyrics.value());
            mainWindow_->highlightCurrentLyric(mediabar::findCurrentLyricLine(lyrics.value(), displayProgressMs));
        } else if (playback->isPlaying) {
            mainWindow_->showNoLyrics();
        }
    } else {
        stalePlaybackTickCount_ = 0;
        currentTrackLiked_ = std::nullopt;
        currentShuffleEnabled_ = std::nullopt;
        currentShuffleSource_.clear();
        latestPlayback_ = PlaybackInfo{};
        latestSourceName_.clear();
        mainWindow_->setFavoriteState(std::nullopt, false);
        mainWindow_->setShuffleState(false, false);
        mainWindow_->setVolumeControlState(-1, false);
        mainWindow_->showWaiting();
        if (miniPlayerWindow_) {
            miniPlayerWindow_->setTrackText("No track", "Waiting for playback...");
            miniPlayerWindow_->setSourceText("Unknown");
            miniPlayerWindow_->setPlaying(false);
            miniPlayerWindow_->setAlbumArtUrl(QString());
            miniPlayerWindow_->setLikedState(std::nullopt, false);
        }
    }
}
