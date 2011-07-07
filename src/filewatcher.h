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

// saesu
#include "sglobal.h"

class FileWatcher : public QObject
{
public:
    explicit FileWatcher();
    virtual ~FileWatcher();

    static FileWatcher *instance();

private:
    void watchDirectoryTree(const QString &path);

    QFileSystemWatcher mWatcher;
};

#endif // FILEWATCHER_H
