/*
 * Copyright (C) 2011 Robin Burchell <viroteck@viroteck.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
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
    
    SDeleteListFetchRequest *deleteFetchRequest = new SDeleteListFetchRequest;
    connect(deleteFetchRequest, SIGNAL(finished()), SLOT(onDeleteListRead()));
    connect(deleteFetchRequest, SIGNAL(finished()), deleteFetchRequest, SLOT(deleteLater()));
    deleteFetchRequest->start(&mManager);
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
