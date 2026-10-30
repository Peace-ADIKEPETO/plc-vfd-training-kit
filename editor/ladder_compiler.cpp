#include "ladder_compiler.h"
extern "C" {
#include "ladder_engine.h"
}

namespace {

struct Ctx {
    QByteArray ins;
    int instr = 0;
    int nextId = 0;
    QString err;
    int rung = 0;
};

void put(Ctx &c, uint8_t op, int id, uint8_t mode, uint16_t a, uint16_t b)
{
    const char bytes[LD_INSTR_SIZE] = {
        static_cast<char>(op), static_cast<char>(id < 0 ? LD_ELEM_NONE : id), static_cast<char>(mode), 0,
        static_cast<char>(a & 0xFF), static_cast<char>(a >> 8), static_cast<char>(b & 0xFF), static_cast<char>(b >> 8)
    };
    c.ins.append(bytes, LD_INSTR_SIZE);
    c.instr++;
}

bool fail(Ctx &c, const QString &msg)
{
    if (c.err.isEmpty()) c.err = QString("Rung %1: %2").arg(c.rung + 1).arg(msg);
    return false;
}

bool compileElement(Ctx &c, Node *n)
{
    if (c.nextId >= LD_ELEM_NONE) return fail(c, "too many elements (maximum 255)");
    n->id = c.nextId++;
    put(c, n->op, n->id, n->mode, n->a, n->b);
    return true;
}

bool compileNode(Ctx &c, Node *n, int depth)
{
    switch (n->kind) {
    case Node::Element:
        if (n->isOutput()) return fail(c, "outputs (OTE, timers, counters, RES, MOV) must be at the end of the rung, not inside a branch");
        return compileElement(c, n);
    case Node::Series:
        for (auto &k : n->kids)
            if (!compileNode(c, k.get(), depth)) return false;
        return true;
    case Node::Parallel:
        if (depth + 1 > LD_MAX_BRANCH) return fail(c, "branches nested too deeply (maximum 4)");
        put(c, LD_OP_BR_OPEN, -1, 0, 0, 0);
        for (size_t i = 0; i < n->kids.size(); i++) {
            if (i > 0) put(c, LD_OP_BR_NEXT, -1, 0, 0, 0);
            if (!compileNode(c, n->kids[i].get(), depth + 1)) return false;
        }
        put(c, LD_OP_BR_CLOSE, -1, 0, 0, 0);
        return true;
    }
    return false;
}

} // namespace

CompileResult compileProgram(Program &p)
{
    Ctx c;
    for (c.rung = 0; c.rung < static_cast<int>(p.rungs.size()); c.rung++) {
        Node *root = p.rungs[c.rung].root.get();
        put(c, LD_OP_RUNG_BEGIN, -1, 0, 0, 0);
        bool seenOutput = false;
        for (auto &k : root->kids) {
            if (k->isOutput()) {
                seenOutput = true;
                if (!compileElement(c, k.get())) break;
            } else {
                if (seenOutput) { fail(c, "conditions must come before the outputs"); break; }
                if (!compileNode(c, k.get(), 0)) break;
            }
        }
        if (!c.err.isEmpty()) break;
    }

    CompileResult r;
    if (!c.err.isEmpty()) { r.error = c.err; return r; }
    if (c.instr > LD_MAX_INSTR) {
        r.error = QString("Program too long: %1 instructions (maximum %2)").arg(c.instr).arg(LD_MAX_INSTR);
        return r;
    }
    const uint16_t crc = ld_crc16(reinterpret_cast<const uint8_t *>(c.ins.constData()), static_cast<size_t>(c.ins.size()));
    QByteArray img;
    img.append(static_cast<char>(LD_MAGIC0)); img.append(static_cast<char>(LD_MAGIC1));
    img.append(static_cast<char>(LD_VERSION)); img.append(char(0));
    img.append(static_cast<char>(c.instr & 0xFF)); img.append(static_cast<char>(c.instr >> 8));
    img.append(static_cast<char>(crc & 0xFF)); img.append(static_cast<char>(crc >> 8));
    img.append(c.ins);
    r.image = img;
    r.instrCount = c.instr;
    r.elemCount = c.nextId;
    return r;
}
