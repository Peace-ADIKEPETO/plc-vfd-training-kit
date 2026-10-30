#include "operand_dialog.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
extern "C" {
#include "ladder_engine.h"
}

namespace {

struct FieldDef {
    QString labelA, labelB;        // labelB empty = only one field
    QString textA, textB;
};

QString operandText(uint16_t v, bool isConst)
{
    return isConst ? QString::number(static_cast<int16_t>(v)) : regName(v);
}

FieldDef fieldsFor(const Node &n)
{
    FieldDef f;
    switch (n.op) {
    case LD_OP_XIC: case LD_OP_XIO:
        f.labelA = "Bit (I0-I7, Q0-Q5, M0-M63, T0.DN, C0.DN, VST0-7, VCT0-2)"; f.textA = bitName(n.a); break;
    case LD_OP_OTE:
        f.labelA = "Output bit (Q0-Q5, M0-M63, VCT0-2)"; f.textA = bitName(n.a); break;
    case LD_OP_TON: case LD_OP_TOF: case LD_OP_TONR:
        f.labelA = "Timer (T0-T15)"; f.textA = QString("T%1").arg(n.a);
        f.labelB = "Preset (scans of 10 ms, 0-65535)"; f.textB = QString::number(n.b); break;
    case LD_OP_CTU:
        f.labelA = "Counter (C0-C15)"; f.textA = QString("C%1").arg(n.a);
        f.labelB = "Preset (0-65535)"; f.textB = QString::number(n.b); break;
    case LD_OP_RES:
        f.labelA = "Timer or counter (T0-T15, C0-C15)";
        f.textA = QString("%1%2").arg((n.mode & LD_MODE_RES_CTR) ? "C" : "T").arg(n.a); break;
    case LD_OP_GRT: case LD_OP_LES: case LD_OP_EQU:
        f.labelA = "Value A (register or number)"; f.textA = operandText(n.a, n.mode & LD_MODE_A_CONST);
        f.labelB = "Value B (register or number)"; f.textB = operandText(n.b, n.mode & LD_MODE_B_CONST); break;
    case LD_OP_MOV:
        f.labelA = "Source (register or number)"; f.textA = operandText(n.a, n.mode & LD_MODE_A_CONST);
        f.labelB = "Destination (D0-D15, VH0-VH18)"; f.textB = regName(n.b); break;
    }
    return f;
}

// Returns an empty string when the texts are valid, otherwise the reason.
QString parseFields(uint8_t op, const QString &ta, const QString &tb, Operands &o)
{
    o = Operands();
    int idx = 0;
    bool constFlag = false;
    switch (op) {
    case LD_OP_XIC: case LD_OP_XIO:
        return parseBit(ta, o.a) ? QString() : QString("Not a valid bit name");
    case LD_OP_OTE: {
        if (!parseBit(ta, o.a)) return "Not a valid bit name";
        const bool writable = (o.a >= LD_BIT_Q && o.a < LD_BIT_Q + 6) || (o.a >= LD_BIT_M && o.a < LD_BIT_M + 64) ||
                              (o.a >= LD_BIT_VFD_CT && o.a < LD_BIT_VFD_CT + 3);
        return writable ? QString() : QString("An output must be Q0-Q5, M0-M63 or VCT0-2");
    }
    case LD_OP_TON: case LD_OP_TOF: case LD_OP_TONR: case LD_OP_CTU: {
        const bool timer = op != LD_OP_CTU;
        if (!parseIndex(ta, timer ? 'T' : 'C', 16, idx)) return timer ? "Timer must be T0-T15" : "Counter must be C0-C15";
        bool ok = false;
        const int pre = tb.trimmed().toInt(&ok);
        if (!ok || pre < 0 || pre > 65535) return "Preset must be a number from 0 to 65535";
        o.a = static_cast<uint16_t>(idx); o.b = static_cast<uint16_t>(pre);
        return QString();
    }
    case LD_OP_RES: {
        const QString s = ta.trimmed().toUpper();
        const bool ctr = s.startsWith('C');
        if (!parseIndex(s, ctr ? 'C' : 'T', 16, idx)) return "Use T0-T15 or C0-C15";
        o.a = static_cast<uint16_t>(idx);
        o.mode = ctr ? LD_MODE_RES_CTR : 0;
        return QString();
    }
    case LD_OP_GRT: case LD_OP_LES: case LD_OP_EQU: {
        bool ca = false, cb = false;
        if (!parseValue(ta, true, o.a, ca)) return "Value A: a register name (D0, VH1...) or a number from -32768 to 32767";
        if (!parseValue(tb, true, o.b, cb)) return "Value B: a register name (D0, VH1...) or a number from -32768 to 32767";
        if (ca && cb) return "At least one value must be a register";
        o.mode = static_cast<uint8_t>((ca ? LD_MODE_A_CONST : 0) | (cb ? LD_MODE_B_CONST : 0));
        return QString();
    }
    case LD_OP_MOV:
        if (!parseValue(ta, true, o.a, constFlag)) return "Source: a register name or a number from -32768 to 32767";
        o.mode = constFlag ? LD_MODE_A_CONST : 0;
        if (!parseRegister(tb, true, o.b)) return "Destination must be D0-D15 or VH0-VH18";
        return QString();
    }
    return "Unknown instruction";
}

} // namespace

bool askOperands(QWidget *parent, const Node &n, Operands &out)
{
    const FieldDef f = fieldsFor(n);
    QDialog dlg(parent);
    dlg.setWindowTitle(QString("Edit %1").arg(mnemonic(n.op)));
    auto *lay = new QVBoxLayout(&dlg);
    auto *form = new QFormLayout;
    auto *ea = new QLineEdit(f.textA);
    form->addRow(f.labelA, ea);
    QLineEdit *eb = nullptr;
    if (!f.labelB.isEmpty()) { eb = new QLineEdit(f.textB); form->addRow(f.labelB, eb); }
    lay->addLayout(form);
    auto *msg = new QLabel;
    msg->setStyleSheet("color:#b91c1c");
    msg->setWordWrap(true);
    lay->addWidget(msg);
    auto *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    lay->addWidget(box);
    QObject::connect(box, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    Operands res;
    QObject::connect(box, &QDialogButtonBox::accepted, &dlg, [&] {
        const QString err = parseFields(n.op, ea->text(), eb ? eb->text() : QString(), res);
        if (err.isEmpty()) dlg.accept(); else msg->setText(err);
    });
    dlg.setMinimumWidth(440);
    if (dlg.exec() != QDialog::Accepted) return false;
    out = res;
    return true;
}
