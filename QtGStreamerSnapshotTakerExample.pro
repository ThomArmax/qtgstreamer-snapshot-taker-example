#-------------------------------------------------
#
# Project created by QtCreator 2016-03-02T19:17:53
#
#-------------------------------------------------

QT += core gui widgets
CONFIG += no_keywords
CONFIG += link_pkgconfig
PKGCONFIG += Qt5GStreamer-1.0 gstreamer-1.0 gstreamer-video-1.0

TARGET = QtGStreamerSnapshotTakerExample
TEMPLATE = app

SOURCES += main.cpp\
        mainwindow.cpp \
        player.cpp

HEADERS += mainwindow.h \
        player.h

FORMS += mainwindow.ui
