QT += network
TEMPLATE = app
TARGET = saesu-syncd
DEPENDPATH += .

unix:!mac {
    LIBS += -ldns_sd
}

MOC_DIR = ./.moc/
OBJECTS_DIR = ./.obj/

# Input
SOURCES += src/main.cpp \
    src/syncadvertiser.cpp \
    src/syncmanagersynchroniser.cpp \
    src/syncmanager.cpp \
    src/filewatcher.cpp

HEADERS += src/syncadvertiser.h \
    src/syncmanagersynchroniser.h \
    src/syncmanager.h \
    src/filewatcher.h

CONFIG += link_pkgconfig
PKGCONFIG += saesu
