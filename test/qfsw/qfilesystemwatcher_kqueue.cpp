/****************************************************************************
**
** Copyright (C) 2012 Robin Burchell <robin+qt@viroteck.net>
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

#include <qplatformdefs.h>

#include "qfilesystemwatcher.h"
#include "qfilesystemwatcher_kqueue_p.h"
#include "private/qcore_unix_p.h"

#ifndef QT_NO_FILESYSTEMWATCHER

#include <qdebug.h>
#include <qfile.h>
#include <qsocketnotifier.h>
#include <qvarlengtharray.h>
#include <qdiriterator.h>

#include <sys/types.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

QT_BEGIN_NAMESPACE

// #define KEVENT_DEBUG
#ifdef KEVENT_DEBUG
#  define DEBUG qDebug
#else
#  define DEBUG if(false)qDebug
#endif

QKqueueFileSystemWatcherEngine *QKqueueFileSystemWatcherEngine::create(QObject *parent)
{
    // the default open file count for OS X at least is absolutely ridiculous.
    // raise it to something a little better.
    int retval = 0;
    struct rlimit rl;
    retval = getrlimit(RLIMIT_NOFILE, &rl);
    if (retval == -1)
        perror("QKqueueFileSystemWatcherEngine: getrlimit");

    rl.rlim_cur = (OPEN_MAX < RLIM_INFINITY) ? OPEN_MAX : RLIM_INFINITY;
    DEBUG() << "QKqueueFileSystemWatcherEngine: increasing fd limit to " << rl.rlim_cur;

    retval = setrlimit(RLIMIT_NOFILE, &rl);
    if (retval == -1)
        perror("QKqueueFileSystemWatcherEngine: setrlimit");

    int kqfd = kqueue();
    if (kqfd == -1)
        return 0;
    return new QKqueueFileSystemWatcherEngine(kqfd, parent);
}

QKqueueFileSystemWatcherEngine::QKqueueFileSystemWatcherEngine(int kqfd, QObject *parent)
    : QFileSystemWatcherEngine(parent),
      kqfd(kqfd),
      notifier(kqfd, QSocketNotifier::Read, this)
{
    connect(&notifier, SIGNAL(activated(int)), SLOT(readFromKqueue()));

    fcntl(kqfd, F_SETFD, FD_CLOEXEC);
}

QKqueueFileSystemWatcherEngine::~QKqueueFileSystemWatcherEngine()
{
    notifier.setEnabled(false);
    close(kqfd);

    foreach (int id, pathToID)
        ::close(id < 0 ? -id : id);
}

bool QKqueueFileSystemWatcherEngine::internalAddPath(const QString &path,
                                                     QStringList *files,
                                                     QStringList *directories)
{
    int fd;
#if defined(O_EVTONLY)
    fd = qt_safe_open(QFile::encodeName(path), O_EVTONLY);
#else
    fd = qt_safe_open(QFile::encodeName(path), O_RDONLY);
#endif
    if (fd == -1) {
        perror("QKqueueFileSystemWatcherEngine::addPaths: open");
        return false;
    }
    if (fd >= (int)FD_SETSIZE / 2 && fd < (int)FD_SETSIZE) {
        int fddup = fcntl(fd, F_DUPFD, FD_SETSIZE);
        if (fddup != -1) {
            ::close(fd);
            fd = fddup;
        }
    }
    fcntl(fd, F_SETFD, FD_CLOEXEC);

    QT_STATBUF st;
    if (QT_FSTAT(fd, &st) == -1) {
        perror("QKqueueFileSystemWatcherEngine::addPaths: fstat");
        ::close(fd);
        return false;
    }
    int id = (S_ISDIR(st.st_mode)) ? -fd : fd;
    if (id < 0) {
        if (directories->contains(path)) {
            ::close(fd);
            return false;
        }
    } else {
        if (files->contains(path)) {
            ::close(fd);
            return false;
        }
    }

    struct kevent kev;
    EV_SET(&kev,
            fd,
            EVFILT_VNODE,
            EV_ADD | EV_ENABLE | EV_CLEAR,
            NOTE_DELETE | NOTE_WRITE | NOTE_EXTEND | NOTE_ATTRIB | NOTE_RENAME | NOTE_REVOKE,
            0,
            0);
    if (kevent(kqfd, &kev, 1, 0, 0, 0) == -1) {
        perror("QKqueueFileSystemWatcherEngine::addPaths: kevent");
        ::close(fd);
        return false;
    }

    if (id < 0) {
        DEBUG() << "QKqueueFileSystemWatcherEngine: added directory path" << path;
        directories->append(path);

        // when adding a directory, we also need to iterate the contents
        // of the directory, and watch all files in it to get directory
        // notifications, as kqueue does not provide directory-level monitoring
        // of changes.
        QDirIterator di(path);

        while (di.hasNext()) {
            const QString &subPath = di.next();
            QFileInfo fi(subPath);

            // TODO: failure handling. if we fail to watch something inside the
            // directory, we must un-watch everything we just watched, and
            // indicate failure.
            //
            // the cause for failure is _probably_ hitting RLIMIT_NOFILE, which
            // shouldn't happen (except in morbid cases), since we explicitly
            // set that pretty high.
            if (fi.isFile())
                internalAddPath(subPath, files, directories);
        }
    } else {
        DEBUG() << "QKqueueFileSystemWatcherEngine: added file path" << path;
        files->append(path);
    }

    pathToID.insert(path, id);
    idToPath.insert(id, path);
    return true;
}

QStringList QKqueueFileSystemWatcherEngine::addPaths(const QStringList &paths,
                                                     QStringList *files,
                                                     QStringList *directories)
{
    QStringList p = paths;
    QMutableListIterator<QString> it(p);
    while (it.hasNext()) {
        QString path = it.next();

        if (internalAddPath(path, files, directories))
            it.remove();

        reallyWatchedPaths.insert(path);
    }

    return p;
}

QStringList QKqueueFileSystemWatcherEngine::removePaths(const QStringList &paths,
                                                        QStringList *files,
                                                        QStringList *directories)
{
    bool isEmpty;
    QStringList p = paths;
    if (pathToID.isEmpty())
        return p;

    QMutableListIterator<QString> it(p);
    while (it.hasNext()) {
        QString path = it.next();
        int id = pathToID.take(path);
        QString x = idToPath.take(id);
        if (x.isEmpty() || x != path)
            continue;

        ::close(id < 0 ? -id : id);

        it.remove();
        if (id < 0) {
            // when removing a directory, we also need to iterate the contents
            // of the directory, and remove watches on all files that aren't
            // explicitly being monitored for changes.
            QDirIterator di(path);

            while (di.hasNext()) {
                const QString &subPath = di.next();
                QFileInfo fi(subPath);
                if (fi.isFile() && !reallyWatchedPaths.contains(subPath))
                    removePaths(QStringList() << subPath, files, directories);
            }

            directories->removeAll(path);
        } else {
            files->removeAll(path);
        }

        reallyWatchedPaths.remove(path);
    }
    isEmpty = pathToID.isEmpty();

    return p;
}

void QKqueueFileSystemWatcherEngine::readFromKqueue()
{
    forever {
        DEBUG() << "QKqueueFileSystemWatcherEngine: polling for changes";
        int r;
        struct kevent kev;
        struct timespec ts = { 0, 0 }; // 0 ts, because we want to poll
        EINTR_LOOP(r, kevent(kqfd, 0, 0, &kev, 1, &ts));
        if (r < 0) {
            perror("QKqueueFileSystemWatcherEngine: error during kevent wait");
            return;
        } else if (r == 0) {
            // polling returned no events, so stop
            break;
        } else {
            int fd = kev.ident;

            DEBUG() << "QKqueueFileSystemWatcherEngine: processing kevent" << kev.ident << kev.filter;

            int id = fd;
            QString path = idToPath.value(id);
            if (path.isEmpty()) {
                // perhaps a directory?
                id = -id;
                path = idToPath.value(id);
                if (path.isEmpty()) {
                    DEBUG() << "QKqueueFileSystemWatcherEngine: received a kevent for a file we're not watching";
                    continue;
                }
            }
            if (kev.filter != EVFILT_VNODE) {
                DEBUG() << "QKqueueFileSystemWatcherEngine: received a kevent with the wrong filter";
                continue;
            }

            if ((kev.fflags & (NOTE_DELETE | NOTE_REVOKE | NOTE_RENAME)) != 0) {
                DEBUG() << path << "removed, removing watch also";

                pathToID.remove(path);
                idToPath.remove(id);
                ::close(fd);

                if (reallyWatchedPaths.contains(path)) {
                    qDebug("Emitting removed for really watched path %s",
                            qPrintable(path));
                    if (id < 0)
                        emit directoryChanged(path, true);
                    else
                        emit fileChanged(path, true);
                }
            } else {
                DEBUG() << path << "changed";

                if (reallyWatchedPaths.contains(path)) {
                    qDebug("Emitting changed for really watched path %s",
                            qPrintable(path));
                    if (id < 0)
                        emit directoryChanged(path, false);
                    else
                        emit fileChanged(path, false);
                }

                QFileInfo fi(path);
                const QString &parentDirectory = fi.path();
                if (reallyWatchedPaths.contains(parentDirectory)) {
                    // if we're watching the parent directory instead (or as
                    // well), then we need to emit a directoryChanged
                    // notification for it.
                    qDebug("Emitting changed for parent directory of %s",
                            qPrintable(path));
                    emit directoryChanged(parentDirectory, false);
                }
            }
        }

    }
}

#endif //QT_NO_FILESYSTEMWATCHER

QT_END_NAMESPACE
