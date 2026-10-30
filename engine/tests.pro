TEMPLATE = app
TARGET = test_engine
CONFIG += console c11
CONFIG -= qt app_bundle
include(engine.pri)
SOURCES += test_engine.c
