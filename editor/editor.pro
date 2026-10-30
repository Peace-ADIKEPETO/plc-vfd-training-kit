QT += widgets
TEMPLATE = app
TARGET = ladder_editor
CONFIG += c++17 c11
include(../engine/engine.pri)
HEADERS += ladder_model.h ladder_compiler.h editor_core.h operand_dialog.h ladder_scene.h mainwindow.h
SOURCES += ladder_model.cpp ladder_compiler.cpp editor_core.cpp operand_dialog.cpp ladder_scene.cpp mainwindow.cpp main.cpp
OBJECTS_DIR = $$OUT_PWD/obj_$$TARGET
