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
