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

#ifndef FILEWATCHER_H
#define FILEWATCHER_H

// Qt
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QFileInfo>

// saesu
#include "sglobal.h"


struct CachedFileInfo
{
    QString fileName;
    QDateTime lastModified;

    CachedFileInfo &operator=(const QFileInfo &fileInfo)
    {
        fileName = fileInfo.fileName();
        lastModified = fileInfo.lastModified();
        return *this;
    }

    bool operator!=(const QFileInfo &fileInfo) const
    {
        return lastModified != fileInfo.lastModified() ||
                fileName != fileInfo.fileName();
    }

    bool operator==(const QFileInfo &fileInfo) const
    {
        return lastModified == fileInfo.lastModified() &&
               fileName == fileInfo.fileName();
    }
};

class FileWatcher : public QObject
{
public:
    explicit FileWatcher();
    virtual ~FileWatcher();

    static FileWatcher *instance();

private:
    void watchDirectoryTree(const QString &path);

    // QString dirPath, list of info on files in the dir
    QHash<QString, QList<CachedFileInfo> > mDirectoryContentsHash;

    QFileSystemWatcher mWatcher;
};

#endif // FILEWATCHER_H
