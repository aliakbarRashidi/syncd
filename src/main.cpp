#include <QCoreApplication>
#include "saesucloudstorageadvertiser.h"

int main(int argc, char **argv)
{
    QCoreApplication a(argc, argv);

    a.setOrganizationName(QLatin1String("saesu"));
    a.setApplicationName(QLatin1String("syncd"));

    SaesuCloudStorageAdvertiser storageAdvertiser;
    a.exec();
}
