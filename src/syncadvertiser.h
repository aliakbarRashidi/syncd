#ifndef SYNCADVERTISER_H
#define SYNCADVERTISER_H

#include <QObject>
#include <QUdpSocket>
#include <QTcpServer>

class SyncAdvertiser : public QObject
{
    Q_OBJECT
public:
    explicit SyncAdvertiser(QObject *parent = 0);

private slots:
    void onReadyRead();
    void onNewConnection();

private:
    QUdpSocket mBroadcaster;
    QTcpServer mServer;
};

#endif // SYNCADVERTISER_H
