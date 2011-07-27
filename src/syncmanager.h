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

    QList<SObject> objects() const;

    QHash<SObjectLocalId, SObject> objectHash() const;

    QList<SObjectLocalId> deleteList() const;

    SObjectManager *manager();

    void ensureRemoved(const QList<SObjectLocalId> &ids);

    bool isRemoved(const SObjectLocalId &id) const;

signals:
    void objectsAddedOrUpdated(const QString &managerName, const QList<SObject> &objects);
    void objectsDeleted(const QString &managerName, const QList<SObjectLocalId> &ids);

private slots:
    void readObjects(const QList<SObjectLocalId> &ids);
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
