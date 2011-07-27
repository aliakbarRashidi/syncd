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
