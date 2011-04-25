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

// Us
#include "syncmanager.h"

SyncManager::SyncManager()
     : QObject()
     , mManager("saesu")
{
    connect(&mManager, SIGNAL(objectsAdded(QList<SObjectLocalId>)), SLOT(readObjects()));
    connect(&mManager, SIGNAL(objectsRemoved(QList<SObjectLocalId>)), SLOT(readObjects()));
    connect(&mManager, SIGNAL(objectsUpdated(QList<SObjectLocalId>)), SLOT(readObjects()));

    readObjects();
}

SyncManager::~SyncManager()
{
}

SyncManager *SyncManager::instance()
{
    static SyncManager *manager = NULL;

    if (!manager)
        manager = new SyncManager;

    return manager;
}

void SyncManager::readObjects()
{
    SObjectFetchRequest *fetchRequest = new SObjectFetchRequest;
    connect(fetchRequest, SIGNAL(finished()), SLOT(onObjectsRead()));
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
}

QHash<SObjectLocalId, SObject> SyncManager::objects() const
{
    return mObjects;
}
