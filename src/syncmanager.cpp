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

// Qt
#include <QObject>

// saesu
#include <sobjectmanager.h>
#include <sobjectfetchrequest.h>
#include <sobjectremoverequest.h>
#include <sdeletelistfetchrequest.h>

// Us
#include "syncmanager.h"

SyncManager::SyncManager(const QString &managerName)
     : QObject()
     , mManager(managerName)
     , mManagerName(managerName)
{
    connect(&mManager, SIGNAL(objectsAdded(QList<SObjectLocalId>)), SLOT(readObjects()));
    connect(&mManager, SIGNAL(objectsRemoved(QList<SObjectLocalId>)), SLOT(onObjectsRemoved(QList<SObjectLocalId>)));
    connect(&mManager, SIGNAL(objectsUpdated(QList<SObjectLocalId>)), SLOT(readObjects()));

    readObjects();

    SDeleteListFetchRequest *deleteFetchRequest = new SDeleteListFetchRequest;
    connect(deleteFetchRequest, SIGNAL(finished()), SLOT(onDeleteListRead()));
    connect(deleteFetchRequest, SIGNAL(finished()), deleteFetchRequest, SLOT(deleteLater()));
    deleteFetchRequest->start(&mManager);
}

SyncManager::~SyncManager()
{
}

SyncManager *SyncManager::instance(const QString &managerName)
{
    static QHash<QString, SyncManager *> managerMap;

    QHash<QString, SyncManager *>::ConstIterator it = managerMap.find(managerName);
    if (it == managerMap.end()) {
        SyncManager *manager = new SyncManager(managerName);
        managerMap.insert(managerName, manager);
        return manager;
    }

    return *it;
}

SObjectManager *SyncManager::manager()
{
    return &mManager;
}

void SyncManager::readObjects()
{
    SObjectFetchRequest *fetchRequest = new SObjectFetchRequest;
    connect(fetchRequest, SIGNAL(finished()), SLOT(onObjectsRead()));
    connect(fetchRequest, SIGNAL(finished()), fetchRequest, SLOT(deleteLater()));
    fetchRequest->start(&mManager);
}

void SyncManager::onObjectsRead()
{
    mObjects.clear();

    SObjectFetchRequest *req = qobject_cast<SObjectFetchRequest*>(sender());

    QList<SObject> objects = req->objects();

    foreach (const SObject &object, objects) {
        mObjects.insert(object.id().localId(), object);
    }

    emit resyncRequired(mManagerName);
}

void SyncManager::onObjectsRemoved(const QList<SObjectLocalId> &ids)
{
    // don't clear the list here, we haven't got the full list
    mDeleteList.append(ids);

    foreach (const SObjectLocalId &id, ids) {
        mDeleteListHash.insert(id);
    }


    emit objectsDeleted(mManagerName, ids);
}

void SyncManager::onDeleteListRead()
{
    mDeleteList.clear();
    SDeleteListFetchRequest *req = qobject_cast<SDeleteListFetchRequest*>(sender());

    mDeleteList = req->objectIds();

    foreach (const SObjectLocalId &id, mDeleteList) {
        mDeleteListHash.insert(id);
    }

    emit objectsDeleted(mManagerName, mDeleteList);
}

void SyncManager::ensureRemoved(const QList<SObjectLocalId> &ids)
{
    QList<SObjectLocalId> notRemovedYet;

    foreach (const SObjectLocalId &id, ids) {
        if (mDeleteListHash.contains(id))
            continue;

        notRemovedYet.append(id);
        mDeleteListHash.insert(id);
    }

    if (notRemovedYet.count() == 0)
        return; // no need to start an empty request

    SObjectRemoveRequest *removeRequest = new SObjectRemoveRequest;
    connect(removeRequest, SIGNAL(finished()), removeRequest, SLOT(deleteLater()));
    removeRequest->setObjectIds(notRemovedYet);
    removeRequest->start(&mManager);

    mDeleteList.append(notRemovedYet);
}

bool SyncManager::isRemoved(const SObjectLocalId &id) const
{
    return mDeleteListHash.contains(id);
}

QList<SObjectLocalId> SyncManager::deleteList() const
{
    return mDeleteList;
}

QHash<SObjectLocalId, SObject> SyncManager::objects() const
{
    return mObjects;
}
