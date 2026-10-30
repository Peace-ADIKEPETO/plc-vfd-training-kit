#include "ladder_scene.h"
#include <QFont>
#include <QGraphicsSceneDragDropEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMimeData>
#include <QPainter>
#include <QPen>
#include <QTimer>
#include <algorithm>
extern "C" {
#include "ladder_engine.h"
}

static void paintSelection(QPainter *p, bool selected)
{
    if (!selected) return;
    p->setPen(QPen(QColor(37, 99, 235), 1.6, Qt::DashLine));
    p->setBrush(QColor(37, 99, 235, 22));
    p->drawRoundedRect(QRectF(4, 4, ElementItem::CW - 8, ElementItem::CH - 8), 6, 6);
}

void ElementItem::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    auto *s = static_cast<LadderScene *>(scene());
    if (s->onSelect) s->onSelect(node_);
    e->accept();
}

void ElementItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e)
{
    auto *s = static_cast<LadderScene *>(scene());
    s->requestEdit(node_);
    e->accept();
}

void ElementItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    p->setRenderHint(QPainter::Antialiasing);
    paintSelection(p, static_cast<LadderScene *>(scene())->selected() == node_);
    const QColor on(22, 163, 74), off(70, 70, 70);
    const QPen wirePen(QColor(90, 90, 90), 1.8);
    const QPen pen(power_ ? on : off, power_ ? 2.6 : 1.8);
    const double cy = CH / 2, mid = CW / 2;
    QFont small = p->font();
    small.setPointSizeF(8);
    p->setFont(small);

    switch (node_->op) {
    case LD_OP_XIC: case LD_OP_XIO: {
        const double d = 14;
        p->setPen(wirePen);
        p->drawLine(QPointF(0, cy), QPointF(mid - d, cy));
        p->drawLine(QPointF(mid + d, cy), QPointF(CW, cy));
        p->setPen(pen);
        p->drawLine(QPointF(mid - d, cy - 14), QPointF(mid - d, cy + 14));
        p->drawLine(QPointF(mid + d, cy - 14), QPointF(mid + d, cy + 14));
        if (node_->op == LD_OP_XIO) p->drawLine(QPointF(mid - d + 4, cy + 12), QPointF(mid + d - 4, cy - 12));
        break;
    }
    case LD_OP_OTE: {
        p->setPen(wirePen);
        p->drawLine(QPointF(0, cy), QPointF(mid - 16, cy));
        p->drawLine(QPointF(mid + 16, cy), QPointF(CW, cy));
        p->setPen(pen);
        p->setBrush(power_ ? QBrush(QColor(22, 163, 74, 50)) : Qt::NoBrush);
        p->drawEllipse(QPointF(mid, cy), 16, 16);
        break;
    }
    default: {
        const QRectF r(12, 8, CW - 24, CH - 16);
        p->setPen(wirePen);
        p->drawLine(QPointF(0, cy), QPointF(r.left(), cy));
        p->drawLine(QPointF(r.right(), cy), QPointF(CW, cy));
        p->setPen(pen);
        p->setBrush(power_ ? QBrush(QColor(22, 163, 74, 50)) : QBrush(QColor(248, 248, 248)));
        p->drawRoundedRect(r, 4, 4);
        QFont b = small; b.setBold(true);
        p->setFont(b);
        p->setPen(QColor(30, 30, 30));
        p->drawText(QRectF(r.left(), r.top() + 2, r.width(), 14), Qt::AlignCenter, mnemonic(node_->op));
        p->setFont(small);
        p->drawText(QRectF(r.left() + 2, r.top() + 17, r.width() - 4, r.height() - 19),
                    Qt::AlignCenter | Qt::TextWordWrap, detailText(*node_));
        return;
    }
    }
    p->setPen(QColor(30, 30, 30));
    p->drawText(QRectF(0, 1, CW, 16), Qt::AlignCenter, detailText(*node_));
    p->setPen(QColor(120, 120, 120));
    p->drawText(QRectF(0, CH - 17, CW, 16), Qt::AlignCenter, mnemonic(node_->op));
}

void PlaceholderItem::mousePressEvent(QGraphicsSceneMouseEvent *e)
{
    auto *s = static_cast<LadderScene *>(scene());
    if (s->onSelect) s->onSelect(series_);
    e->accept();
}

void PlaceholderItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *)
{
    p->setRenderHint(QPainter::Antialiasing);
    paintSelection(p, static_cast<LadderScene *>(scene())->selected() == series_);
    const double cy = ElementItem::CH / 2;
    p->setPen(QPen(QColor(90, 90, 90), 1.8));
    p->drawLine(QPointF(0, cy), QPointF(ElementItem::CW, cy));
    p->setPen(QPen(QColor(150, 150, 150), 1.2, Qt::DashLine));
    p->setBrush(QColor(255, 255, 255));
    const QRectF r(ElementItem::CW / 2 - 22, cy - 13, 44, 26);
    p->drawRoundedRect(r, 5, 5);
    QFont f = p->font(); f.setPointSizeF(8); p->setFont(f);
    p->setPen(QColor(130, 130, 130));
    p->drawText(r, Qt::AlignCenter, "empty");
}

LadderScene::Size LadderScene::measure(const Node *n) const
{
    switch (n->kind) {
    case Node::Element: return {1, 1};
    case Node::Series: {
        int w = 0, h = 1;
        for (auto &k : n->kids) { Size s = measure(k.get()); w += s.w; h = std::max(h, s.h); }
        return {w, h};
    }
    case Node::Parallel: {
        int w = 1, h = 0;
        for (auto &k : n->kids) { Size s = measure(k.get()); w = std::max(w, s.w); h += s.h; }
        return {w, std::max(h, 1)};
    }
    }
    return {1, 1};
}

void LadderScene::wire(double x1, double y1, double x2, double y2)
{
    addLine(x1, y1, x2, y2, QPen(QColor(90, 90, 90), 1.8));
}

void LadderScene::draw(Node *n, QPointF o, int col, int row, int width)
{
    const double CW = ElementItem::CW, CH = ElementItem::CH;
    const double cy = o.y() + row * CH + CH / 2;
    switch (n->kind) {
    case Node::Element: {
        auto *it = new ElementItem(n);
        it->setPos(o.x() + col * CW, o.y() + row * CH);
        addItem(it);
        items_.push_back(it);
        if (width > 1) wire(o.x() + (col + 1) * CW, cy, o.x() + (col + width) * CW, cy);
        break;
    }
    case Node::Series: {
        if (n->kids.empty()) {
            auto *ph = new PlaceholderItem(n);
            ph->setPos(o.x() + col * CW, o.y() + row * CH);
            addItem(ph);
            if (width > 1) wire(o.x() + (col + 1) * CW, cy, o.x() + (col + width) * CW, cy);
            break;
        }
        int x = col;
        for (auto &k : n->kids) {
            Size s = measure(k.get());
            draw(k.get(), o, x, row, s.w);
            x += s.w;
        }
        if (x < col + width) wire(o.x() + x * CW, cy, o.x() + (col + width) * CW, cy);
        break;
    }
    case Node::Parallel: {
        int y = row;
        double firstY = 0, lastY = 0;
        bool first = true;
        for (auto &k : n->kids) {
            Size s = measure(k.get());
            draw(k.get(), o, col, y, width);
            const double yc = o.y() + y * CH + CH / 2;
            if (first) { firstY = yc; first = false; }
            lastY = yc;
            y += s.h;
        }
        if (n->kids.size() > 1) {
            wire(o.x() + col * CW, firstY, o.x() + col * CW, lastY);
            wire(o.x() + (col + width) * CW, firstY, o.x() + (col + width) * CW, lastY);
        }
        break;
    }
    }
}

void LadderScene::setProgram(Program &p)
{
    clear();
    items_.clear();
    bands_.clear();
    const double CW = ElementItem::CW, CH = ElementItem::CH, RX = 60;
    int maxCols = 1;
    for (auto &r : p.rungs) maxCols = std::max(maxCols, measure(r.root.get()).w);

    double top = 40;
    for (size_t i = 0; i < p.rungs.size(); i++) {
        Rung &r = p.rungs[i];
        const Size s = measure(r.root.get());
        auto *num = addText(QString::number(i + 1));
        num->setPos(12, top + CH / 2 - 12);
        if (!r.comment.isEmpty()) {
            auto *c = addText(r.comment);
            c->setDefaultTextColor(QColor(100, 100, 100));
            c->setPos(RX, top - 24);
        }
        draw(r.root.get(), QPointF(RX, top), 0, 0, maxCols);
        bands_.push_back({top - 24, top + s.h * CH + 12, r.root.get()});
        top += s.h * CH + 44;
    }
    const double bottom = std::max(top - 44 + 12, 90.0);
    const QPen rail(QColor(40, 40, 40), 3.5);
    addLine(RX, 16, RX, bottom, rail);
    addLine(RX + maxCols * CW, 16, RX + maxCols * CW, bottom, rail);
    setSceneRect(QRectF(0, 0, RX + maxCols * CW + 40, bottom + 40));
}

void LadderScene::showPower(const uint8_t *elemPower)
{
    for (auto *it : items_)
        if (it->id() >= 0) it->setPower(elemPower[it->id()] != 0);
}

void LadderScene::clearPower()
{
    for (auto *it : items_) it->setPower(false);
}

void LadderScene::requestEdit(Node *n)
{
    QTimer::singleShot(0, this, [this, n] { if (onEdit) onEdit(n); });
}

void LadderScene::dragEnterEvent(QGraphicsSceneDragDropEvent *e) { e->acceptProposedAction(); }
void LadderScene::dragMoveEvent(QGraphicsSceneDragDropEvent *e) { e->acceptProposedAction(); }

void LadderScene::dropEvent(QGraphicsSceneDragDropEvent *e)
{
    bool ok = false;
    const int op = e->mimeData()->text().toInt(&ok);
    if (!ok) { e->ignore(); return; }
    Node *target = nullptr;
    for (QGraphicsItem *it : items(e->scenePos())) {
        if (auto *ei = dynamic_cast<ElementItem *>(it)) { target = ei->node(); break; }
        if (auto *pi = dynamic_cast<PlaceholderItem *>(it)) { target = pi->series(); break; }
    }
    if (!target) {
        for (const Band &b : bands_)
            if (e->scenePos().y() >= b.top && e->scenePos().y() <= b.bottom) { target = b.root; break; }
    }
    if (!target) { e->ignore(); return; }
    e->acceptProposedAction();
    QTimer::singleShot(0, this, [this, op, target] { if (onDropOp) onDropOp(static_cast<uint8_t>(op), target); });
}
