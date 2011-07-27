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

// Qt
#include <QObject>

// saesu
#include <sobjectmanager.h>
#include <sobjectfetchrequest.h>
#include <sobjectremoverequest.h>
#include <sdeletelistfetchrequest.h>
#include <sobjectlocalidfilter.h>

// Us
#include "syncmanager.h"

SyncManager::SyncManager(const QString &managerName)
     : QObject()
     , mManager(managerName)
     , mManagerName(managerName)
{
    connect(&mManager, SIGNAL(objectsAdded(QList<SObjectLocalId>)), SLOT(readObjects(QList<SObjectLocalId>)));
    connect(&mManager, SIGNAL(objectsRemoved(QList<SObjectLocalId>)), SLOT(onObjectsRemoved(QList<SObjectLocalId>)));
    connect(&mManager, SIGNAL(objectsUpdated(QList<SObjectLocalId>)), SLOT(readObjects(QList<SObjectLocalId>)));

    readObjects(QList<SObjectLocalId>());

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

void SyncManager::readObjects(const QList<SObjectLocalId> &ids)
{
    SObjectFetchRequest *fetchRequest = new SObjectFetchRequest;

    if (ids.count()) {
        SObjectLocalIdFilter filter;
        filter.setIds(ids);
        fetchRequest->setFilter(filter);
    }

    connect(fetchRequest, SIGNAL(finished()), SLOT(onObjectsRead()));
    connect(fetchRequest, SIGNAL(finished()), fetchRequest, SLOT(deleteLater()));
    fetchRequest->start(&mManager);
}

void SyncManager::onObjectsRead()
{
    SObjectFetchRequest *req = qobject_cast<SObjectFetchRequest*>(sender());

    QList<SObject> objects = req->objects();

    foreach (const SObject &object, objects) {
        mObjects.insert(object.id().localId(), object);
    }

    emit objectsAddedOrUpdated(mManagerName, objects);
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

QList<SObject> SyncManager::objects() const
{
    QList<SObject> objectsList;
    foreach (const SObject &obj, mObjects) {
        objectsList.append(obj);
    }

    return objectsList;
}

QHash<SObjectLocalId, SObject> SyncManager::objectHash() const
{
    return mObjects;
}
