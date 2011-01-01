// Qt
#include <QTcpSocket>
#include <QHostAddress>
#include <QDir>

// Saesu
#include <scloudstorage.h>

// Us
#include "saesucloudstoragesynchroniser.h"

SaesuCloudStorageSynchroniser::SaesuCloudStorageSynchroniser(QObject *parent, QTcpSocket *socket)
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

void SaesuCloudStorageSynchroniser::sendCommand(quint8 token, const QByteArray &data)
{
    // TODO: endianness?
    quint32 length = (quint32)data.length() + 1;
    mSocket->write(reinterpret_cast<char *>(&length), sizeof(quint32)); // TODO: endianness
    mSocket->write(reinterpret_cast<char *>(&token), sizeof(quint8));
    mSocket->write(data);
}

void SaesuCloudStorageSynchroniser::startSync()
{
     // TODO: dynamic picking
    QDir cloudsDir(SCloudStorage::cloudPath(QLatin1String("")));
    QStringList clouds = cloudsDir.entryList();

    foreach (const QString &cloudName, clouds) {
        QFileInfo fi(cloudsDir.absoluteFilePath(cloudName));

        if (fi.isFile()) {
            sDebug() << "Skipping file " << cloudName;
            continue;
        }

        if (cloudName[0] == '.') {
            sDebug() << "Skipping dotfile " << cloudName;
            continue;
        }

        sDebug() << "Synchronising " << cloudName;
        syncCloud(cloudName);
    }
}

void SaesuCloudStorageSynchroniser::syncCloud(const QString &cloudName)
{
    sDebug() << "Sending delete list";

    sDebug() << "Sending object list";

    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    SCloudStorage *cloud = SCloudStorage::instance(cloudName);
    stream << cloud->cloudName();
    stream << (quint32)cloud->itemUUIDs().count();

    foreach (const QByteArray &uuid, cloud->itemUUIDs()) {
        SCloudItem *item = cloud->item(uuid);

        stream << uuid;
        stream << item->mHash;
        stream << item->mTimeStamp;
    }

    sendCommand(ObjectListCommand, data);
}

void SaesuCloudStorageSynchroniser::onReadyRead()
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

void SaesuCloudStorageSynchroniser::processObjectList(QDataStream &stream)
{
    QString cloudName;
    stream >> cloudName;
    SCloudStorage *cloud = SCloudStorage::instance(cloudName);

    // sending a message, find message
    sDebug() << "Processing an object list";

    quint32 itemCount;
    stream >> itemCount;

    sDebug() << itemCount << " items";

    for (quint32 i = 0; i < itemCount; ++i) {
        QByteArray uuid;
        QByteArray itemHash;
        quint64 itemTS;

        stream >> uuid;
        stream >> itemHash;
        stream >> itemTS;

        bool requestItem = false;
        if (!cloud->hasItem(uuid)) {
            sDebug() << "Item " << uuid.toHex() << " not found; requesting";
            requestItem = true;
        } else {
            SCloudItem *item = cloud->item(uuid);
            if (item->mHash != itemHash ||
                item->mTimeStamp != itemTS) {
                sDebug() << "Modified item " << uuid.toHex() << " detected, requesting";
                requestItem = true;
            }
        }

        if (requestItem) {
            QByteArray sendingData;
            QDataStream sendingStream(&sendingData, QIODevice::WriteOnly);

            sendingStream << cloud->cloudName();
            sendingStream << uuid;
            sendCommand(ObjectRequestCommand, sendingData);
        }
    }
}

void SaesuCloudStorageSynchroniser::processObjectRequest(QDataStream &stream)
{
    QString cloudName;
    stream >> cloudName;
    SCloudStorage *cloud = SCloudStorage::instance(cloudName);

    QByteArray uuid;
    stream >> uuid;
    sDebug() << "Object request for " << uuid.toHex() << " recieved; sending";

    QByteArray sendingData;
    QDataStream sendingStream(&sendingData, QIODevice::WriteOnly);

    sendingStream << cloudName;
    sendingStream << uuid;
    sendingStream << *(cloud->item(uuid));
    sendCommand(ObjectReplyCommand, sendingData);
}

void SaesuCloudStorageSynchroniser::processObjectReply(QDataStream &stream)
{
    QString cloudName;
    stream >> cloudName;
    SCloudStorage *cloud = SCloudStorage::instance(cloudName);

    QByteArray uuid;
    SCloudItem remoteItem;
    stream >> uuid;
    stream >> remoteItem;

    if (!cloud->hasItem(uuid)) {
        cloud->insertItem(uuid, &remoteItem);
    } else {
        SCloudItem *localItem = cloud->item(uuid);

        // find out which is the newer item
        if (localItem->mTimeStamp > remoteItem.mTimeStamp) {
            // ours is newer, ignore theirs
            sDebug() << "For modified item " << uuid.toHex() << ", using ours on TS";
        } else if (localItem->mTimeStamp == remoteItem.mTimeStamp) {

            // identical, resort to alphabetically superior SHA to force a compromise
            if (localItem->mHash > remoteItem.mHash) {
                // ours is alphabetically superior, ignore theirs
                sDebug() << "For modified item " << uuid.toHex() << " using ours on hash";
            } else if (localItem->mHash == remoteItem.mHash) {
                // what else can we realistically do?
                sWarning() << "Identical hashes but differing timestamps detected for " << uuid.toHex() << ", data state now inconsistent!";
            } else if (localItem->mHash < remoteItem.mHash) {
                // take theirs
                sDebug() << "For modified item " << uuid.toHex() << " using theirs on hash";
                cloud->insertItem(uuid, &remoteItem);
            }
        } else if (localItem->mTimeStamp < remoteItem.mTimeStamp) {
            // theirs wins
            sDebug() << "For modified item " << uuid.toHex() << " using theirs on TS";
            cloud->insertItem(uuid, &remoteItem);
        }
    }
}

void SaesuCloudStorageSynchroniser::processData(const QByteArray &bytes)
{
    QDataStream stream(bytes);

    quint8 command;
    stream >> command;

    switch (command) {
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

void SaesuCloudStorageSynchroniser::connectToHost(const QHostAddress &address)
{
    sDebug() << "Connecting to " << address;
    mSocket->connectToHost(address, 1337);
}

void SaesuCloudStorageSynchroniser::onError(QAbstractSocket::SocketError error)
{
    sDebug() << "Had an error: " << error;
    deleteLater();
}

void SaesuCloudStorageSynchroniser::onDisconnected()
{
    sDebug() << "Connection closed";
    deleteLater();
}
