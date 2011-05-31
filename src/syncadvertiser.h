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
