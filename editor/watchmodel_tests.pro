TEMPLATE = app
TARGET = test_watchmodel
CONFIG += console c++17
CONFIG -= app_bundle
QT += core gui
include(../engine/engine.pri)
INCLUDEPATH += $$PWD
HEADERS += watchmodel.h backend.h
SOURCES += test_watchmodel.cpp watchmodel.cpp