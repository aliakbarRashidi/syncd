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
