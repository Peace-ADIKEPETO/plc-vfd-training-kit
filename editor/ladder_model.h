// ladder_model.h - in-memory ladder program: rungs made of series elements and parallel branches
#pragma once
#include <QString>
#include <cstdint>
#include <memory>
#include <vector>

struct Node {
    enum Kind { Element, Series, Parallel };
    Kind kind = Series;
    uint8_t op = 0;                 // LD_OP_* (elements only)
    uint8_t mode = 0;               // LD_MODE_* flags
    uint16_t a = 0;
    uint16_t b = 0;
    int id = -1;                    // animation id, assigned by the compiler
    std::vector<std::unique_ptr<Node>> kids;

    static std::unique_ptr<Node> element(uint8_t op, uint16_t a, uint16_t b = 0, uint8_t mode = 0);
    static std::unique_ptr<Node> series();
    static std::unique_ptr<Node> parallel();
    Node *add(std::unique_ptr<Node> n);   // returns the node that was added
    bool isOutput() const;                // OTE, TON, TOF, TONR, CTU, RES, MOV
};

struct Rung {
    QString comment;
    std::unique_ptr<Node> root = Node::series();
};

struct Program {
    std::vector<Rung> rungs;
};

QString bitName(uint16_t addr);        // 0x0100 -> "Q0"
QString regName(uint16_t addr);        // 0x0010 -> "D0"
QString mnemonic(uint8_t op);          // LD_OP_XIC -> "XIC"
QString detailText(const Node &n);     // operands, as shown under the symbol
Program makeDemoProgram();             // 5 rungs used by the tests

bool isKnownOp(int op);
// Text -> address. Return false when the text is not valid.
bool parseBit(const QString &s, uint16_t &addr);                       // I0, Q3, M5, T0.DN, C1.DN, VST0, VCT0
bool parseRegister(const QString &s, bool writable, uint16_t &addr);   // D0, T0.ACC, C0.ACC, VH0, VI0
bool parseValue(const QString &s, bool allowConst, uint16_t &v, bool &isConst);  // register name or number
bool parseIndex(const QString &s, QChar prefix, int count, int &idx);  // "T3" or "3"
// Empty string when the operands are valid for the op, otherwise the reason.
QString validateOperands(uint8_t op, uint16_t a, uint16_t b, uint8_t mode);
