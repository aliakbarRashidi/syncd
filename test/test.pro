######################################################################
# Automatically generated by qmake (2.01a) Wed Mar 7 15:01:37 2012
######################################################################

TEMPLATE = app
TARGET = 
DEPENDPATH += . qfsw/ qfsw/private/
INCLUDEPATH += . qfsw/ qfsw/private/

# Input
HEADERS += filewatcher.h
SOURCES += filewatcher.cpp main.cpp


#qfilesystemwatcher.cpp          qfilesystemwatcher_inotify.cpp
#qfilesystemwatcher_kqueue.cpp   qfilesystemwatcher_p.h
##qfilesystemwatcher_polling_p.h  qfilesystemwatcher_win_p.h                                    
#qfilesystemwatcher.h            qfilesystemwatcher_inotify_p.h
#qfilesystemwatcher_kqueue_p.h   qfilesystemwatcher_polling.cpp
#qfilesystemwatcher_win.cpp                                                                  

SOURCES += qfsw/qfilesystemwatcher.cpp qfsw/qfilesystemwatcher_polling.cpp
HEADERS += qfsw/qfilesystemwatcher.h qfsw/qfilesystemwatcher_p.h qfsw/qfilesystemwatcher_polling_p.h

macx-* {
    SOURCES += qfsw/qfilesystemwatcher_kqueue.cpp
    HEADERS += qfsw/qfilesystemwatcher_kqueue_p.h
}
