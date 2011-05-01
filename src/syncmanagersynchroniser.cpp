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
        mSocket = socket;
        mSocket->setParent(this); // make sure we take over
        startSync(); // already connected, so send introduction
    } else {
        mSocket = new QTcpSocket(this);
    }

    connect(mSocket, SIGNAL(connected()), SLOT(startSync()));
    connect(mSocket, SIGNAL(readyRead()), SLOT(onReadyRead()));
    connect(mSocket, SIGNAL(error(QAbstractSocket::SocketError)), SLOT(onError(QAbstractSocket::SocketError)));
    connect(mSocket, SIGNAL(disconnected()), SLOT(onDisconnected()));
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

    foreach (const QString &database, databases) {
        connect(SyncManager::instance(database),
                SIGNAL(resyncRequired(QString)),
                SLOT(sendObjectList(QString)),
                Qt::UniqueConnection);
        connect(SyncManager::instance(database),
                SIGNAL(deleteListChanged(QString)),
                SLOT(sendDeleteList(QString)),
                Qt::UniqueConnection);
        sendDeleteList(database);
        sendObjectList(database);
    }
}

void SyncManagerSynchroniser::sendDeleteList(const QString &managerName)
{
    sDebug() << "Sending delete list";
    QList<SObjectLocalId> deleteList = SyncManager::instance(managerName)->deleteList();
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << managerName;
    stream << (quint32)deleteList.count();

    foreach (const SObjectLocalId &localId, deleteList) {
        stream << localId;
    }
    
    sendCommand(DeleteListCommand, data);
}

void SyncManagerSynchroniser::sendObjectList(const QString &cloudName)
{
    sDebug() << "Sending object list";
    QHash<SObjectLocalId, SObject> objects = SyncManager::instance(cloudName)->objects();

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream << cloudName;
    stream << (quint32)objects.count();

    foreach (const SObjectLocalId &localId, objects.keys()) {
        SObject &obj = objects[localId];

        stream << localId;
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
    sDebug() << "Processing a delete list";

    quint32 itemCount;
    stream >> itemCount;

    sDebug() << itemCount << " items";
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
    sDebug() << "Processing an object list";
    QHash<SObjectLocalId, SObject> objects = SyncManager::instance(cloudName)->objects();

    quint32 itemCount;
    stream >> itemCount;

    sDebug() << itemCount << " items";

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
            sDebug() << "Item " << uuid << " not found; requesting";
            requestItem = true;
        } else {
            const SObject &obj = *cit;

            if (obj.hash() != itemHash ||
                obj.lastSaved() != itemTS) {
                sDebug() << "Modified item " << uuid << " detected, requesting";
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

    QHash<SObjectLocalId, SObject> objects = SyncManager::instance(cloudName)->objects();
    QHash<SObjectLocalId, SObject>::ConstIterator cit = objects.find(uuid);
    if (cit == objects.end()) {
        sDebug() << "Recieved a request for a nonexistant item! UUID: " << uuid;
        return;
    }

    sDebug() << "Object request for " << uuid << " recieved; sending";

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
        sDebug() << "Ignoring deleted UUID " << uuid;
        return;
    }

    QHash<SObjectLocalId, SObject> objects = SyncManager::instance(cloudName)->objects();
    QHash<SObjectLocalId, SObject>::ConstIterator cit = objects.find(uuid);
    bool saveItem = false;

    if (cit == objects.end()) {
        sDebug() << "Inserting an item I don't have";
        saveItem = true;
    } else {
        const SObject &localItem = *cit;
        
        sDebug() << "Existing object " << uuid;
        sDebug() << "   LOCAL TS: " << localItem.lastSaved();
        sDebug() << "   REMOTE TS: " << remoteItem.lastSaved();

        // find out which is the newer item
        if (localItem.lastSaved() > remoteItem.lastSaved()) {
            // ours is newer, ignore theirs
            sDebug() << "For modified item " << uuid << ", using ours on TS";
        } else if (localItem.lastSaved() == remoteItem.lastSaved()) {
            // identical, resort to alphabetically superior SHA to force a compromise
            sDebug() << "TS equal; hash comparison needed";
            sDebug() << "  LOCAL HASH: " << localItem.hash();
            sDebug() << "  REMOTE HASH: " << remoteItem.hash();
            if (localItem.hash() > remoteItem.hash()) {
                // ours is alphabetically superior, ignore theirs
                sDebug() << "For modified item " << uuid << " using ours on hash";
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
                sDebug() << "For modified item " << uuid << " using theirs on hash";
                saveItem = true;
            }
        } else if (localItem.lastSaved() < remoteItem.lastSaved()) {
            // theirs wins
            sDebug() << "For modified item " << uuid << " using theirs on TS";
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

void SyncManagerSynchroniser::processData(const QByteArray &bytes)
{
    QDataStream stream(bytes);

    quint8 command;
    stream >> command;

    switch (command) {
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

void SyncManagerSynchroniser::connectToHost(const QHostAddress &address)
{
    sDebug() << "Connecting to " << address;
    mSocket->connectToHost(address, 1337);
}

void SyncManagerSynchroniser::onError(QAbstractSocket::SocketError error)
{
    sDebug() << "Had an error: " << error;
    deleteLater();
}

void SyncManagerSynchroniser::onDisconnected()
{
    sDebug() << "Connection closed";
    deleteLater();
}

