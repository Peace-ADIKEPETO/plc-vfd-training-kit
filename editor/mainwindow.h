// mainwindow.h - ladder editor: palette, editable ladder view, live simulator
#pragma once
#include "editor_core.h"
#include "ladder_scene.h"
#include "simulator.h"
#include "watchmodel.h"
#include <QTableView>
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
    void onStateChanged();
    void onSelect(Node *n);
    void onEdit(Node *n);
    void updateUi();
    bool maybeSave();
    void newFile();
    void openFile();
    bool saveFile();
    bool saveFileAs();
    bool writeFile(const QString &path);
    void exportC();

    ProgramEditor ed_;
    Simulator *sim_ = nullptr;              // owned by this window (Qt parent)
    LadderScene *scene_;
    QGraphicsView *view_;
    QCheckBox *in_[8];
    QLabel *out_[6];
    WatchModel *watch_ = nullptr;
    QAction *actUndo_ = nullptr, *actRedo_ = nullptr;
    QString path_;
    bool modified_ = false;
    bool simValid_ = false;
};
