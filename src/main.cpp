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

#include <QCoreApplication>
#include <QHostInfo>

#include "bonjourserviceregister.h"
#include "syncadvertiser.h"

int main(int argc, char **argv)
{
    QCoreApplication a(argc, argv);


    BonjourServiceRegister bonjourRegister;
    bonjourRegister.registerService(BonjourRecord(QString("Saesu Synchronisation on %1").arg(QHostInfo::localHostName()),
                                            QLatin1String("_saesu._tcp"), QString()),
                                            1337);

    a.setOrganizationName(QLatin1String("saesu"));
    a.setApplicationName(QLatin1String("syncd"));

    SyncAdvertiser storageAdvertiser;
    a.exec();
}
