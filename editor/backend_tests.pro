TEMPLATE = app
TARGET = test_backend
QT = core
CONFIG += console c++17 c11
CONFIG -= app_bundle
OBJECTS_DIR = $$OUT_PWD/obj_$$TARGET
include(../engine/engine.pri)
HEADERS += ladder_model.h ladder_compiler.h backend.h simulator.h
SOURCES += ladder_model.cpp ladder_compiler.cpp test_backend.cpp simulator.cpp