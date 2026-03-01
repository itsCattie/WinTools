#pragma once

#include <QObject>
#include <QSystemTrayIcon>
#include <vector>

class QMenu;

namespace wintools::modules {
struct ModuleEntry;
}

namespace wintools::ui {

class TrayManager : public QObject {
    Q_OBJECT
public:
    explicit TrayManager(QObject* parent = nullptr);

    void updateModules(const std::vector<wintools::modules::ModuleEntry>& modules);

    void setTooltip(const QString& text);
    void showMessage(const QString& title, const QString& message,
                     QSystemTrayIcon::MessageIcon icon = QSystemTrayIcon::Information,
                     int msecs = 5000);

    void show();

signals:
    void openMainWindowRequested();
    void moduleActionRequested(const QString& module, const QString& action);
    void quitRequested();

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void rebuildMenu(const std::vector<wintools::modules::ModuleEntry>* modules = nullptr);

    QSystemTrayIcon* m_tray;
    QMenu*           m_menu;
};

}
