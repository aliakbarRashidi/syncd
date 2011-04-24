QT += network
TEMPLATE = app
TARGET = saesu-syncd
DEPENDPATH += .

MOC_DIR = ./.moc/
OBJECTS_DIR = ./.obj/

# Input
SOURCES += src/main.cpp \
    src/saesucloudstorageadvertiser.cpp \
    src/saesucloudstoragesynchroniser.cpp

HEADERS += src/saesucloudstorageadvertiser.h \
    src/saesucloudstoragesynchroniser.h

CONFIG += link_pkgconfig
PKGCONFIG += saesu
