#include "common/ui/launch_page.hpp"
#include "logger/logger.hpp"

#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>

// WinTools: main manages feature behavior.

namespace {
constexpr auto kSingleInstanceServerName = "WinToolsNative.SingleInstance";
constexpr auto kShowCommand = "show";
#ifdef WINTOOLS_APP_VERSION
constexpr auto kAppVersion = WINTOOLS_APP_VERSION;
#else
constexpr auto kAppVersion = "0.0.0";
#endif
}

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("WinTools");
    QCoreApplication::setApplicationName("WinToolsNative");
    QCoreApplication::setApplicationVersion(QString::fromUtf8(kAppVersion));

    QLocalSocket existingInstanceSocket;
    existingInstanceSocket.connectToServer(kSingleInstanceServerName);
    if (existingInstanceSocket.waitForConnected(250)) {
        existingInstanceSocket.write(kShowCommand);
        existingInstanceSocket.flush();
        existingInstanceSocket.waitForBytesWritten(250);
        wintools::logger::Logger::log("Program", wintools::logger::Severity::Pass,
                                      "Second launch redirected to existing WinTools instance.");
        return 0;
    }

    QLocalServer::removeServer(kSingleInstanceServerName);
    QLocalServer singleInstanceServer;
    if (!singleInstanceServer.listen(kSingleInstanceServerName)) {
        wintools::logger::Logger::log("Program", wintools::logger::Severity::Warning,
                                      "Single-instance server listen failed.",
                                      singleInstanceServer.errorString());
    }

    wintools::logger::Logger::log("Program", wintools::logger::Severity::Pass, "Application starting.");

    wintools::ui::LaunchPage window;
    window.show();

    QObject::connect(&singleInstanceServer, &QLocalServer::newConnection,
                     &window, [&singleInstanceServer, &window]() {
        while (QLocalSocket* socket = singleInstanceServer.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::readyRead, socket,
                             [&window, socket]() {
                const QByteArray msg = socket->readAll().trimmed();
                if (msg == kShowCommand) {
                    if (window.isMinimized()) {
                        window.showNormal();
                    }
                    window.show();
                    window.raise();
                    window.activateWindow();
                }
            });
            QObject::connect(socket, &QLocalSocket::disconnected,
                             socket, &QLocalSocket::deleteLater);
        }
    });

    const int exitCode = app.exec();
    wintools::logger::Logger::log("Program", wintools::logger::Severity::Pass, "Application closed.");
    return exitCode;
}
