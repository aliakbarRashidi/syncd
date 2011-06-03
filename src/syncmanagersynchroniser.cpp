/*
 * Copyright (C) 2011 Robin Burchell <viroteck@viroteck.net>
 *
 * This software, and all associated material(s), including but not limited
 * to documentation are protected by copyright. All rights reserved.
 * Copying, including reproducing, storing, adapting, translating, or
 * reverse-engineering any or all of this material requires prior written
 * consent. This material also contains confidential information which
 * may not be disclosed in any form without prior written consent.
 */

// Qt
#include <QTcpSocket>
#include <QHostAddress>
#include <QDir>
#include <QCoreApplication>
#include <QDesktopServices>

// Saesu
#include <sobject.h>
#include <sglobal.h> // XXX: move to sobject.h
#include <sobjectsaverequest.h>

// Us
#include "syncmanager.h"
#include "syncmanagersynchroniser.h"

SyncManagerSynchroniser::SyncManagerSynchroniser(QObject *parent, QTcpSocket *socket)
    : QObject(parent)
    , mBytesExpected(0)
{
    if (socket) {
        mIsOutgoing = false;
        mSocket = socket;
        mSocket->setParent(this); // make sure we take over
        startSync(); // already connected, so send introduction
    } else {
        mSocket = new QTcpSocket(this);
        mIsOutgoing = true;
    }

    connect(mSocket, SIGNAL(connected()), SLOT(startSync()));
    connect(mSocket, SIGNAL(readyRead()), SLOT(onReadyRead()));
    connect(mSocket, SIGNAL(error(QAbstractSocket::SocketError)), SLOT(onError(QAbstractSocket::SocketError)));
    connect(mSocket, SIGNAL(disconnected()), SLOT(onDisconnected()));
}

bool SyncManagerSynchroniser::isOutgoing() const
{
    return mIsOutgoing;
}

void SyncManagerSynchroniser::sendCommand(quint8 token, const QByteArray &data)
{
    // TODO: endianness?
    quint32 length = (quint32)data.length() + 1;
    mSocket->write(reinterpret_cast<char *>(&length), sizeof(quint32)); // TODO: endianness
    mSocket->write(reinterpret_cast<char *>(&token), sizeof(quint8));
    mSocket->write(data);
}

void SyncManagerSynchroniser::startSync()
{
    // as both sides advertise their presence, we end up with two connections: one outgoing from each side
    // we can't stop those connections, because we don't know which interface the OS will route the connection
    // from, so this is the only place we can know IPs.
    //
    // however, we also can't deal (in the sense of efficiency) with multiple connections performing
    // the exact same synchronisation, so we need to force the peers to agree on a single connection.
    // the easiest way to do this is to assume that the dominant connection will always be from the peer
    // with the *LOWER* IP address.
    const QHostAddress peerAddress = mSocket->peerAddress();
    const QHostAddress localAddress = mSocket->localAddress();

    sDebug() << (void*)this << "Got connected from " << peerAddress.toString() << " to " << localAddress.toString() << " direction is " << (isOutgoing() ? "outgoing" : "incoming");

    if (QString::compare(peerAddress.toString(), localAddress.toString()) < 0 && !isOutgoing()) {
        sDebug() << (void*)this << "Dropping redundant connection from " << peerAddress << " to " << localAddress;
        mSocket->disconnectFromHost();
        return;
    }

     // TODO: listen for cloud add/remove
    QString databasePath;
    QCoreApplication *a = QCoreApplication::instance();

    QString orgName = a->organizationName();
    QString appName = a->applicationName();

    a->setOrganizationName(QLatin1String("saesu"));
    a->setApplicationName(QLatin1String("clouds"));

    databasePath = QDesktopServices::storageLocation(QDesktopServices::DataLocation);
    QDir databaseDir(databasePath);

    QStringList databases = databaseDir.entryList(QDir::Files);

    {
        // send current time
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << (qint64)QDateTime::currentMSecsSinceEpoch(); // TODO: endianness

        sendCommand(CurrentTimeCommand, data);
    }

    foreach (const QString &database, databases) {
        connect(SyncManager::instance(database),
                SIGNAL(objectsAddedOrUpdated(QString,QList<SObject>)),
                SLOT(sendObjectList(QString,QList<SObject>)),
                Qt::UniqueConnection);
        connect(SyncManager::instance(database),
                SIGNAL(objectsDeleted(QString,QList<SObjectLocalId>)),
                SLOT(sendDeleteList(QString,QList<SObjectLocalId>)),
                Qt::UniqueConnection);
        sendDeleteList(database, SyncManager::instance(database)->deleteList());
        sendObjectList(database, SyncManager::instance(database)->objects());
    }
}

void SyncManagerSynchroniser::sendDeleteList(const QString &managerName, const QList<SObjectLocalId> &ids)
{
    sDebug() << (void*)this << "Sending delete list of " << ids.count() << " items";
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << managerName;
    stream << (quint32)ids.count();

    foreach (const SObjectLocalId &localId, ids) {
        stream << localId;
    }
    
    sendCommand(DeleteListCommand, data);
}

void SyncManagerSynchroniser::sendObjectList(const QString &cloudName, const QList<SObject> &objects)
{
    sDebug() << (void*)this << "Sending object list of " << objects.count() << " items";

    if (!objects.count())
        return;

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << cloudName;
    stream << (quint32)objects.count();

    foreach (const SObject &obj, objects) {
        stream << obj.id().localId();
        stream << obj.hash();
        stream << obj.lastSaved();
    }

    sendCommand(ObjectListCommand, data);
}

void SyncManagerSynchroniser::onReadyRead()
{
    while (mSocket->bytesAvailable() >= sizeof(quint32)) {
        if (mBytesExpected == 0) {
            // read header
            mSocket->read(reinterpret_cast<char *>(&mBytesExpected), sizeof(mBytesExpected));
        }

        if (mSocket->bytesAvailable() < mBytesExpected)
            return;

        // read mBytesExpected bytes and process it
        QByteArray bytes = mSocket->read(mBytesExpected);
        Q_ASSERT((quint32)bytes.length() == mBytesExpected);

        processData(bytes);
        mBytesExpected = 0;
    }
}

void SyncManagerSynchroniser::processDeleteList(QDataStream &stream)
{
    QString cloudName;
    stream >> cloudName;

    // sending a message, find message

    quint32 itemCount;
    stream >> itemCount;

    sDebug() << (void*)this << "Processing a delete list of " << itemCount << " items";
    QList<SObjectLocalId> ids;

    for (quint32 i = 0; i < itemCount; ++i) {
        SObjectLocalId uuid;
        
        stream >> uuid;
        ids.append(uuid);
    }

    SyncManager::instance(cloudName)->ensureRemoved(ids);
}

void SyncManagerSynchroniser::processObjectList(QDataStream &stream)
{
    QString cloudName;
    stream >> cloudName;

    // sending a message, find message
    QHash<SObjectLocalId, SObject> objects = SyncManager::instance(cloudName)->objectHash();

    quint32 itemCount;
    stream >> itemCount;

    sDebug() << (void*)this << "Processing an object list of " << itemCount << " items";

    for (quint32 i = 0; i < itemCount; ++i) {
        SObjectLocalId uuid;
        QByteArray itemHash;
        qint64 itemTS;

        stream >> uuid;
        stream >> itemHash;
        stream >> itemTS;

        bool requestItem = false;

        QHash<SObjectLocalId, SObject>::ConstIterator cit = objects.find(uuid);
        if (cit == objects.end()) {
            sDebug() << (void*)this << "Item " << uuid << " not found; requesting";
            requestItem = true;
        } else {
            const SObject &obj = *cit;

            if (obj.hash() != itemHash ||
                obj.lastSaved() != itemTS) {
                sDebug() << (void*)this << "Modified item " << uuid << " detected, requesting";
                requestItem = true;
            }
        }

        if (requestItem) {
            QByteArray sendingData;
            QDataStream sendingStream(&sendingData, QIODevice::WriteOnly);

            sendingStream << cloudName;
            sendingStream << uuid;
            sendCommand(ObjectRequestCommand, sendingData);
        }
    }
}

void SyncManagerSynchroniser::processObjectRequest(QDataStream &stream)
{
    QString cloudName;
    stream >> cloudName;

    SObjectLocalId uuid;
    stream >> uuid;

    QHash<SObjectLocalId, SObject> objects = SyncManager::instance(cloudName)->objectHash();
    QHash<SObjectLocalId, SObject>::ConstIterator cit = objects.find(uuid);
    if (cit == objects.end()) {
        sDebug() << (void*)this << "Recieved a request for a nonexistant item! UUID: " << uuid;
        return;
    }

    sDebug() << (void*)this << "Object request for " << uuid << " recieved; sending";

    QByteArray sendingData;
    QDataStream sendingStream(&sendingData, QIODevice::WriteOnly);

    sendingStream << cloudName;
    sendingStream << uuid;
    
    // TODO: this won't correctly serialise hash/modified timestamp; we need to
    // change how we save SObject instances in the db (stop using stream operators)
    // and then change the stream operators to persist the _WHOLE_ object
    sendingStream << *(cit);
    sendCommand(ObjectReplyCommand, sendingData);
}

void SyncManagerSynchroniser::processObjectReply(QDataStream &stream)
{
    QString cloudName;
    stream >> cloudName;

    SObjectLocalId uuid;
    SObject remoteItem;
    stream >> uuid;
    stream >> remoteItem;
    
    if (SyncManager::instance(cloudName)->isRemoved(uuid)) {
        sDebug() << (void*)this << "Ignoring deleted UUID " << uuid;
        return;
    }

    QHash<SObjectLocalId, SObject> objects = SyncManager::instance(cloudName)->objectHash();
    QHash<SObjectLocalId, SObject>::ConstIterator cit = objects.find(uuid);
    bool saveItem = false;

    if (cit == objects.end()) {
        sDebug() << (void*)this << "Inserting an item I don't have";
        saveItem = true;
    } else {
        const SObject &localItem = *cit;
        
        sDebug() << (void*)this << "Existing object " << uuid;
        sDebug() << (void*)this << "   LOCAL TS: " << localItem.lastSaved();
        sDebug() << (void*)this << "   REMOTE TS: " << remoteItem.lastSaved();

        // find out which is the newer item
        if (localItem.lastSaved() > remoteItem.lastSaved()) {
            // ours is newer, ignore theirs
            sDebug() << (void*)this << "For modified item " << uuid << ", using ours on TS";
        } else if (localItem.lastSaved() == remoteItem.lastSaved()) {
            // identical, resort to alphabetically superior SHA to force a compromise
            sDebug() << (void*)this << "TS equal; hash comparison needed";
            sDebug() << (void*)this << "  LOCAL HASH: " << localItem.hash();
            sDebug() << (void*)this << "  REMOTE HASH: " << remoteItem.hash();
            if (localItem.hash() > remoteItem.hash()) {
                // ours is alphabetically superior, ignore theirs
                sDebug() << (void*)this << "For modified item " << uuid << " using ours on hash";
            } else if (localItem.hash() == remoteItem.hash()) {
                // in the case of two connected clients, A and B,
                // A may add an item, send a change notification (via object list) to B
                // B will request the item, add it, which will trigger a
                // local create notification on B, which will then result in a remote notification
                // to A.
                //
                // This is actually desired (if slightly annoying) behaviour, because it allows
                // items to propegate around the mesh more fully, as an instant notification,
                // (think of situations where B actually has more than one other syncd connected)
                //
                // Of course, we could get here in the case that two items both hash to the same
                // sha1, but let's just hope that never ever happens.
                //
                // TODO: we could be neurotic, and check data A == data B
                // (and if not, use the alphabetically superior like other parts of collision resolution)
            } else if (localItem.hash() < remoteItem.hash()) {
                // take theirs
                sDebug() << (void*)this << "For modified item " << uuid << " using theirs on hash";
                saveItem = true;
            }
        } else if (localItem.lastSaved() < remoteItem.lastSaved()) {
            // theirs wins
            sDebug() << (void*)this << "For modified item " << uuid << " using theirs on TS";
            saveItem = true;
        }
    }

    if (saveItem) {
        // TODO: batch saving
        SObjectSaveRequest *saveRequest = new SObjectSaveRequest;
        connect(saveRequest, SIGNAL(finished()), saveRequest, SLOT(deleteLater()));
        saveRequest->add(remoteItem);
        saveRequest->setSaveHint(SObjectSaveRequest::ObjectFromSync);
        saveRequest->start(SyncManager::instance(cloudName)->manager());
    }
}

void SyncManagerSynchroniser::processCurrentTime(QDataStream &stream)
{
    qint64 currentTime;
    stream >> currentTime;

    qint64 delta = currentTime - QDateTime::currentMSecsSinceEpoch();
    if (delta < 0)
        delta *= -1;

    if (delta > 3000) {
        sWarning() << "Synchronisation with " << mSocket->peerAddress() << " aborted! Time delta is too high: " << delta;
        mSocket->disconnectFromHost();
        return;
    } else if (delta > 0) {
        sDebug() << (void*)this << "Synchronisation delta with " << mSocket->peerAddress() << " is " << delta;
    }
}

void SyncManagerSynchroniser::processData(const QByteArray &bytes)
{
    QDataStream stream(bytes);

    quint8 command;
    stream >> command;

    switch (command) {
        case CurrentTimeCommand:
            processCurrentTime(stream);
            break;
        case DeleteListCommand:
            processDeleteList(stream);
            break;
        case ObjectListCommand:
            processObjectList(stream);
            break;
        case ObjectRequestCommand:
            processObjectRequest(stream);
            break;
        case ObjectReplyCommand:
            processObjectReply(stream);
            break;
        default:
            break;
    }
}

void SyncManagerSynchroniser::connectToHost(const QHostAddress &address, int port)
{
    sDebug() << (void*)this << "Connecting to " << address;
    mSocket->connectToHost(address, port);
}

void SyncManagerSynchroniser::onError(QAbstractSocket::SocketError error)
{
    sDebug() << (void*)this << "Had an error: " << error;

    if (error == QAbstractSocket::RemoteHostClosedError && isOutgoing()) {
        // try reconnect
        mSocket->connectToHost(mSocket->peerAddress(), mSocket->peerPort());
    } else {
        deleteLater();
    }
}

void SyncManagerSynchroniser::onDisconnected()
{
    sDebug() << (void*)this << "Connection closed";
    deleteLater();
}

