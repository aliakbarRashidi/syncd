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
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QObject>

// Us
#include "filewatcher.h"

Q_GLOBAL_STATIC(FileWatcher, fileWatcherInstance)

FileWatcher::FileWatcher()
     : QObject()
{
    sDebug() << "Constructing";
    sDebug() << "Watching documents";
    watchDirectoryTree(QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation));
    sDebug() << "Watching music";
    watchDirectoryTree(QDesktopServices::storageLocation(QDesktopServices::MusicLocation));
    sDebug() << "Watching movies";
    watchDirectoryTree(QDesktopServices::storageLocation(QDesktopServices::MoviesLocation));
    sDebug() << "Watching pictures";
    watchDirectoryTree(QDesktopServices::storageLocation(QDesktopServices::PicturesLocation));
}

FileWatcher::~FileWatcher()
{
}

FileWatcher *FileWatcher::instance()
{
    return fileWatcherInstance();
}

/*! Recursively watches a given path, and all paths in it.
 */
void FileWatcher::watchDirectoryTree(const QString &path)
{
    const QString &cleanedPath = QDir::cleanPath(path);
    if (mWatcher.directories().contains(cleanedPath))
        return; // don't recurse endlessly

    QDir tmp(path);
    if (!tmp.exists())
        tmp.mkpath(path);

    sDebug() << "Watching " << cleanedPath;
    mWatcher.addPath(cleanedPath);

    QList<CachedFileInfo> fileList;
    QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        const QString &entryName = it.next();
        QFileInfo fileInfo(entryName);

        if (fileInfo.isDir()) {
            const QDir &dir(entryName);
            if (mWatcher.directories().contains(QDir::cleanPath(dir.absolutePath())))
                continue; // don't recurse endlessly

            watchDirectoryTree(dir.absolutePath());
        } else {
            CachedFileInfo fi;
            fi = fileInfo;
            fileList.append(fi);
        }
    }

    qDebug() << fileList.count() << " files in " << path;
}
