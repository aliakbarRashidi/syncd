QT += network
TEMPLATE = app
TARGET = saesu-syncd
DEPENDPATH += .

MOC_DIR = ./.moc/
OBJECTS_DIR = ./.obj/

# Input
SOURCES += src/main.cpp \
    src/syncadvertiser.cpp \
    src/syncmanagersynchroniser.cpp \
    src/syncmanager.cpp

HEADERS += src/syncadvertiser.h \
    src/syncmanagersynchroniser.h \
    src/syncmanager.h

CONFIG += link_pkgconfig
PKGCONFIG += saesu
