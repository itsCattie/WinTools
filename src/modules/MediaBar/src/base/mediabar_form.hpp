#pragma once

// MediaBar: mediabar form manages feature behavior.

#include <QDialog>

class QLabel;

namespace wintools::modules {

class MediaBarForm : public QDialog {
    Q_OBJECT

public:
    explicit MediaBarForm(QWidget* parent = nullptr);

private slots:
    void tryLaunchAndCloseOnSuccess();
    void runBuildScript();

private:
    QString findMediaBarExecutable() const;
    QString findRepositoryRoot() const;
    static bool isDeploymentReady(const QString& executablePath);

    QLabel* m_statusLabel;
    QString m_executableName;
};

}
