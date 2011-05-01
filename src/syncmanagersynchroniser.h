#ifndef SYNCMANAGERSYNCHRONISER_H
#define SYNCMANAGERSYNCHRONISER_H

// Qt
#include <QObject>
#include <QTcpSocket>

// Saesu
class SCloudStorage;

class SyncManagerSynchroniser : public QObject
{
    Q_OBJECT
public:
    explicit SyncManagerSynchroniser(QObject *parent, QTcpSocket *socket = 0);

public slots:
    void connectToHost(const QHostAddress &address);
    void processData(const QByteArray &bytes);

    // command processing
    void processDeleteList(QDataStream &stream);
    void processObjectList(QDataStream &stream);
    void processObjectRequest(QDataStream &stream);
    void processObjectReply(QDataStream &stream);

private slots:
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void onDisconnected();
    void startSync();
    void sendDeleteList(const QString &managerName);
    void sendObjectList(const QString &cloudName);
    void sendCommand(quint8 token, const QByteArray &data);

private:
    QTcpSocket *mSocket;
    quint32 mBytesExpected;

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
        ObjectReplyCommand = 0x3
    };
};

#endif // SYNCMANAGERSYNCHRONISER_H
