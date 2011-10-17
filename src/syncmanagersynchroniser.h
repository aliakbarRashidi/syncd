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

#ifndef SYNCMANAGERSYNCHRONISER_H
#define SYNCMANAGERSYNCHRONISER_H

// Qt
#include <QObject>
#include <QTcpSocket>

// Saesu
class SCloudStorage;
#include "sobject.h"

class SyncManagerSynchroniser : public QObject
{
    Q_OBJECT
public:
    explicit SyncManagerSynchroniser(QObject *parent, QTcpSocket *socket = 0);

    bool isOutgoing() const;

public slots:
    void connectToHost(const QHostAddress &address, int port);
    void processData(const QByteArray &bytes);
    void disconnectFromHost() { mSocket->disconnectFromHost(); }

    // command processing
    void processCurrentTime(QDataStream &stream);
    void processDeleteList(QDataStream &stream);
    void processObjectList(QDataStream &stream);
    void processObjectRequest(QDataStream &stream);
    void processObjectReply(QDataStream &stream);
    void processFileInfo(QDataStream &stream);
    void processFileHashRequest(QDataStream &stream);
    void processFileHashReply(QDataStream &stream);
    void processFileBlockRequest(QDataStream &stream);
    void processFileBlockReply(QDataStream &stream);

private slots:
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void onDisconnected();
    void startSync();
    void sendDeleteList(const QString &managerName, const QList<SObjectLocalId> &ids);
    void sendObjectList(const QString &cloudName, const QList<SObject> &objects);
    void sendCommand(quint8 token, const QByteArray &data);

private:
    QTcpSocket *mSocket;
    quint32 mBytesExpected;
    bool mIsOutgoing;

    // expected handshake proceedure:
    // exchange auth (TBD)
    // exchange CurrentTimeCommand, abort if excessive delta
    // exchange DeleteListCommand(s), delete objects as appropriate
    // exchange ObjectListCommand(s), interleave with ObjectRequestCommand(s)
    // reply with ObjectReplyCommand instances

    enum CommandTokens
    {
        // listing all deleted object ids
        // QString <cloudName>
        // quint32 <objectCount>
        // for objectCount iterations
        //   SObjectLocalId deletedId
        DeleteListCommand = 0x0,

        // listing all objects and metadata
        // QString: <cloudName>
        // quint32: <objectCount>
        // for count iterations:
        //  QByteArray: object uuid
        //  QString: object hash
        //  quint64: object timestamp
        ObjectListCommand = 0x1,

        // QString: <cloudName>
        // QByteArray: object uuid to request
        ObjectRequestCommand = 0x2,

        // QString: <cloudName>
        // QByteArray <uuid>
        // SCloudItem <item>, see libsaesu for exact formatting
        ObjectReplyCommand = 0x3,

        // used to inform the other side as to what the time is from our point of view
        // if the time delta is excessive, synchronisation will be halted
        // qint64: milliseconds since the epoch
        CurrentTimeCommand = 0x4,

        // Gives information about a file to a peer.
        // Peers may then issue FileHashRequestCommand if their version of the
        // file doesn't match.
        //
        // TBD: how to point out exactly where this file is?
        //
        // QString: <fileName>
        // quint64: fileSize
        // QByteArray: hash of the file, in sha-1
        FileInfoCommand = 0x5,

        // Request the hash information for a given file.
        //
        // The recieving peer then sends hash information for each block of the
        // requested file, using FileHashReplyCommand for each block
        //
        // TBD: how to point out exactly where this file is?
        //
        // QString: <fileName>
        FileHashRequestCommand = 0x6,

        // Many of these may be sent in response to a single FileHashRequestCommand.
        //
        // One is sent for each 'block' of a file. A block is 4096 bytes of a
        // file, and a 'block number' indicates which block is being referred
        // to, starting at 0. Thus, block 4 would be 4 * 4096 = 16384 bytes into
        // a file, but is actually the 5th 'block'.
        //
        // If the peer does not have a matching hash for this block, it may
        // request this block using FileBlockRequestCommand.
        //
        // TBD: how to point out exactly where this file is?
        //
        // QString: <fileName>
        // quint64: blockNumber
        // QByteArray: hash of the block number
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
    };
};

#endif // SYNCMANAGERSYNCHRONISER_H
