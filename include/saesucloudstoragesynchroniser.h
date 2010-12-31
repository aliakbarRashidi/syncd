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
        // used to select a cloud to synchronise
        // creates if it doesn't exist already
        // should be sent by the client only
        // QString: <cloudname>
        SelectCloudCommand = 0x0,

        // listing all objects and metadata
        // quint32: <objectCount>
        // for count iterations:
        //  QByteArray: object uuid
        //  QString: object hash
        //  quint64: object timestamp
        ObjectListCommand = 0x1,

        // QByteArray: object uuid to request
        ObjectRequestCommand = 0x2,

        // format should follow SCloudItem, see libsaesu's scloudstorage_p.h
        ObjectReplyCommand = 0x3
    };

    enum State
    {
        Unknown,
        Introduction,
        DeleteList,
        ObjectList
    };

    State mState;
};

#endif // SAESUCLOUDSTORAGESYNCHRONISER_H
