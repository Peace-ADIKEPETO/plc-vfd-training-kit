#include "ladder_model.h"
extern "C" {
#include "ladder_engine.h"
}

std::unique_ptr<Node> Node::element(uint8_t op, uint16_t a, uint16_t b, uint8_t mode)
{
    auto n = std::make_unique<Node>();
    n->kind = Element; n->op = op; n->a = a; n->b = b; n->mode = mode;
    return n;
}
std::unique_ptr<Node> Node::series()   { auto n = std::make_unique<Node>(); n->kind = Series;   return n; }
std::unique_ptr<Node> Node::parallel() { auto n = std::make_unique<Node>(); n->kind = Parallel; return n; }

Node *Node::add(std::unique_ptr<Node> n)
{
    kids.push_back(std::move(n));
    return kids.back().get();
}

bool Node::isOutput() const
{
    if (kind != Element) return false;
    switch (op) {
    case LD_OP_OTE: case LD_OP_TON: case LD_OP_TOF: case LD_OP_TONR:
    case LD_OP_CTU: case LD_OP_RES: case LD_OP_MOV: return true;
    default: return false;
    }
}

QString bitName(uint16_t a)
{
    if (a < 0x0100) return QString("I%1").arg(a - LD_BIT_I);
    if (a < 0x0200) return QString("Q%1").arg(a - LD_BIT_Q);
    if (a < 0x0300) return QString("M%1").arg(a - LD_BIT_M);
    if (a < 0x0310) return QString("T%1.DN").arg(a - LD_BIT_TDN);
    if (a < 0x0320) return QString("C%1.DN").arg(a - LD_BIT_CDN);
    if (a < 0x0410) return QString("VST%1").arg(a - LD_BIT_VFD_ST);
    return QString("VCT%1").arg(a - LD_BIT_VFD_CT);
}

QString regName(uint16_t a)
{
    if (a >= LD_REG_D && a < LD_REG_D + 16) return QString("D%1").arg(a - LD_REG_D);
    if (a >= LD_REG_T && a < LD_REG_T + 16) return QString("T%1.ACC").arg(a - LD_REG_T);
    if (a >= LD_REG_C && a < LD_REG_C + 16) return QString("C%1.ACC").arg(a - LD_REG_C);
    if (a >= LD_REG_VFD_H && a < LD_REG_VFD_I) return QString("VH%1").arg(a - LD_REG_VFD_H);
    if (a >= LD_REG_VFD_I) return QString("VI%1").arg(a - LD_REG_VFD_I);
    return QString("R%1").arg(a, 4, 16, QChar('0'));
}

QString mnemonic(uint8_t op)
{
    switch (op) {
    case LD_OP_XIC: return "XIC";   case LD_OP_XIO: return "XIO";   case LD_OP_OTE: return "OTE";
    case LD_OP_TON: return "TON";   case LD_OP_TOF: return "TOF";   case LD_OP_TONR: return "TONR";
    case LD_OP_CTU: return "CTU";   case LD_OP_RES: return "RES";
    case LD_OP_GRT: return "GRT";   case LD_OP_LES: return "LES";   case LD_OP_EQU: return "EQU";
    case LD_OP_MOV: return "MOV";
    default: return "?";
    }
}

static QString operand(uint16_t v, bool isConst)
{
    return isConst ? QString::number(static_cast<int16_t>(v)) : regName(v);
}

QString detailText(const Node &n)
{
    switch (n.op) {
    case LD_OP_XIC: case LD_OP_XIO: case LD_OP_OTE: return bitName(n.a);
    case LD_OP_TON: case LD_OP_TOF: case LD_OP_TONR: return QString("T%1  PRE %2").arg(n.a).arg(n.b);
    case LD_OP_CTU: return QString("C%1  PRE %2").arg(n.a).arg(n.b);
    case LD_OP_RES: return QString("%1%2").arg((n.mode & LD_MODE_RES_CTR) ? "C" : "T").arg(n.a);
    case LD_OP_GRT: case LD_OP_LES: case LD_OP_EQU: {
        const char *sym = n.op == LD_OP_GRT ? " > " : n.op == LD_OP_LES ? " < " : " = ";
        return operand(n.a, n.mode & LD_MODE_A_CONST) + sym + operand(n.b, n.mode & LD_MODE_B_CONST);
    }
    case LD_OP_MOV: return operand(n.a, n.mode & LD_MODE_A_CONST) + " -> " + regName(n.b);
    default: return QString();
    }
}

Program makeDemoProgram()
{
    Program p;
    auto el = [](uint8_t op, uint16_t a, uint16_t b = 0, uint8_t mode = 0) { return Node::element(op, a, b, mode); };

    Rung r1; r1.comment = "Start / stop latch (I0 = start, I1 = stop)";
    Node *par = r1.root->add(Node::parallel());
    par->add(Node::series())->add(el(LD_OP_XIC, LD_BIT_I + 0));
    par->add(Node::series())->add(el(LD_OP_XIC, LD_BIT_Q + 0));
    r1.root->add(el(LD_OP_XIO, LD_BIT_I + 1));
    r1.root->add(el(LD_OP_OTE, LD_BIT_Q + 0));
    p.rungs.push_back(std::move(r1));

    Rung r2; r2.comment = "Timer: runs while Q0 is on (100 scans = 1 s)";
    r2.root->add(el(LD_OP_XIC, LD_BIT_Q + 0));
    r2.root->add(el(LD_OP_TON, 0, 100));
    p.rungs.push_back(std::move(r2));

    Rung r3; r3.comment = "When the timer is done, Q1 turns on";
    r3.root->add(el(LD_OP_XIC, LD_BIT_TDN + 0));
    r3.root->add(el(LD_OP_OTE, LD_BIT_Q + 1));
    p.rungs.push_back(std::move(r3));

    Rung r4; r4.comment = "I2 loads 1500 into D0";
    r4.root->add(el(LD_OP_XIC, LD_BIT_I + 2));
    r4.root->add(el(LD_OP_MOV, 1500, LD_REG_D + 0, LD_MODE_A_CONST));
    p.rungs.push_back(std::move(r4));

    Rung r5; r5.comment = "D0 > 1000 turns on Q2";
    r5.root->add(el(LD_OP_GRT, LD_REG_D + 0, 1000, LD_MODE_B_CONST));
    r5.root->add(el(LD_OP_OTE, LD_BIT_Q + 2));
    p.rungs.push_back(std::move(r5));
    return p;
}

bool isKnownOp(int op)
{
    switch (op) {
    case LD_OP_XIC: case LD_OP_XIO: case LD_OP_OTE: case LD_OP_TON: case LD_OP_TOF: case LD_OP_TONR:
    case LD_OP_CTU: case LD_OP_RES: case LD_OP_GRT: case LD_OP_LES: case LD_OP_EQU: case LD_OP_MOV:
        return true;
    default: return false;
    }
}

// "prefix N suffix" with 0 <= N <= maxIdx
static bool matchIndexed(const QString &s, const QString &prefix, const QString &suffix, int maxIdx, int &n)
{
    if (!s.startsWith(prefix) || !s.endsWith(suffix) || s.size() <= prefix.size() + suffix.size()) return false;
    const QString mid = s.mid(prefix.size(), s.size() - prefix.size() - suffix.size());
    for (QChar ch : mid) if (!ch.isDigit()) return false;
    bool ok = false;
    n = mid.toInt(&ok);
    return ok && n >= 0 && n <= maxIdx;
}

bool parseBit(const QString &text, uint16_t &addr)
{
    const QString s = text.trimmed().toUpper();
    int n = 0;
    if (matchIndexed(s, "VST", "", 7, n))   { addr = static_cast<uint16_t>(LD_BIT_VFD_ST + n); return true; }
    if (matchIndexed(s, "VCT", "", 2, n))   { addr = static_cast<uint16_t>(LD_BIT_VFD_CT + n); return true; }
    if (matchIndexed(s, "T", ".DN", LD_NUM_TIMERS - 1, n))   { addr = static_cast<uint16_t>(LD_BIT_TDN + n); return true; }
    if (matchIndexed(s, "C", ".DN", LD_NUM_COUNTERS - 1, n)) { addr = static_cast<uint16_t>(LD_BIT_CDN + n); return true; }
    if (matchIndexed(s, "I", "", 7, n))     { addr = static_cast<uint16_t>(LD_BIT_I + n); return true; }
    if (matchIndexed(s, "Q", "", 5, n))     { addr = static_cast<uint16_t>(LD_BIT_Q + n); return true; }
    if (matchIndexed(s, "M", "", 63, n))    { addr = static_cast<uint16_t>(LD_BIT_M + n); return true; }
    return false;
}

bool parseRegister(const QString &text, bool writable, uint16_t &addr)
{
    const QString s = text.trimmed().toUpper();
    int n = 0;
    if (matchIndexed(s, "D", "", 15, n))    { addr = static_cast<uint16_t>(LD_REG_D + n); return true; }
    if (matchIndexed(s, "VH", "", 0x12, n)) { addr = static_cast<uint16_t>(LD_REG_VFD_H + n); return true; }
    if (writable) return false;
    if (matchIndexed(s, "T", ".ACC", 15, n)) { addr = static_cast<uint16_t>(LD_REG_T + n); return true; }
    if (matchIndexed(s, "C", ".ACC", 15, n)) { addr = static_cast<uint16_t>(LD_REG_C + n); return true; }
    if (matchIndexed(s, "VI", "", 8, n))     { addr = static_cast<uint16_t>(LD_REG_VFD_I + n); return true; }
    return false;
}

bool parseValue(const QString &text, bool allowConst, uint16_t &v, bool &isConst)
{
    const QString s = text.trimmed();
    bool ok = false;
    const int num = s.toInt(&ok, 0);
    if (ok) {
        if (!allowConst || num < -32768 || num > 32767) return false;
        v = static_cast<uint16_t>(num & 0xFFFF);
        isConst = true;
        return true;
    }
    isConst = false;
    return parseRegister(s, false, v);
}

bool parseIndex(const QString &text, QChar prefix, int count, int &idx)
{
    QString s = text.trimmed().toUpper();
    if (!s.isEmpty() && s[0] == prefix.toUpper()) s = s.mid(1);
    for (QChar ch : s) if (!ch.isDigit()) return false;
    bool ok = false;
    idx = s.toInt(&ok);
    return ok && idx >= 0 && idx < count;
}

QString validateOperands(uint8_t op, uint16_t a, uint16_t b, uint8_t mode)
{
    switch (op) {
    case LD_OP_XIC: case LD_OP_XIO: case LD_OP_OTE:
        return a < LD_BIT_SPACE ? QString() : QString("Bit address out of range");
    case LD_OP_TON: case LD_OP_TOF: case LD_OP_TONR: case LD_OP_CTU: case LD_OP_RES:
        return a < 16 ? QString() : QString("Timer or counter index out of range (0-15)");
    case LD_OP_GRT: case LD_OP_LES: case LD_OP_EQU:
        if (!(mode & LD_MODE_A_CONST) && a >= LD_REG_SPACE) return "Register address out of range";
        if (!(mode & LD_MODE_B_CONST) && b >= LD_REG_SPACE) return "Register address out of range";
        return QString();
    case LD_OP_MOV:
        if (!(mode & LD_MODE_A_CONST) && a >= LD_REG_SPACE) return "Source register out of range";
        return b < LD_REG_SPACE ? QString() : QString("Destination register out of range");
    default: return "Unknown instruction";
    }
}
