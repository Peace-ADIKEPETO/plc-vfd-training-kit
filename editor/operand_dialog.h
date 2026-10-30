// operand_dialog.h - dialog to edit the operands of one instruction
#pragma once
#include "ladder_model.h"
#include <QWidget>

struct Operands {
    uint16_t a = 0;
    uint16_t b = 0;
    uint8_t mode = 0;
};

// Returns true when the user pressed OK with valid values (written to out).
bool askOperands(QWidget *parent, const Node &n, Operands &out);
