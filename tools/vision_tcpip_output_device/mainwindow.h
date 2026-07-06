#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "vision_virtual_client.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onConnectClicked();
    void onSendTriggerClicked();
    void onClearLogClicked();

    void onMainConnectedChanged(bool connected);
    void onHbConnectedChanged(bool connected);
    void onMainPayloadReceived(const QByteArray &payload);
    void onHbAckSent(quint16 ackCount);
    void onErrorOccurred(const QString &msg);

private:
    void updateStatusLabels();
    void appendLog(const QString &line);

    Ui::MainWindow *ui;
    VisionTcpipVirtualClient *m_client;
};

#endif // MAINWINDOW_H
