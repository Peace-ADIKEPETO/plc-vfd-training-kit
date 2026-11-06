TEMPLATE = app
TARGET = test_cexport
CONFIG += console c++17
CONFIG -= app_bundle
QT += core
HEADERS += c_export.h
SOURCES += test_cexport.cpp c_export.cpp