// editor_core.h - all editing operations on a Program (no GUI, fully testable)
#pragma once
#include "ladder_model.h"
#include <QByteArray>
#include <QString>
#include <vector>

class ProgramEditor {
public:
    Program &program() { return prog_; }
    const Program &program() const { return prog_; }
    Node *selected() const { return sel_; }
    void select(Node *n) { sel_ = n; }

    // Every edit returns an empty string on success, otherwise the reason (and changes nothing).
    QString insert(uint8_t op);                    // at the selection
    QString insertAt(uint8_t op, Node *target);    // drag and drop
    QString remove();                              // selected element, or selected empty branch
    QString wrapInBranch();                        // selected element gets a parallel branch
    QString addBranch();                           // one more branch in the selected group
    QString addRung();                             // after the selected rung, or at the end
    QString deleteRung();
    QString setOperands(Node *n, uint16_t a, uint16_t b, uint8_t mode);
    QString setComment(int rung, const QString &text);

    bool canUndo() const { return !undo_.empty(); }
    bool canRedo() const { return !redo_.empty(); }
    void undo();
    void redo();

    QByteArray toJson() const;
    QString fromJson(const QByteArray &data);      // replaces the program and clears the undo history
    void clear();                                  // one empty rung
    int rungOf(const Node *n) const;               // -1 if the node is not in the program

private:
    void snapshot();
    bool path(Node *target, std::vector<Node *> &p) const;
    QString place(std::unique_ptr<Node> el, Node *target);

    Program prog_;
    Node *sel_ = nullptr;
    std::vector<QByteArray> undo_, redo_;
};
