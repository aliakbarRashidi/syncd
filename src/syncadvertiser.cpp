// Qt
#include <QCoreApplication>
#include <QNetworkInterface>

// Saesu
#include "sglobal.h"

// Us
#include "syncadvertiser.h"
#include "syncmanagersynchroniser.h"
#include "bonjourservicebrowser.h"
#include "bonjourserviceresolver.h"

SyncAdvertiser::SyncAdvertiser(QObject *parent)
    : QObject(parent)
{
    BonjourServiceBrowser *bonjourBrowser = new BonjourServiceBrowser(this);

    connect(bonjourBrowser, SIGNAL(currentBonjourRecordsChanged(const QList<BonjourRecord> &)),
            this, SLOT(updateRecords(const QList<BonjourRecord> &)));
    bonjourBrowser->browseForServiceType(QLatin1String("_saesu._tcp"));

    mBonjourResolver = new BonjourServiceResolver(this);
    connect(mBonjourResolver, SIGNAL(bonjourRecordResolved(const QHostInfo &, int)),
            this, SLOT(connectToServer(const QHostInfo &, int)));

    if (!mServer.listen(QHostAddress::Any, 1337)) {
        qDebug("Couldn't bind TCP; presuming another instance running");
        exit(1);
    }

    // and open for business
    connect(&mServer, SIGNAL(newConnection()), SLOT(onNewConnection()));
}

void SyncAdvertiser::updateRecords(const QList<BonjourRecord> &list)
{
    // TODO: optimisation here would be to not drop any connections
    // we should only open connections to *new* services
    foreach (SyncManagerSynchroniser *syncer, mSyncers) {
        syncer->disconnectFromHost();
        syncer->deleteLater();
    }

    foreach (const BonjourRecord &record, list) {
        mBonjourResolver->resolveBonjourRecord(record);
    }
}

void SyncAdvertiser::connectToServer(const QHostInfo &address, int port)
{
    QNetworkInterface iface;

    foreach (const QHostAddress &addr, iface.allAddresses()) {
        foreach (const QHostAddress &remoteAddr, address.addresses()) {
            if (remoteAddr == addr) {
                sDebug() << "Dropping broadcast from myself (" << remoteAddr << ")";
                return;
            }
        }
    }

    // TODO: handle this properly by queueing addresses and trying each of them until we find one that works
    // TODO: this breaks ipv6 until we do
    foreach (const QHostAddress &remoteAddr, address.addresses()) {
        if (remoteAddr.protocol() == QAbstractSocket::IPv4Protocol) {
            qDebug() << address.addresses() << " said hello, connecting back...";
            SyncManagerSynchroniser *syncSocket = new SyncManagerSynchroniser(this);
            syncSocket->connectToHost(remoteAddr);
            mSyncers.append(syncSocket);
        }
    }
}

void SyncAdvertiser::onNewConnection()
{
    sDebug() << "Got a new connection!";
    while (mServer.hasPendingConnections()) {
        QTcpSocket *socket = mServer.nextPendingConnection();
        SyncManagerSynchroniser *syncSocket = new SyncManagerSynchroniser(this, socket);
        mSyncers.append(syncSocket);
    }
}
