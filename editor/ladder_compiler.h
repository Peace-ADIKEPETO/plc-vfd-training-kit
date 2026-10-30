// ladder_compiler.h - turns a Program into the bytecode image described in engine/SPEC.md
#pragma once
#include "ladder_model.h"
#include <QByteArray>
#include <QString>

struct CompileResult {
    QByteArray image;          // header + instructions, ready for ld_load() or Modbus upload
    QString error;             // empty when the compile succeeded
    int instrCount = 0;
    int elemCount = 0;
    bool ok() const { return error.isEmpty(); }
};

// Assigns an animation id to every element (node->id) and compiles all rungs.
CompileResult compileProgram(Program &p);
