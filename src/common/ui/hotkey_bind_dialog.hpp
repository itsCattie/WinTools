#pragma once

#include "common/hotkey/hotkey_engine.hpp"
#include "common/themes/window_colour.hpp"

#include <optional>
#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;

namespace wintools::ui {

class HotkeyBindDialog : public QDialog {
    Q_OBJECT

public:

    explicit HotkeyBindDialog(const QString&                               module,
                               const QString&                               actionId,
                               const QString&                               actionLabel,
                               const wintools::hotkeys::HotkeyBinding*      existing,
                               const wintools::themes::ThemePalette&        palette,
                               QWidget*                                     parent = nullptr);

    std::optional<wintools::hotkeys::HotkeyBinding> capturedBinding() const;

    bool cleared() const;

    static bool qtKeyToBinding(Qt::Key                key,
                                Qt::KeyboardModifiers  mods,
                                QString&               outKeyString,
                                QList<QString>&        outModifiers);

    static QString actionIdToLabel(const QString& actionId);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void applyCapture(const QString& keyString, const QList<QString>& mods);
    void applyTheme(const wintools::themes::ThemePalette& palette);

    QString  m_module;
    QString  m_actionId;

    QLabel*      m_instructLabel;
    QLabel*      m_captureLabel;
    QPushButton* m_okButton;
    QPushButton* m_clearButton;
    QPushButton* m_cancelButton;

    std::optional<wintools::hotkeys::HotkeyBinding> m_result;
    bool m_cleared = false;
};

}
