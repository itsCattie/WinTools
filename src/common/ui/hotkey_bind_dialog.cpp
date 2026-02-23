#include "common/ui/hotkey_bind_dialog.hpp"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

// WinTools: hotkey bind dialog manages UI behavior and presentation.

namespace wintools::ui {

HotkeyBindDialog::HotkeyBindDialog(const QString&                          module,
                                   const QString&                          actionId,
                                   const QString&                          actionLabel,
                                   const wintools::hotkeys::HotkeyBinding* existing,
                                   const wintools::themes::ThemePalette&   palette,
                                   QWidget*                                parent)
    : QDialog(parent),
      m_module(module),
      m_actionId(actionId),
      m_instructLabel(new QLabel(this)),
      m_captureLabel(new QLabel(this)),
      m_okButton(new QPushButton("OK", this)),
      m_clearButton(new QPushButton("Clear", this)),
      m_cancelButton(new QPushButton("Cancel", this)) {
    setWindowTitle(QString("Set Hotkey – %1 / %2").arg(module, actionLabel));
    setMinimumWidth(360);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    auto* root    = new QVBoxLayout(this);
    auto* btnRow  = new QHBoxLayout();

    m_instructLabel->setText(
        QString("<b>%1</b> &nbsp;›&nbsp; %2<br/>"
                "<span style='font-size:9pt;'>Click here then press your key combination.</span>")
            .arg(module, actionLabel));
    m_instructLabel->setTextFormat(Qt::RichText);
    m_instructLabel->setAlignment(Qt::AlignCenter);

    m_captureLabel->setAlignment(Qt::AlignCenter);
    m_captureLabel->setMinimumHeight(52);
    m_captureLabel->setFont(QFont("Segoe UI", 14, QFont::Bold));

    if (existing && existing->enabled) {
        applyCapture(existing->keyString, existing->modifiers);
    } else {
        m_captureLabel->setText("— press a key —");
        m_okButton->setEnabled(false);
    }

    m_okButton->setMinimumWidth(80);
    m_clearButton->setMinimumWidth(80);
    m_cancelButton->setMinimumWidth(80);

    btnRow->addWidget(m_clearButton);
    btnRow->addStretch();
    btnRow->addWidget(m_cancelButton);
    btnRow->addWidget(m_okButton);

    root->addWidget(m_instructLabel);
    root->addSpacing(12);
    root->addWidget(m_captureLabel);
    root->addSpacing(16);
    root->addLayout(btnRow);

    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_clearButton, &QPushButton::clicked, this, [this]() {
        m_cleared  = true;
        m_result   = std::nullopt;
        accept();
    });

    applyTheme(palette);
}

std::optional<wintools::hotkeys::HotkeyBinding> HotkeyBindDialog::capturedBinding() const {
    return m_result;
}

bool HotkeyBindDialog::cleared() const {
    return m_cleared;
}

void HotkeyBindDialog::keyPressEvent(QKeyEvent* event) {
    const auto qtKey = static_cast<Qt::Key>(event->key());

    if (qtKey == Qt::Key_Control || qtKey == Qt::Key_Alt   ||
        qtKey == Qt::Key_Shift   || qtKey == Qt::Key_Meta  ||
        qtKey == Qt::Key_unknown) {
        return;
    }

    QString        keyString;
    QList<QString> modifiers;
    if (!qtKeyToBinding(qtKey, event->modifiers(), keyString, modifiers)) {
        return;
    }

    applyCapture(keyString, modifiers);
    event->accept();
}

void HotkeyBindDialog::applyCapture(const QString& keyString,
                                     const QList<QString>& mods) {
    wintools::hotkeys::HotkeyBinding b;
    b.action.id     = m_actionId;
    b.action.module = m_module;
    b.action.action = m_actionId;
    b.keyString     = keyString;
    b.modifiers     = mods;
    b.enabled       = true;
    m_result        = b;

    const QString disp = wintools::hotkeys::HotkeyEngine::displayString(b);
    m_captureLabel->setText(disp);
    m_okButton->setEnabled(true);
}

void HotkeyBindDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);
    setFocus();
    activateWindow();
}

void HotkeyBindDialog::applyTheme(const wintools::themes::ThemePalette& palette) {
    setStyleSheet(QString(
        "QDialog{background:%1;color:%2;}"
        "QLabel{color:%2;}"
        "QPushButton{border:1px solid %3;border-radius:6px;padding:6px 14px;"
        "background:%4;color:%2;}"
        "QPushButton:hover{background:%3;}"
        "QPushButton:disabled{color:%5;}"
    ).arg(
        palette.windowBackground.name(),
        palette.foreground.name(),
        palette.cardBorder.name(),
        palette.cardBackground.name(),
        palette.mutedForeground.name()));

    m_captureLabel->setStyleSheet(QString(
        "QLabel{background:%1;border:1px solid %2;border-radius:8px;"
        "padding:8px;color:%3;}")
        .arg(palette.cardBackground.name(),
             palette.cardBorder.name(),
             palette.foreground.name()));
}

bool HotkeyBindDialog::qtKeyToBinding(Qt::Key               qtKey,
                                       Qt::KeyboardModifiers  mods,
                                       QString&               outKeyString,
                                       QList<QString>&        outModifiers) {

    if (qtKey >= Qt::Key_F1 && qtKey <= Qt::Key_F24) {
        outKeyString = QString("F%1").arg(qtKey - Qt::Key_F1 + 1);
    }

    else if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        outKeyString = QString(QChar(static_cast<int>(qtKey)));
    }

    else if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        outKeyString = QString(QChar(static_cast<int>(qtKey)));
    }
    else {

        static const QHash<int, QString> named = {
            {Qt::Key_Space,        "Space"},
            {Qt::Key_Return,       "Enter"},
            {Qt::Key_Enter,        "Enter"},
            {Qt::Key_Tab,          "Tab"},
            {Qt::Key_Escape,       "Escape"},
            {Qt::Key_Backspace,    "Backspace"},
            {Qt::Key_Delete,       "Delete"},
            {Qt::Key_Insert,       "Insert"},
            {Qt::Key_Home,         "Home"},
            {Qt::Key_End,          "End"},
            {Qt::Key_PageUp,       "PageUp"},
            {Qt::Key_PageDown,     "PageDown"},
            {Qt::Key_Up,           "Up"},
            {Qt::Key_Down,         "Down"},
            {Qt::Key_Left,         "Left"},
            {Qt::Key_Right,        "Right"},
            {Qt::Key_Pause,        "Pause"},
            {Qt::Key_Print,        "PrintScreen"},
            {Qt::Key_ScrollLock,   "ScrollLock"},
            {Qt::Key_NumLock,      "NumLock"},
            {Qt::Key_CapsLock,     "CapsLock"},
            {Qt::Key_Semicolon,    "Semicolon"},
            {Qt::Key_Equal,        "Equals"},
            {Qt::Key_Comma,        "Comma"},
            {Qt::Key_Minus,        "Minus"},
            {Qt::Key_Period,       "Period"},
            {Qt::Key_Slash,        "Slash"},
            {Qt::Key_BracketLeft,  "LBracket"},
            {Qt::Key_Backslash,    "Backslash"},
            {Qt::Key_BracketRight, "RBracket"},
            {Qt::Key_QuoteLeft,    "Tilde"},
            {Qt::Key_Apostrophe,   "Quote"},
            {Qt::Key_MediaPlay,    "Media_Play"},
            {Qt::Key_MediaNext,    "Media_Next"},
            {Qt::Key_MediaPrevious,"Media_Prev"},
            {Qt::Key_VolumeUp,     "Volume_Up"},
            {Qt::Key_VolumeDown,   "Volume_Down"},
            {Qt::Key_VolumeMute,   "Volume_Mute"},
        };

        if (!named.contains(static_cast<int>(qtKey))) return false;
        outKeyString = named.value(static_cast<int>(qtKey));
    }

    outModifiers.clear();
    if (mods & Qt::ControlModifier) outModifiers << "Ctrl";
    if (mods & Qt::AltModifier)     outModifiers << "Alt";
    if (mods & Qt::ShiftModifier)   outModifiers << "Shift";
    if (mods & Qt::MetaModifier)    outModifiers << "Win";

    return true;
}

QString HotkeyBindDialog::actionIdToLabel(const QString& actionId) {

    static const QHash<QString, QString> labels = {
        {"toggle",    "Toggle"},
        {"show_mini", "Show Mini"},
        {"show_full", "Show Full"},
        {"open_main", "Open Main Window"},
    };
    if (labels.contains(actionId)) return labels.value(actionId);

    QStringList parts = actionId.split('_');
    for (QString& p : parts) {
        if (!p.isEmpty()) p[0] = p[0].toUpper();
    }
    return parts.join(' ');
}

}
