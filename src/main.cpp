#include <QCoreApplication>
#include "saesucloudstorageadvertiser.h"

int main(int argc, char **argv)
{
    QCoreApplication a(argc, argv);

    SaesuCloudStorageAdvertiser storageAdvertiser;
    a.exec();
}
