// Qt
#include <QTcpSocket>
#include <QHostAddress>

// Saesu
#include <scloudstorage.h>

// Us
#include "saesucloudstoragesynchroniser.h"

SaesuCloudStorageSynchroniser::SaesuCloudStorageSynchroniser(QObject *parent, QTcpSocket *socket)
    : QObject(parent)
    , mBytesExpected(0)
    , mState(Unknown)
{
    if (socket) {
        mSocket = socket;
        mSocket->setParent(this); // make sure we take over
        changeState(); // already connected, so send introduction
    } else {
        mSocket = new QTcpSocket(this);
    }

    connect(mSocket, SIGNAL(connected()), SLOT(onConnected()));
    connect(mSocket, SIGNAL(readyRead()), SLOT(onReadyRead()));
    connect(mSocket, SIGNAL(error(QAbstractSocket::SocketError)), SLOT(onError(QAbstractSocket::SocketError)));
    connect(mSocket, SIGNAL(disconnected()), SLOT(onDisconnected()));
}


void SaesuCloudStorageSynchroniser::onConnected()
{
    sDebug() << "Connected!";
    changeState();
//    quint32 length = 5;
//    mSocket->write(reinterpret_cast<char *>(&length), sizeof(quint32));
//    mSocket->write("hello");
}

void SaesuCloudStorageSynchroniser::sendCommand(quint8 token, const QByteArray &data)
{
    // TODO: endianness?
    quint32 length = (quint32)data.length() + 1;
    mSocket->write(reinterpret_cast<char *>(&length), sizeof(quint32)); // TODO: endianness
    mSocket->write(reinterpret_cast<char *>(&token), sizeof(quint8));
    mSocket->write(data);
}

void SaesuCloudStorageSynchroniser::changeState()
{
    switch (mState) {
    case Unknown:
        mState = DeleteList;
        sDebug() << "Sending delete list";
        changeState();
        break;
    case DeleteList: {
        mState = ObjectList;
        sDebug() << "Sending object list";

        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        mCurrentCloud = SCloudStorage::instance("test"); // TODO: dynamic picking
        stream << QString("test"); // TODO: make this be picked dynamically
        stream << (quint32)mCurrentCloud->itemUUIDs().count();

        foreach (const QByteArray &uuid, mCurrentCloud->itemUUIDs()) {
            SCloudItem *item = mCurrentCloud->item(uuid);

            stream << uuid;
            stream << item->mHash;
            stream << item->mTimeStamp;
        }

        sendCommand(ObjectListCommand, data);
        }
        break;
    case ObjectList:
        mSocket->disconnectFromHost();
        break;
    }
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

void SaesuCloudStorageSynchroniser::processData(const QByteArray &bytes)
{
    QDataStream stream(bytes);

    quint8 command;
    stream >> command;

    switch (command) {
        case ObjectListCommand: {
            QString cloudName;
            stream >> cloudName;
            mCurrentCloud = SCloudStorage::instance(cloudName);

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
                if (!mCurrentCloud->hasItem(uuid)) {
                    sDebug() << "Item " << uuid.toHex() << " not found; requesting";
                    requestItem = true;
                } else {
                    SCloudItem *item = mCurrentCloud->item(uuid);
                    if (item->mHash != itemHash ||
                        item->mTimeStamp != itemTS) {
                        sDebug() << "Modified item " << uuid.toHex() << " detected, requesting";
                        requestItem = true;
                    }
                }

                if (requestItem) {
                    QByteArray sendingData;
                    QDataStream sendingStream(&sendingData, QIODevice::WriteOnly);

                    sendingStream << mCurrentCloud->cloudName();
                    sendingStream << uuid;
                    sendCommand(ObjectRequestCommand, sendingData);
                }
            }
            }
            break;
        case ObjectRequestCommand: {
            QString cloudName;
            stream >> cloudName;
            mCurrentCloud = SCloudStorage::instance(cloudName);

            QByteArray uuid;
            stream >> uuid;
            sDebug() << "Object request for " << uuid.toHex() << " recieved; sending";

            QByteArray sendingData;
            QDataStream sendingStream(&sendingData, QIODevice::WriteOnly);

            sendingStream << cloudName;
            sendingStream << uuid;
            sendingStream << *(mCurrentCloud->item(uuid));
            sendCommand(ObjectReplyCommand, sendingData);
            }
            break;
        case ObjectReplyCommand: {
            QString cloudName;
            stream >> cloudName;
            mCurrentCloud = SCloudStorage::instance(cloudName);

            QByteArray uuid;
            SCloudItem remoteItem;
            stream >> uuid;
            stream >> remoteItem;

            if (!mCurrentCloud->hasItem(uuid)) {
                mCurrentCloud->insertItem(uuid, &remoteItem);
            } else {
                SCloudItem *localItem = mCurrentCloud->item(uuid);

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
                        mCurrentCloud->insertItem(uuid, &remoteItem);
                    }
                } else if (localItem->mTimeStamp < remoteItem.mTimeStamp) {
                    // theirs wins
                    sDebug() << "For modified item " << uuid.toHex() << " using theirs on TS";
                    mCurrentCloud->insertItem(uuid, &remoteItem);
                }
            }
            }
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
