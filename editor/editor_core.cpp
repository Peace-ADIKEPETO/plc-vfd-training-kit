#include "editor_core.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
extern "C" {
#include "ladder_engine.h"
}

namespace {

std::unique_ptr<Node> makeDefault(uint8_t op)
{
    switch (op) {
    case LD_OP_XIC: case LD_OP_XIO: return Node::element(op, LD_BIT_I);
    case LD_OP_OTE: return Node::element(op, LD_BIT_Q);
    case LD_OP_TON: case LD_OP_TOF: case LD_OP_TONR: return Node::element(op, 0, 10);
    case LD_OP_CTU: return Node::element(op, 0, 5);
    case LD_OP_RES: return Node::element(op, 0);
    case LD_OP_GRT: case LD_OP_LES: case LD_OP_EQU: return Node::element(op, LD_REG_D, 0, LD_MODE_B_CONST);
    case LD_OP_MOV: return Node::element(op, 0, LD_REG_D, LD_MODE_A_CONST);
    default: return nullptr;
    }
}

bool findPath(Node *cur, Node *target, std::vector<Node *> &p)
{
    p.push_back(cur);
    if (cur == target) return true;
    for (auto &k : cur->kids)
        if (findPath(k.get(), target, p)) return true;
    p.pop_back();
    return false;
}

size_t indexIn(Node *parent, Node *child)
{
    for (size_t i = 0; i < parent->kids.size(); i++)
        if (parent->kids[i].get() == child) return i;
    return parent->kids.size();
}

QJsonObject nodeToJson(const Node &n)
{
    QJsonObject o;
    if (n.kind == Node::Element) {
        o["k"] = "E"; o["op"] = n.op; o["mode"] = n.mode; o["a"] = n.a; o["b"] = n.b;
    } else {
        o["k"] = n.kind == Node::Series ? "S" : "P";
        QJsonArray arr;
        for (auto &k : n.kids) arr.append(nodeToJson(*k));
        o["kids"] = arr;
    }
    return o;
}

// parentKind: -1 for a rung root, otherwise the kind of the parent node.
// A series holds elements and parallel groups, a parallel group holds series.
std::unique_ptr<Node> nodeFromJson(const QJsonObject &o, int parentKind, int depth, QString &err)
{
    if (depth > 32) { err = "nesting too deep"; return nullptr; }
    const QString k = o["k"].toString();
    if (k == "E") {
        if (parentKind != Node::Series) { err = "element in the wrong place"; return nullptr; }
        const int op = o["op"].toInt(-1);
        if (!isKnownOp(op)) { err = "unknown instruction"; return nullptr; }
        const auto a = static_cast<uint16_t>(o["a"].toInt());
        const auto b = static_cast<uint16_t>(o["b"].toInt());
        const auto mode = static_cast<uint8_t>(o["mode"].toInt());
        const QString bad = validateOperands(static_cast<uint8_t>(op), a, b, mode);
        if (!bad.isEmpty()) { err = bad; return nullptr; }
        return Node::element(static_cast<uint8_t>(op), a, b, mode);
    }
    if (k == "S" || k == "P") {
        const bool isSeries = k == "S";
        if (isSeries && parentKind != -1 && parentKind != Node::Parallel) { err = "series in the wrong place"; return nullptr; }
        if (!isSeries && parentKind != Node::Series) { err = "branch group in the wrong place"; return nullptr; }
        auto n = isSeries ? Node::series() : Node::parallel();
        for (const QJsonValue &v : o["kids"].toArray()) {
            auto kid = nodeFromJson(v.toObject(), n->kind, depth + 1, err);
            if (!kid) return nullptr;
            n->kids.push_back(std::move(kid));
        }
        return n;
    }
    err = "unknown node type";
    return nullptr;
}

} // namespace

QByteArray ProgramEditor::toJson() const
{
    QJsonArray rungs;
    for (const Rung &r : prog_.rungs) {
        QJsonObject o;
        o["comment"] = r.comment;
        o["root"] = nodeToJson(*r.root);
        rungs.append(o);
    }
    QJsonObject doc;
    doc["format"] = "ladder";
    doc["version"] = 1;
    doc["rungs"] = rungs;
    return QJsonDocument(doc).toJson(QJsonDocument::Compact);
}

QString ProgramEditor::fromJson(const QByteArray &data)
{
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(data, &pe);
    if (d.isNull() || !d.isObject()) return "Not a valid ladder file";
    const QJsonObject doc = d.object();
    if (doc["format"].toString() != "ladder") return "Not a ladder file";
    if (doc["version"].toInt() != 1) return "Unsupported file version";
    Program p;
    for (const QJsonValue &v : doc["rungs"].toArray()) {
        const QJsonObject o = v.toObject();
        QString err;
        auto root = nodeFromJson(o["root"].toObject(), -1, 0, err);
        if (!root) return "Invalid file: " + err;
        Rung r;
        r.comment = o["comment"].toString();
        r.root = std::move(root);
        p.rungs.push_back(std::move(r));
    }
    prog_ = std::move(p);
    sel_ = nullptr;
    undo_.clear();
    redo_.clear();
    return QString();
}

void ProgramEditor::clear()
{
    prog_ = Program();
    prog_.rungs.emplace_back();
    sel_ = prog_.rungs[0].root.get();
    undo_.clear();
    redo_.clear();
}

void ProgramEditor::snapshot()
{
    undo_.push_back(toJson());
    if (undo_.size() > 200) undo_.erase(undo_.begin());
    redo_.clear();
}

void ProgramEditor::undo()
{
    if (undo_.empty()) return;
    redo_.push_back(toJson());
    const QByteArray prev = undo_.back();
    undo_.pop_back();
    auto keepUndo = undo_, keepRedo = redo_;
    fromJson(prev);
    undo_ = keepUndo;
    redo_ = keepRedo;
}

void ProgramEditor::redo()
{
    if (redo_.empty()) return;
    undo_.push_back(toJson());
    const QByteArray next = redo_.back();
    redo_.pop_back();
    auto keepUndo = undo_, keepRedo = redo_;
    fromJson(next);
    undo_ = keepUndo;
    redo_ = keepRedo;
}

bool ProgramEditor::path(Node *target, std::vector<Node *> &p) const
{
    for (const Rung &r : prog_.rungs) {
        p.clear();
        if (findPath(r.root.get(), target, p)) return true;
    }
    p.clear();
    return false;
}

int ProgramEditor::rungOf(const Node *n) const
{
    std::vector<Node *> p;
    if (!n || !path(const_cast<Node *>(n), p)) return -1;
    for (size_t i = 0; i < prog_.rungs.size(); i++)
        if (prog_.rungs[i].root.get() == p[0]) return static_cast<int>(i);
    return -1;
}

QString ProgramEditor::place(std::unique_ptr<Node> el, Node *target)
{
    const bool output = el->isOutput();
    Node *series = nullptr, *rootOfRung = nullptr;
    size_t idx = 0;
    std::vector<Node *> p;
    if (prog_.rungs.empty()) {
        snapshot();
        prog_.rungs.emplace_back();
        series = rootOfRung = prog_.rungs[0].root.get();
    } else {
        if (!target) target = prog_.rungs.back().root.get();
        if (!path(target, p)) return "Nothing selected";
        rootOfRung = p[0];
        if (target->kind == Node::Series) {
            series = target;
            idx = target->kids.size();
        } else if (target->kind == Node::Element) {
            series = p[p.size() - 2];
            idx = indexIn(series, target) + 1;
        } else {
            return "Select an element or an empty branch";
        }
        snapshot();
    }
    if (output) {
        series = rootOfRung;                       // outputs always end the rung
        idx = series->kids.size();
    } else if (series == rootOfRung) {             // conditions stay before the outputs
        size_t firstOut = series->kids.size();
        for (size_t i = 0; i < series->kids.size(); i++)
            if (series->kids[i]->isOutput()) { firstOut = i; break; }
        idx = std::min(idx, firstOut);
    }
    Node *raw = el.get();
    series->kids.insert(series->kids.begin() + static_cast<std::ptrdiff_t>(idx), std::move(el));
    sel_ = raw;
    return QString();
}

QString ProgramEditor::insert(uint8_t op) { return insertAt(op, sel_); }

QString ProgramEditor::insertAt(uint8_t op, Node *target)
{
    auto el = makeDefault(op);
    if (!el) return "Unknown instruction";
    return place(std::move(el), target);
}

QString ProgramEditor::remove()
{
    if (!sel_) return "Nothing selected";
    std::vector<Node *> p;
    if (!path(sel_, p)) return "Nothing selected";
    if (sel_->kind == Node::Element) {
        Node *parent = p[p.size() - 2];
        const size_t i = indexIn(parent, sel_);
        snapshot();
        parent->kids.erase(parent->kids.begin() + static_cast<std::ptrdiff_t>(i));
        sel_ = nullptr;
        return QString();
    }
    if (sel_->kind == Node::Series) {
        if (!sel_->kids.empty()) return "Delete the elements first, or use Delete rung";
        if (p.size() == 1) return deleteRung();
        Node *par = p[p.size() - 2];               // the parallel group
        Node *outer = p[p.size() - 3];             // the series that holds the group
        const size_t ib = indexIn(par, sel_);
        const size_t ip = indexIn(outer, par);
        snapshot();
        par->kids.erase(par->kids.begin() + static_cast<std::ptrdiff_t>(ib));
        if (par->kids.size() == 1) {               // one branch left: unwrap the group
            std::vector<std::unique_ptr<Node>> moved = std::move(par->kids[0]->kids);
            outer->kids.erase(outer->kids.begin() + static_cast<std::ptrdiff_t>(ip));
            outer->kids.insert(outer->kids.begin() + static_cast<std::ptrdiff_t>(ip),
                               std::make_move_iterator(moved.begin()), std::make_move_iterator(moved.end()));
        }
        sel_ = nullptr;
        return QString();
    }
    return "Select an element";
}

QString ProgramEditor::wrapInBranch()
{
    if (!sel_ || sel_->kind != Node::Element) return "Select an element first";
    if (sel_->isOutput()) return "Outputs cannot be placed in a branch";
    std::vector<Node *> p;
    if (!path(sel_, p)) return "Nothing selected";
    int depth = 0;
    for (Node *n : p) if (n->kind == Node::Parallel) depth++;
    if (depth + 1 > LD_MAX_BRANCH) return "Branches can be nested 4 levels at most";
    Node *series = p[p.size() - 2];
    const size_t i = indexIn(series, sel_);
    snapshot();
    auto a = Node::series();
    a->kids.push_back(std::move(series->kids[i]));
    auto b = Node::series();
    Node *braw = b.get();
    auto par = Node::parallel();
    par->kids.push_back(std::move(a));
    par->kids.push_back(std::move(b));
    series->kids[i] = std::move(par);
    sel_ = braw;                                   // select the new empty branch
    return QString();
}

QString ProgramEditor::addBranch()
{
    if (!sel_) return "Select an element inside a parallel branch";
    std::vector<Node *> p;
    if (!path(sel_, p)) return "Nothing selected";
    Node *par = nullptr;
    for (size_t i = p.size(); i-- > 0;)
        if (p[i]->kind == Node::Parallel) { par = p[i]; break; }
    if (!par) return "Select an element inside a parallel branch";
    snapshot();
    auto s = Node::series();
    Node *raw = s.get();
    par->kids.push_back(std::move(s));
    sel_ = raw;
    return QString();
}

QString ProgramEditor::addRung()
{
    size_t at = prog_.rungs.size();
    const int r = rungOf(sel_);
    if (r >= 0) at = static_cast<size_t>(r) + 1;
    snapshot();
    prog_.rungs.emplace(prog_.rungs.begin() + static_cast<std::ptrdiff_t>(at));
    sel_ = prog_.rungs[at].root.get();
    return QString();
}

QString ProgramEditor::deleteRung()
{
    const int r = rungOf(sel_);
    if (r < 0) return "Select something in the rung first";
    snapshot();
    prog_.rungs.erase(prog_.rungs.begin() + r);
    sel_ = nullptr;
    return QString();
}

QString ProgramEditor::setOperands(Node *n, uint16_t a, uint16_t b, uint8_t mode)
{
    std::vector<Node *> p;
    if (!n || n->kind != Node::Element || !path(n, p)) return "Nothing selected";
    const QString bad = validateOperands(n->op, a, b, mode);
    if (!bad.isEmpty()) return bad;
    snapshot();
    n->a = a; n->b = b; n->mode = mode;
    return QString();
}

QString ProgramEditor::setComment(int rung, const QString &text)
{
    if (rung < 0 || rung >= static_cast<int>(prog_.rungs.size())) return "No such rung";
    snapshot();
    prog_.rungs[static_cast<size_t>(rung)].comment = text;
    return QString();
}
