#ifndef SAESUCLOUDSTORAGEADVERTISER_H
#define SAESUCLOUDSTORAGEADVERTISER_H

#include <QObject>
#include <QUdpSocket>
#include <QTcpServer>

class SaesuCloudStorageAdvertiser : public QObject
{
    Q_OBJECT
public:
    explicit SaesuCloudStorageAdvertiser(QObject *parent = 0);

private slots:
    void onReadyRead();
    void onNewConnection();

private:
    QUdpSocket mBroadcaster;
    QTcpServer mServer;
};

#endif // SAESUCLOUDSTORAGEADVERTISER_H
