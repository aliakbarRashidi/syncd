#ifndef SYNCADVERTISER_H
#define SYNCADVERTISER_H

#include <QObject>
#include <QTcpServer>
#include <QHostInfo>

#include "bonjourrecord.h"
class BonjourServiceResolver;
class SyncManagerSynchroniser;

class SyncAdvertiser : public QObject
{
    Q_OBJECT
public:
    explicit SyncAdvertiser(QObject *parent = 0);

private slots:
    void updateRecords(const QList<BonjourRecord> &list);
    void connectToServer(const QHostInfo &address, int port);
    void onNewConnection();
    void onDisconnected();

private:
    BonjourServiceResolver *mBonjourResolver;
    QTcpServer mServer;
    QList<SyncManagerSynchroniser *> mSyncers;
};

#endif // SYNCADVERTISER_H
