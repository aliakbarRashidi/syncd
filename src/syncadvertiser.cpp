/*
 * Copyright (C) 2010-2011 Robin Burchell
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Qt
#include <QCoreApplication>
#include <QNetworkInterface>
#include <QStringList>

// Saesu
#include <sglobal.h>
#include <bonjourservicebrowser.h>
#include <bonjourserviceresolver.h>
#include <bonjourserviceregister.h>

// Us
#include "syncadvertiser.h"
#include "syncmanagersynchroniser.h"

SyncAdvertiser::SyncAdvertiser(QObject *parent)
    : QObject(parent)
    , mPeerAdvertiser("saesu://peer-model")
{
    connect(&mServer, SIGNAL(newConnection()), SLOT(onNewConnection()));

    int portNo;

    forever {
        portNo = qrand() % ((65535 + 1) - 49152) + 49152; // ephemeral port range

        if (!mServer.listen(QHostAddress::Any, portNo)) {
            if (mServer.serverError() == QAbstractSocket::AddressInUseError)
                continue;

            sWarning() << "Couldn't bind TCP; error: " << mServer.errorString();
            exit(1);
        }

        sDebug() << "Bound to port " << portNo;
        break;
    }

    BonjourServiceRegister *bonjourRegister = new BonjourServiceRegister(this);
    bonjourRegister->registerService(BonjourRecord(QString("Saesu Synchronisation on %1").arg(QHostInfo::localHostName()),
                                            QLatin1String("_saesu._tcp"),
                                            QString()), portNo);

    BonjourServiceBrowser *bonjourBrowser = new BonjourServiceBrowser(this);
    connect(bonjourBrowser, SIGNAL(currentBonjourRecordsChanged(const QList<BonjourRecord> &)),
            this, SLOT(updateRecords(const QList<BonjourRecord> &)));
    bonjourBrowser->browseForServiceType(QLatin1String("_saesu._tcp"));

    mBonjourResolver = new BonjourServiceResolver(this);
    connect(mBonjourResolver, SIGNAL(bonjourRecordResolved(const QHostInfo &, int)),
            this, SLOT(connectToServer(const QHostInfo &, int)));
}

void SyncAdvertiser::updateRecords(const QList<BonjourRecord> &list)
{
    // TODO: optimisation here would be to not drop any connections
    // we should only open connections to *new* services
    foreach (SyncManagerSynchroniser *syncer, mSyncers) {
        syncer->disconnectFromHost();
        syncer->deleteLater();
    }

    QStringList peerNames;

    foreach (const BonjourRecord &record, list) {
        mBonjourResolver->resolveBonjourRecord(record);
        peerNames.append(record.serviceName);
    }

    sDebug() << "Got peers: " << peerNames;

    QByteArray data;
    QDataStream ds(&data, QIODevice::WriteOnly);
    ds << peerNames;

    mPeerAdvertiser.sendMessage("peersAvailable(QStringList)", data);
}

void SyncAdvertiser::connectToServer(const QHostInfo &address, int port)
{
    QNetworkInterface iface;

    foreach (const QHostAddress &addr, iface.allAddresses()) {
        foreach (const QHostAddress &remoteAddr, address.addresses()) {
            if (remoteAddr == addr) {
                return;
            }
        }
    }

    // TODO: handle this properly by queueing addresses and trying each of them until we find one that works
    // TODO: this breaks ipv6 until we do
    foreach (const QHostAddress &remoteAddr, address.addresses()) {
        if (remoteAddr.protocol() == QAbstractSocket::IPv4Protocol) {
            sDebug() << remoteAddr << " said hello, connecting back...";
            SyncManagerSynchroniser *syncSocket = new SyncManagerSynchroniser(this);
            connect(syncSocket, SIGNAL(destroyed()), SLOT(onDisconnected()));
            syncSocket->connectToHost(remoteAddr, port);
            mSyncers.append(syncSocket);
            break;
        }
    }
}

void SyncAdvertiser::onNewConnection()
{
    while (mServer.hasPendingConnections()) {
        QTcpSocket *socket = mServer.nextPendingConnection();
        sDebug() << "Got a new connection from " << socket->peerAddress();
        SyncManagerSynchroniser *syncSocket = new SyncManagerSynchroniser(this, socket);
        connect(syncSocket, SIGNAL(destroyed()), SLOT(onDisconnected()));
        mSyncers.append(syncSocket);
    }
}

void SyncAdvertiser::onDisconnected()
{
    SyncManagerSynchroniser *mgr = static_cast<SyncManagerSynchroniser*>(sender());
    mSyncers.removeAll(mgr);
}
