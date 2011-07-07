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
#include <QtEndian>
#include <QCryptographicHash>

// Saesu
#include <sobject.h>
#include <sglobal.h> // XXX: move to sobject.h
#include <sobjectsaverequest.h>

// Us
#include "syncmanager.h"
#include "syncmanagersynchroniser.h"

static const qint64 blockSize = 4096;

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
    quint32 length = qToBigEndian<quint32>((quint32)data.length() + 1);
    mSocket->write(reinterpret_cast<char *>(&length), sizeof(quint32));
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

    sDebug() << "Opening file";
    QCryptographicHash fileHash(QCryptographicHash::Sha1);
    char buf[blockSize + 1]; // + 1 to not overwrite the block
    *buf = 0;
    bool error = false;
    QList<QByteArray> blockHashes;
    QFile f("music.mp3");

    f.open(QIODevice::ReadOnly);

    while (!f.atEnd()) {
        QCryptographicHash blockHash(QCryptographicHash::Sha1);
        qint64 readBlockSize = f.read(buf, blockSize);

        if (readBlockSize == -1) {
            sDebug() << "Error!";
            error = true;
            break;
        } else if (readBlockSize < blockSize) {
            sDebug() << "Didn't read a full block, near EOF?";
            sDebug() << "Read a block of " << readBlockSize << " bytes";
        }

        blockHash.addData(buf, readBlockSize);
        fileHash.addData(buf, readBlockSize);
        blockHashes.append(blockHash.result());
    }

    // TODO: check for empty file
    if (!error) {
        sDebug() << "No error! :)";
        sDebug() << "File contains " << blockHashes.count() << " hashes";
        sDebug() << "File hash is " << fileHash.result().toHex();

        {
            // send file overview
            QByteArray data;
            QDataStream stream(&data, QIODevice::WriteOnly);
            stream << QString("music.mp3");
            stream << (quint64)f.size();
            stream << fileHash.result();

            sendCommand(FileInfoCommand, data);
        }
    }

    sDebug() << "Finished reading file";

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
        stream << (qint64)QDateTime::currentMSecsSinceEpoch();

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
            mBytesExpected = qFromBigEndian<quint32>(mBytesExpected);
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

void SyncManagerSynchroniser::processFileInfo(QDataStream &stream)
{
    QString theirFileName;
    quint64 theirFileSize;
    QByteArray theirFileHash;

    stream >> theirFileName;
    stream >> theirFileSize;
    stream >> theirFileHash;

    sDebug() << "Got a file info for: " << theirFileName << "; file is " <<
        theirFileSize << " bytes, hash is " << theirFileHash.toHex();

    // TODO: cache hashes for blocks and files
    char buf[blockSize + 1]; // + 1 to not overwrite the block
    *buf = 0;
    QCryptographicHash fileHash(QCryptographicHash::Sha1);
    QFile f(theirFileName);
    f.open(QIODevice::ReadOnly);

    while (!f.atEnd()) {
        qint64 readBlockSize = f.read(buf, blockSize);

        if (readBlockSize == -1) {
            sDebug() << "Error!";
            break;
        } else if (readBlockSize < blockSize) {
            sDebug() << "Didn't read a full block, near EOF?";
            sDebug() << "Read a block of " << readBlockSize << " bytes";
        }

        fileHash.addData(buf, readBlockSize);
    }

    if (theirFileSize != f.size() &&
        theirFileHash != fileHash.result()) {
        sDebug() << "File differs: " << theirFileName;
        sDebug() << "   OUR SIZE: " << f.size() << "; theirs: " << theirFileSize;
        sDebug() << "   OUR HASH: " << fileHash.result().toHex() << "; theirs: " << theirFileHash;

        {
            // send hash request
            QByteArray data;
            QDataStream stream(&data, QIODevice::WriteOnly);
            stream << QString(theirFileName);

            sendCommand(FileHashRequestCommand, data);
        }
    }
}

void SyncManagerSynchroniser::processFileHashRequest(QDataStream &stream)
{
    QString theirFileName;
    stream >> theirFileName;

    sDebug() << "Recieved a hash request for " << theirFileName;
    char buf[blockSize + 1]; // + 1 to not overwrite the block
    *buf = 0;
    bool error = false;
    QFile f(theirFileName);
    quint64 blockId = 0;

    f.open(QIODevice::ReadOnly);

    while (!f.atEnd()) {
        QCryptographicHash blockHash(QCryptographicHash::Sha1);
        qint64 readBlockSize = f.read(buf, blockSize);

        if (readBlockSize == -1) {
            sDebug() << "Error!";
            error = true;
            break;
        } else if (readBlockSize < blockSize) {
            sDebug() << "Didn't read a full block, near EOF?";
            sDebug() << "Read a block of " << readBlockSize << " bytes";
        }

        blockHash.addData(buf, readBlockSize);

        {
            // send file overview
            QByteArray data;
            QDataStream stream(&data, QIODevice::WriteOnly);
            stream << theirFileName;
            stream << blockId;
            stream << blockHash.result();

            sendCommand(FileHashReplyCommand, data);
        }

        blockId++;
    }

    sDebug() << "Finished reading file";
}

void SyncManagerSynchroniser::processFileHashReply(QDataStream &stream)
{
    QString theirFileName;
    quint64 theirBlockNumber;
    QByteArray theirBlockHash;

    stream >> theirFileName;
    stream >> theirBlockNumber;
    stream >> theirBlockHash;

    sDebug() << "Got a hash reply for " << theirFileName << " block number "
             <<  theirBlockNumber << " with hash " << theirBlockHash.toHex();

    char buf[blockSize + 1]; // + 1 to not overwrite the block
    *buf = 0;
    QFile f(theirFileName);
    if (!f.open(QIODevice::ReadOnly)) {
        // couldn't open file! probably doesn't exist
        // TODO: proper error checking would be grand
        sDebug() << "Requesting chunk " << theirBlockNumber << "for presumably nonexistent file " << theirFileName;
        
        {
            // send file overview
            QByteArray data;
            QDataStream stream(&data, QIODevice::WriteOnly);
            stream << theirFileName;
            stream << theirBlockNumber;

            sendCommand(FileBlockRequestCommand, data);
        }
    }
    f.seek(blockSize * theirBlockNumber);

    QCryptographicHash blockHash(QCryptographicHash::Sha1);
    qint64 readBlockSize = f.read(buf, blockSize);

    if (readBlockSize == -1) {
        sDebug() << "Error!";
        return;
    } else if (readBlockSize < blockSize) {
        sDebug() << "Didn't read a full block, near EOF?";
        sDebug() << "Read a block of " << readBlockSize << " bytes";
    }

    blockHash.addData(buf, readBlockSize);

    if (blockHash.result() != theirBlockHash) {
        sDebug() << "Differing block hash for " << theirFileName << " id " << theirBlockNumber;
        sDebug() << "   THEIRS: " << theirBlockHash.toHex();
        sDebug() << "   OURS: " << blockHash.result().toHex();

        {
            // send file overview
            QByteArray data;
            QDataStream stream(&data, QIODevice::WriteOnly);
            stream << theirFileName;
            stream << theirBlockNumber;

            sendCommand(FileBlockRequestCommand, data);
        }
    }
}

void SyncManagerSynchroniser::processFileBlockRequest(QDataStream &stream)
{
    QString theirFileName;
    quint64 theirBlockNumber;

    stream >> theirFileName;
    stream >> theirBlockNumber;

    sDebug() << "Got a block request for " << theirFileName << " block number "
             <<  theirBlockNumber;

    char buf[blockSize + 1]; // + 1 to not overwrite the block
    *buf = 0;
    QFile f(theirFileName);
    f.open(QIODevice::ReadOnly);
    f.seek(blockSize * theirBlockNumber);

    qint64 readBlockSize = f.read(buf, blockSize);

    if (readBlockSize == -1) {
        sDebug() << "Error!";
        return;
    } else if (readBlockSize < blockSize) {
        sDebug() << "Didn't read a full block, near EOF?";
        sDebug() << "Read a block of " << readBlockSize << " bytes";
    }

    {
        // send file overview
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << theirFileName;
        stream << theirBlockNumber;
        stream << QByteArray(data, (int)blockSize);

        sendCommand(FileBlockReplyCommand, data);
    }
/*
        FileHashReplyCommand = 0x7,

        // Request the given block number.
        // The peer will then reply with a FileBlockReplyCommand for this block.
        //
        // TBD: how to point out exactly where this file is?
        //
        // QString: <fileName>
        // quint64: blockNumber
        FileBlockRequestCommand = 0x8,

        // FileBlockReplyCommand is sent in response to FileBlockRequestCommand.
        // It contains a given block of a file.
        //
        // TBD: how to point out exactly where this file is?
        //
        // QString: <fileName>
        // quint64: blockNumber
        // QByteArray: block
        FileBlockReplyCommand = 0x10
*/

}

void SyncManagerSynchroniser::processFileBlockReply(QDataStream &stream)
{
    QString theirFileName;
    quint64 theirBlockNumber;
    QByteArray theirBlock;

    stream >> theirFileName;
    stream >> theirBlockNumber;
    stream >> theirBlock;

    sDebug() << "Got a block for " << theirFileName << theirBlockNumber << " of size " << theirBlock.count() << " bytes";

    QFile f(theirFileName);
    f.open(QIODevice::ReadWrite);
    f.seek(blockSize * theirBlockNumber);

    f.write(theirBlock);
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
        case FileInfoCommand:
            processFileInfo(stream);
            break;
        case FileHashRequestCommand:
            processFileHashRequest(stream);
            break;
        case FileHashReplyCommand:
            processFileHashReply(stream);
            break;
        case FileBlockRequestCommand:
            processFileBlockRequest(stream);
            break;
        case FileBlockReplyCommand:
            processFileBlockReply(stream);
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

