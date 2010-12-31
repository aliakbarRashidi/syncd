#ifndef SAESUCLOUDSTORAGESYNCHRONISER_H
#define SAESUCLOUDSTORAGESYNCHRONISER_H

// Qt
#include <QObject>
#include <QTcpSocket>

// Saesu
class SCloudStorage;

class SaesuCloudStorageSynchroniser : public QObject
{
    Q_OBJECT
public:
    explicit SaesuCloudStorageSynchroniser(QObject *parent, QTcpSocket *socket = 0);

public slots:
    void connectToHost(const QHostAddress &address);
    void processData(const QByteArray &bytes);

private slots:
    void onConnected();
    void onReadyRead();
    void onError(QAbstractSocket::SocketError error);
    void onDisconnected();
    void changeState();
    void sendCommand(quint8 token, const QByteArray &data);
private:
    QTcpSocket *mSocket;
    SCloudStorage *mCurrentCloud;
    quint32 mBytesExpected;

    enum CommandTokens
    {
        // listing all objects and metadata
        // QString: <cloudName>
        // quint32: <objectCount>
        // for count iterations:
        //  QByteArray: object uuid
        //  QString: object hash
        //  quint64: object timestamp
        ObjectListCommand = 0x0,

        // QString: <cloudName>
        // QByteArray: object uuid to request
        ObjectRequestCommand = 0x1,

        // QString: <cloudName>
        // QByteArray <uuid>
        // SCloudItem <item>, see libsaesu for exact formatting
        ObjectReplyCommand = 0x2
    };

    enum State
    {
        Introduction,
        DeleteList,
        ObjectList
    };

    State mState;
};

#endif // SAESUCLOUDSTORAGESYNCHRONISER_H
