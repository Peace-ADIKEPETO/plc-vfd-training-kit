// ladder_scene.h - draws a Program as rungs; click to select, double-click to edit, drop instructions on it
#pragma once
#include "ladder_model.h"
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <functional>
#include <vector>

class LadderScene;

class ElementItem : public QGraphicsItem {
public:
    static constexpr double CW = 110.0;   // cell width
    static constexpr double CH = 64.0;    // cell height

    explicit ElementItem(Node *n) : node_(n) { setAcceptedMouseButtons(Qt::LeftButton); }
    QRectF boundingRect() const override { return QRectF(0, 0, CW, CH); }
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;
    void setPower(bool on) { if (on != power_) { power_ = on; update(); } }
    int id() const { return node_->id; }
    Node *node() const { return node_; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *e) override;

private:
    Node *node_;
    bool power_ = false;
};

// Shown for an empty series: an empty rung or an empty parallel branch
class PlaceholderItem : public QGraphicsItem {
public:
    explicit PlaceholderItem(Node *series) : series_(series) { setAcceptedMouseButtons(Qt::LeftButton); }
    QRectF boundingRect() const override { return QRectF(0, 0, ElementItem::CW, ElementItem::CH); }
    void paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) override;
    Node *series() const { return series_; }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *e) override;

private:
    Node *series_;
};

class LadderScene : public QGraphicsScene {
public:
    explicit LadderScene(QObject *parent = nullptr) : QGraphicsScene(parent) {}

    std::function<void(Node *)> onSelect;                // click
    std::function<void(Node *)> onEdit;                  // double-click on an element
    std::function<void(uint8_t, Node *)> onDropOp;       // instruction dropped on a target

    void setProgram(Program &p);                         // rebuilds the whole scene
    void setSelected(Node *n) { sel_ = n; update(); }
    Node *selected() const { return sel_; }
    void showPower(const uint8_t *elemPower);            // engine.elem_power[]
    void clearPower();
    void requestEdit(Node *n);                           // deferred, so the scene is not rebuilt inside an item event

protected:
    void dragEnterEvent(QGraphicsSceneDragDropEvent *e) override;
    void dragMoveEvent(QGraphicsSceneDragDropEvent *e) override;
    void dropEvent(QGraphicsSceneDragDropEvent *e) override;

private:
    struct Size { int w, h; };
    struct Band { double top, bottom; Node *root; };
    Size measure(const Node *n) const;
    void draw(Node *n, QPointF origin, int col, int row, int width);
    void wire(double x1, double y1, double x2, double y2);
    std::vector<ElementItem *> items_;
    std::vector<Band> bands_;
    Node *sel_ = nullptr;
};
