#include <QCoreApplication>
#include "syncadvertiser.h"
#include "syncmanager.h"

int main(int argc, char **argv)
{
    QCoreApplication a(argc, argv);

    a.setOrganizationName(QLatin1String("saesu"));
    a.setApplicationName(QLatin1String("syncd"));
    
    SyncManager::instance();
    sleep(5); // XXX: arbitrary delay to make sure the cloud is loaded

    SyncAdvertiser storageAdvertiser;
    a.exec();
}
