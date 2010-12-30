// Qt
#include <QCoreApplication>

// Saesu
#include "sglobal.h"

// Us
#include "saesucloudstorageadvertiser.h"
#include "saesucloudstoragesynchroniser.h"

SaesuCloudStorageAdvertiser::SaesuCloudStorageAdvertiser(QObject *parent)
    : QObject(parent)
{
    connect(&mBroadcaster, SIGNAL(readyRead()), SLOT(onReadyRead()));
    connect(&mServer, SIGNAL(newConnection()), SLOT(onNewConnection()));

    if (!mBroadcaster.bind(QHostAddress::Any, 1337)) {
        qDebug("Couldn't bind UDP; presuming another instance running");
        exit(1);
    }

    if (!mServer.listen(QHostAddress::Any, 1337)) {
        qDebug("Couldn't bind TCP; presuming another instance running");
        exit(1);
    }
}

void SaesuCloudStorageAdvertiser::onReadyRead()
{
    QHostAddress senderAddress;
    quint16      senderPort;
    QByteArray   datagram(5, 0);

    int returnedSize = mBroadcaster.readDatagram(datagram.data(), datagram.size(),
                                                 &senderAddress, &senderPort);
    if (returnedSize == 5) {
        // Great joy
        qDebug() << senderAddress << " said " << datagram << ", connecting back...";
        SaesuCloudStorageSynchroniser *syncSocket = new SaesuCloudStorageSynchroniser(this);
        syncSocket->connectToHost(senderAddress);
    }
}

void SaesuCloudStorageAdvertiser::onNewConnection()
{
    sDebug() << "Got a new connection!";
    while (mServer.hasPendingConnections()) {
        QTcpSocket *socket = mServer.nextPendingConnection();
        SaesuCloudStorageSynchroniser *syncSocket = new SaesuCloudStorageSynchroniser(this, socket);
    }
}
