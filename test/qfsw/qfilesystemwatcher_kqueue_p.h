/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the QtCore module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** GNU Lesser General Public License Usage
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this
** file. Please review the following information to ensure the GNU Lesser
** General Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 3.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of this
** file. Please review the following information to ensure the GNU General
** Public License version 3.0 requirements will be met:
** http://www.gnu.org/copyleft/gpl.html.
**
** Other Usage
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
**
**
**
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef FILEWATCHER_KQUEUE_P_H
#define FILEWATCHER_KQUEUE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of the QLibrary class.  This header file may change from
// version to version without notice, or even be removed.
//
// We mean it.
//

#include "qfilesystemwatcher_p.h"

#include <QtCore/qhash.h>
#include <QtCore/qmutex.h>
#include <QtCore/qthread.h>
#include <QtCore/qvector.h>
#include <QtCore/qsocketnotifier.h>
#include <QtCore/qset.h>

#ifndef QT_NO_FILESYSTEMWATCHER
struct kevent;

QT_BEGIN_NAMESPACE

class QKqueueFileSystemWatcherEngine : public QFileSystemWatcherEngine
{
    Q_OBJECT
public:
    ~QKqueueFileSystemWatcherEngine();

    static QKqueueFileSystemWatcherEngine *create(QObject *parent);

    bool internalAddPath(const QString &path, QStringList *files, QStringList *directories);
    QStringList addPaths(const QStringList &paths, QStringList *files, QStringList *directories);
    QStringList removePaths(const QStringList &paths, QStringList *files, QStringList *directories);

private Q_SLOTS:
    void readFromKqueue();

private:
    QKqueueFileSystemWatcherEngine(int kqfd, QObject *parent);

    int kqfd;

    QSet<QString> reallyWatchedPaths;
    QHash<QString, int> pathToID;
    QHash<int, QString> idToPath;
    QSocketNotifier notifier;
};

QT_END_NAMESPACE

#endif //QT_NO_FILESYSTEMWATCHER
#endif // FILEWATCHER_KQUEUE_P_H