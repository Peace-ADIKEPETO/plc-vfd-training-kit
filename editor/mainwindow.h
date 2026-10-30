// mainwindow.h - ladder editor: palette, editable ladder view, live simulator
#pragma once
#include "editor_core.h"
#include "ladder_scene.h"
#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QGraphicsView>
#include <QLabel>
#include <QMainWindow>
#include <QTimer>
#include <memory>

class MainWindow : public QMainWindow {
public:
    MainWindow();
    ~MainWindow();
    ProgramEditor &editor() { return ed_; }
    LadderScene *scene() { return scene_; }
    void refresh();                         // recompile, reload the simulator, redraw

protected:
    void closeEvent(QCloseEvent *e) override;

private:
    void apply(const QString &err);         // shows the error, or marks the program modified and refreshes
    void tick();
    void onSelect(Node *n);
    void onEdit(Node *n);
    void updateUi();
    bool maybeSave();
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    bool writeFile(const QString &path);

    ProgramEditor ed_;
    struct Sim;                             // holds the engine (keeps the C header out of this file)
    std::unique_ptr<Sim> sim_;
    LadderScene *scene_;
    QGraphicsView *view_;
    QTimer timer_;
    QCheckBox *in_[8];
    QLabel *out_[6];
    QAction *actUndo_ = nullptr, *actRedo_ = nullptr;
    QString path_;
    bool modified_ = false;
    bool simValid_ = false;
};
