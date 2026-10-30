TEMPLATE = app
TARGET = test_compiler
QT = core
CONFIG += console c++17 c11
CONFIG -= app_bundle
include(../engine/engine.pri)
HEADERS += ladder_model.h ladder_compiler.h
SOURCES += ladder_model.cpp ladder_compiler.cpp test_compiler.cpp
OBJECTS_DIR = $$OUT_PWD/obj_$$TARGET
