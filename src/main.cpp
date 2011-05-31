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
