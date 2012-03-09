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
#include <QDebug>


#define sDebug qDebug



// Qt
#include <QFileSystemWatcher>
#include <QObject>
#include <QString>
#include <QFileInfo>
#include <QDateTime>
#include <QHash>
#include <QList>

struct CachedFileInfo
{
    QString fileName;
    QDateTime lastModified;
    qint64 size;

    CachedFileInfo &operator=(const QFileInfo &fileInfo)
    {
        fileName = fileInfo.fileName();
        lastModified = fileInfo.lastModified();
        size = fileInfo.size();
        return *this;
    }
};

inline QDebug operator<<(QDebug debug, const CachedFileInfo& fileInfo)
{
    debug.nospace() << "CachedFileInfo(" 
        << fileInfo.fileName << "," 
        << fileInfo.lastModified << ","
        << fileInfo.size << ")";
    return debug.space();
}

struct CachedDirInfo
{
public:
    CachedDirInfo(const QString &path)
        : mPath(path)
    {
    }

    const CachedFileInfo *file(const QString &fileName)
    {

        return NULL;
    }

    void syncPath()
    {
        sDebug() << "Syncing " << mPath;
        QVector<CachedFileInfo> newDirectoryContents;

        // build list of new items
        QDirIterator it(mPath, QDir::AllEntries | QDir::NoDotAndDotDot);

        while (it.hasNext()) {
            const QString &entryName = it.next();
            QFileInfo fileInfo(entryName);

            if (!fileInfo.isDir()) {
                CachedFileInfo fi;
                fi = fileInfo;
                newDirectoryContents.append(fi);
            }
        }

        // difference the contents
        for (int i = 0; i < newDirectoryContents.count(); ++i) {
            const CachedFileInfo &newFile = newDirectoryContents.at(i);
            const CachedFileInfo *oldFile = NULL;

            // find the old file
            for (int j = 0; j < mDirectoryContents.count(); ++j) {
                const CachedFileInfo &file = mDirectoryContents.at(j);
                if (file.fileName == newFile.fileName) {
                    oldFile = &file;
                    break;
                }
            }

            if (!oldFile) {
                sDebug() << "New file: " << newFile.fileName;
            } else {
                if (newFile.lastModified != oldFile->lastModified ||
                    newFile.size != oldFile->size) {
                    sDebug() << "File modified: " << newFile.fileName;
                }
            }
        }

        // TODO: track deletion
        for (int i = 0; i < mDirectoryContents.count(); ++i) {
            const CachedFileInfo &oldFile = mDirectoryContents.at(i);
            const CachedFileInfo *newFile = NULL;

            // find the new file
            for (int j = 0; j < newDirectoryContents.count(); ++j) {
                const CachedFileInfo &file = newDirectoryContents.at(j);
                if (file.fileName == oldFile.fileName) {
                    newFile = &file;
                    break;
                }
            }

            if (!newFile) {
                sDebug() << "File deleted: " << oldFile.fileName;
            }
        }

        sDebug() << "Monitor cache:" << newDirectoryContents;

        // replace with new item hash
        mDirectoryContents = newDirectoryContents;
    }

private:
    QVector<CachedFileInfo> mDirectoryContents;
    QString mPath;
};

class FileWatcher : public QObject
{
    Q_OBJECT

public:
    explicit FileWatcher();
    virtual ~FileWatcher();

    static FileWatcher *instance();

private slots:
    void watchDirectoryTree(const QString &path);

private:
    // QString dirPath, list of info on files in the dir
    QHash<QString, CachedDirInfo> mWatchedPaths;

    QFileSystemWatcher mWatcher;
};



// Us
#include "filewatcher.h"

Q_GLOBAL_STATIC(FileWatcher, fileWatcherInstance)

FileWatcher::FileWatcher()
     : QObject()
{
    connect(&mWatcher, SIGNAL(directoryChanged(QString)), SLOT(watchDirectoryTree(QString)));
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
//    if (mWatcher.directories().contains(cleanedPath))
//        return; // don't recurse endlessly

    QDir tmp(path);
    if (!tmp.exists())
        tmp.mkpath(path);

    sDebug() << "Watching " << cleanedPath;
    mWatcher.addPath(cleanedPath);

    QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot);
    while (it.hasNext()) {
        const QString &entryName = it.next();
        QFileInfo fileInfo(entryName);

        if (fileInfo.isDir()) {
            const QDir &dir(entryName);
            if (mWatcher.directories().contains(QDir::cleanPath(dir.absolutePath())))
                continue; // don't recurse endlessly

            watchDirectoryTree(dir.absolutePath());
        }
    }

    QHash<QString, CachedDirInfo>::iterator dit = mWatchedPaths.find(path);
    CachedDirInfo dirInfo = dit == mWatchedPaths.end() ? CachedDirInfo(path) : *dit;
    dirInfo.syncPath();
    mWatchedPaths.insert(path, dirInfo);
}






#include <QCoreApplication>
#include <QDateTime>

#include "filewatcher.h"

int main(int argc, char **argv)
{
    QCoreApplication a(argc, argv);

    qsrand((uint)QDateTime::currentMSecsSinceEpoch());

    a.setOrganizationName(QLatin1String("saesu"));
    a.setApplicationName(QLatin1String("syncd"));

    FileWatcher::instance();

    a.exec();
}


#include "filewatcher.moc"
