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
    case Unknown: {
        mState = Introduction;
        sDebug() << "Sending introduction";

        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);

        stream << QString("test"); // TODO: make it dynamically pick cloud names
        sendCommand(SelectCloudCommand, data);
        }
        break;
    case Introduction:
        mState = DeleteList;
        sDebug() << "Sending delete list";
        changeState();
        break;
    case DeleteList: {
        mState = ObjectList;
        sDebug() << "Sending object list";

        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << (quint32)mCurrentCloud->itemUUIDs().count();

        foreach (const QByteArray &uuid, mCurrentCloud->itemUUIDs()) {
            QString itemHash = mCurrentCloud->hash(uuid);
            quint64 itemTS = mCurrentCloud->modifiedAt(uuid);

            stream << uuid;
            stream << itemHash;
            stream << itemTS;

            sDebug() << "Sending item: " << uuid;
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
    sDebug() << "Processing " << bytes.toHex();
    QDataStream stream(bytes);

    quint8 command;
    stream >> command;

    switch (command) {
        case SelectCloudCommand: {
            QString cloudName;
            stream >> cloudName;

            sDebug() << "Synchronising for " << cloudName;
            sDebug() << *(bytes.begin() + 4);
            mCurrentCloud = SCloudStorage::instance(cloudName);
            changeState();
            }
            break;
        case ObjectListCommand: {
            if (!mCurrentCloud) {
                sDebug() << "Got a command with no selected cloud";
                mSocket->disconnectFromHost();
                return;
            }

            // sending a message, find message
            sDebug() << "Processing an object list";

            quint32 itemCount;
            stream >> itemCount;

            sDebug() << itemCount << " items";

            for (quint32 i = 0; i < itemCount; ++i) {
                QByteArray uuid;
                QString itemHash;
                quint64 itemTS;

                stream >> uuid;
                stream >> itemHash;
                stream >> itemTS;

                sDebug() << "Got item: " << uuid;

                //if (!mCurrentCloud->hasItem(uuid)) {
                    sDebug() << "Item " << uuid.toHex() << " not found; requesting";
                    QByteArray sendingData;
                    QDataStream sendingStream(&sendingData, QIODevice::WriteOnly);

                    sendingStream << uuid;
                    sendCommand(ObjectRequestCommand, sendingData);
                //}
            }
            }
            break;
        case ObjectRequestCommand: {
            if (!mCurrentCloud) {
                sDebug() << "Got a command with no selected cloud";
                mSocket->disconnectFromHost();
                return;
            }

            QByteArray uuid;
            stream >> uuid;
            sDebug() << "Unknown object request for " << uuid << " recieved; sending";

            QByteArray sendingData;
            QDataStream sendingStream(&sendingData, QIODevice::WriteOnly);

            sendingStream << uuid;
            sendingStream << *(mCurrentCloud->item(uuid));
            sendCommand(ObjectReplyCommand, sendingData);
            }
            break;
        case ObjectReplyCommand: {
            if (!mCurrentCloud) {
                sDebug() << "Got a command with no selected cloud";
                mSocket->disconnectFromHost();
                return;
            }

            QByteArray uuid;
            SCloudItem item;
            stream >> uuid;
            stream >> item;
            mCurrentCloud->insertItem(uuid, &item);
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
