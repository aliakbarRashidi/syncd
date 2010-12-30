QT += network
TEMPLATE = app
TARGET = saesu-syncd
DEPENDPATH += .
INCLUDEPATH += . include

MOC_DIR = ./.moc/
OBJECTS_DIR = ./.obj/

# Input
SOURCES += src/main.cpp \
    src/saesucloudstorageadvertiser.cpp \
    src/saesucloudstoragesynchroniser.cpp

HEADERS += include/saesucloudstorageadvertiser.h \
    include/saesucloudstoragesynchroniser.h

CONFIG += link_pkgconfig
PKGCONFIG += saesu
