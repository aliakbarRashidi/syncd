#include <QCoreApplication>
#include "syncadvertiser.h"

int main(int argc, char **argv)
{
    QCoreApplication a(argc, argv);

    a.setOrganizationName(QLatin1String("saesu"));
    a.setApplicationName(QLatin1String("syncd"));

    SyncAdvertiser storageAdvertiser;
    a.exec();
}
