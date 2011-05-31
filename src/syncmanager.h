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

#ifndef SYNCMANAGER_H
#define SYNCMANAGER_H

// Qt
#include <QObject>
#include <QString>
#include <QSet>

// saesu
#include <sobject.h>
#include <sobjectmanager.h>
#include <sobjectid.h>

class SyncManager : public QObject
{
    Q_OBJECT
public:
    explicit SyncManager(const QString &managerName);
    virtual ~SyncManager();

    static SyncManager *instance(const QString &managerName);

    QHash<SObjectLocalId, SObject> objects() const;

    QList<SObjectLocalId> deleteList() const;

    SObjectManager *manager();

    void ensureRemoved(const QList<SObjectLocalId> &ids);

    bool isRemoved(const SObjectLocalId &id) const;

signals:
    void resyncRequired(const QString &managerName);
    void objectsDeleted(const QString &managerName, const QList<SObjectLocalId> &ids);

private slots:
    void readObjects();
    void onObjectsRead();
    void onDeleteListRead();
    void onObjectsRemoved(const QList<SObjectLocalId> &ids);

private:
    QHash<SObjectLocalId, SObject> mObjects;
    SObjectManager mManager;
    QList<SObjectLocalId> mDeleteList;
    QSet<SObjectLocalId> mDeleteListHash;
    QString mManagerName; // TODO: this should perhaps be moved to SObjectManager
};

#endif // SYNCMANAGER_H
