TEMPLATE = app
TARGET = test_edit
QT = core
CONFIG += console c++17 c11
CONFIG -= app_bundle
include(../engine/engine.pri)
HEADERS += ladder_model.h ladder_compiler.h editor_core.h
SOURCES += ladder_model.cpp ladder_compiler.cpp editor_core.cpp test_edit.cpp
OBJECTS_DIR = $$OUT_PWD/obj_$$TARGET
