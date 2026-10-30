#include "mainwindow.h"
#include "ladder_compiler.h"
#include "operand_dialog.h"
#include <QDockWidget>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>
extern "C" {
#include "ladder_engine.h"
}

struct MainWindow::Sim {
    ld_engine_t eng;
};

namespace {

// Palette list whose drag carries the op code as text
class PaletteList : public QListWidget {
public:
    using QListWidget::QListWidget;
protected:
    QMimeData *mimeData(const QList<QListWidgetItem *> &items) const override
    {
        auto *m = new QMimeData;
        if (!items.isEmpty()) m->setText(QString::number(items.first()->data(Qt::UserRole).toInt()));
        return m;
    }
};

struct PaletteEntry { uint8_t op; const char *text; };
const PaletteEntry kPalette[] = {
    {LD_OP_XIC,  "XIC   contact, true when the bit is 1"},
    {LD_OP_XIO,  "XIO   contact, true when the bit is 0"},
    {LD_OP_GRT,  "GRT   compare A > B"},
    {LD_OP_LES,  "LES   compare A < B"},
    {LD_OP_EQU,  "EQU   compare A = B"},
    {LD_OP_OTE,  "OTE   output coil"},
    {LD_OP_TON,  "TON   on-delay timer"},
    {LD_OP_TOF,  "TOF   off-delay timer"},
    {LD_OP_TONR, "TONR  retentive timer"},
    {LD_OP_CTU,  "CTU   up counter"},
    {LD_OP_RES,  "RES   reset timer or counter"},
    {LD_OP_MOV,  "MOV   move a value to a register"},
};

} // namespace

MainWindow::MainWindow() : sim_(new Sim)
{
    resize(1250, 760);
    ed_.program() = makeDemoProgram();

    scene_ = new LadderScene(this);
    view_ = new QGraphicsView(scene_, this);
    view_->setBackgroundBrush(Qt::white);
    view_->setRenderHint(QPainter::Antialiasing);
    view_->setAcceptDrops(true);
    setCentralWidget(view_);
    scene_->onSelect = [this](Node *n) { onSelect(n); };
    scene_->onEdit = [this](Node *n) { onEdit(n); };
    scene_->onDropOp = [this](uint8_t op, Node *target) { apply(ed_.insertAt(op, target)); };

    // palette (right)
    auto *palette = new PaletteList;
    palette->setDragEnabled(true);
    palette->setDragDropMode(QAbstractItemView::DragOnly);
    for (const PaletteEntry &e : kPalette) {
        auto *it = new QListWidgetItem(e.text);
        it->setData(Qt::UserRole, e.op);
        palette->addItem(it);
    }
    connect(palette, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *it) {
        apply(ed_.insert(static_cast<uint8_t>(it->data(Qt::UserRole).toInt())));
    });
    auto *pw = new QWidget;
    auto *pl = new QVBoxLayout(pw);
    pl->addWidget(new QLabel("<b>Instructions</b><br>Drag onto the ladder, or select a place and double-click."));
    pl->addWidget(palette);
    auto *pdock = new QDockWidget("Palette", this);
    pdock->setWidget(pw);
    addDockWidget(Qt::RightDockWidgetArea, pdock);

    // simulation panel (left)
    auto *panel = new QWidget;
    auto *lay = new QVBoxLayout(panel);
    lay->addWidget(new QLabel("<b>Inputs</b> (click to toggle)"));
    const char *hint[8] = {"start", "stop", "load D0", "", "", "", "", ""};
    for (int i = 0; i < 8; i++) {
        in_[i] = new QCheckBox(QString("I%1 %2").arg(i).arg(hint[i]));
        lay->addWidget(in_[i]);
    }
    lay->addSpacing(12);
    lay->addWidget(new QLabel("<b>Outputs</b>"));
    for (int i = 0; i < 6; i++) {
        out_[i] = new QLabel(QString("Q%1  off").arg(i));
        lay->addWidget(out_[i]);
    }
    lay->addStretch();
    auto *dock = new QDockWidget("Simulation", this);
    dock->setWidget(panel);
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    // actions
    auto *bar = addToolBar("Edit");
    auto *mFile = menuBar()->addMenu("&File");
    auto *mEdit = menuBar()->addMenu("&Edit");
    auto add = [&](QMenu *m, const QString &text, const QKeySequence &key, auto fn, bool toolbar = false) {
        QAction *a = m->addAction(text);
        if (!key.isEmpty()) a->setShortcut(key);
        connect(a, &QAction::triggered, this, fn);
        if (toolbar) bar->addAction(a);
        return a;
    };
    add(mFile, "&New", QKeySequence::New, [this] { newFile(); });
    add(mFile, "&Open...", QKeySequence::Open, [this] { openFile(); });
    add(mFile, "&Save", QKeySequence::Save, [this] { saveFile(); });
    add(mFile, "Save &as...", QKeySequence(), [this] { saveFileAs(); });
    actUndo_ = add(mEdit, "&Undo", QKeySequence::Undo, [this] { ed_.undo(); modified_ = true; refresh(); }, true);
    actRedo_ = add(mEdit, "&Redo", QKeySequence("Ctrl+Y"), [this] { ed_.redo(); modified_ = true; refresh(); }, true);
    mEdit->addSeparator(); bar->addSeparator();
    add(mEdit, "&Delete", QKeySequence::Delete, [this] { apply(ed_.remove()); }, true);
    add(mEdit, "Edit &instruction...", QKeySequence(Qt::Key_Return), [this] { if (ed_.selected()) onEdit(ed_.selected()); });
    mEdit->addSeparator(); bar->addSeparator();
    add(mEdit, "&Wrap in parallel branch", QKeySequence("Ctrl+B"), [this] { apply(ed_.wrapInBranch()); }, true);
    add(mEdit, "Add &branch to group", QKeySequence("Ctrl+Shift+B"), [this] { apply(ed_.addBranch()); }, true);
    mEdit->addSeparator(); bar->addSeparator();
    add(mEdit, "Add &rung", QKeySequence("Ctrl+R"), [this] { apply(ed_.addRung()); }, true);
    add(mEdit, "Delete r&ung", QKeySequence(), [this] { apply(ed_.deleteRung()); }, true);
    add(mEdit, "Rung &comment...", QKeySequence("Ctrl+M"), [this] {
        const int r = ed_.rungOf(ed_.selected());
        if (r < 0) { statusBar()->showMessage("Select something in the rung first", 5000); return; }
        bool ok = false;
        const QString t = QInputDialog::getText(this, "Rung comment", "Comment:", QLineEdit::Normal,
                                                ed_.program().rungs[static_cast<size_t>(r)].comment, &ok);
        if (ok) apply(ed_.setComment(r, t));
    });
    bar->addSeparator();
    auto *run = bar->addAction("Pause");
    run->setCheckable(true);
    connect(run, &QAction::toggled, this, [this, run](bool paused) {
        run->setText(paused ? "Run" : "Pause");
        if (paused) timer_.stop(); else timer_.start(10);
    });
    auto *reset = bar->addAction("Reset simulation");
    connect(reset, &QAction::triggered, this, [this] { refresh(); });

    refresh();
    connect(&timer_, &QTimer::timeout, this, [this] { tick(); });
    timer_.start(10);                   // one scan every 10 ms, like the PLC
}

MainWindow::~MainWindow() = default;

void MainWindow::refresh()
{
    CompileResult r = compileProgram(ed_.program());       // assigns animation ids
    scene_->setProgram(ed_.program());
    scene_->setSelected(ed_.selected());
    if (r.ok()) {
        ld_init(&sim_->eng);
        const ld_status_t st = ld_load(&sim_->eng, reinterpret_cast<const uint8_t *>(r.image.constData()),
                                       static_cast<size_t>(r.image.size()));
        simValid_ = (st == LD_OK);
        statusBar()->showMessage(simValid_ ? QString("Compiled: %1 instructions, %2 elements, %3 bytes")
                                                 .arg(r.instrCount).arg(r.elemCount).arg(r.image.size())
                                           : QString("Load error %1").arg(int(st)));
    } else {
        simValid_ = false;
        scene_->clearPower();
        statusBar()->showMessage("Compile error: " + r.error);
    }
    updateUi();
}

void MainWindow::updateUi()
{
    actUndo_->setEnabled(ed_.canUndo());
    actRedo_->setEnabled(ed_.canRedo());
    const QString name = path_.isEmpty() ? QString("untitled") : QFileInfo(path_).fileName();
    setWindowTitle(QString("%1%2 - Ladder editor").arg(name).arg(modified_ ? " *" : ""));
}

void MainWindow::apply(const QString &err)
{
    if (!err.isEmpty()) { statusBar()->showMessage(err, 6000); return; }
    modified_ = true;
    refresh();
}

void MainWindow::onSelect(Node *n)
{
    ed_.select(n);
    scene_->setSelected(n);
}

void MainWindow::onEdit(Node *n)
{
    if (!n || n->kind != Node::Element) return;
    ed_.select(n);
    Operands o;
    o.a = n->a; o.b = n->b; o.mode = n->mode;
    if (askOperands(this, *n, o)) apply(ed_.setOperands(n, o.a, o.b, o.mode));
}

void MainWindow::tick()
{
    ld_engine_t &e = sim_->eng;
    for (int i = 0; i < 8; i++) e.bits[LD_BIT_I + i] = in_[i]->isChecked() ? 1 : 0;
    ld_scan(&e);
    if (simValid_) scene_->showPower(e.elem_power);
    for (int i = 0; i < 6; i++) {
        const bool on = e.bits[LD_BIT_Q + i];
        out_[i]->setText(QString("Q%1  %2").arg(i).arg(on ? "ON" : "off"));
        out_[i]->setStyleSheet(on ? "color:#16a34a;font-weight:bold" : "color:#666");
    }
}

bool MainWindow::maybeSave()
{
    if (!modified_) return true;
    const auto b = QMessageBox::question(this, "Unsaved changes", "Save the changes to the program?",
                                         QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    if (b == QMessageBox::Cancel) return false;
    if (b == QMessageBox::Save) return saveFile();
    return true;
}

void MainWindow::newFile()
{
    if (!maybeSave()) return;
    ed_.clear();
    path_.clear();
    modified_ = false;
    refresh();
}

void MainWindow::openFile()
{
    if (!maybeSave()) return;
    const QString path = QFileDialog::getOpenFileName(this, "Open ladder program", path_, "Ladder program (*.json)");
    if (path.isEmpty()) return;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) { QMessageBox::warning(this, "Open", "Cannot read " + path); return; }
    const QString err = ed_.fromJson(f.readAll());
    if (!err.isEmpty()) { QMessageBox::warning(this, "Open", err); return; }
    path_ = path;
    modified_ = false;
    refresh();
}

bool MainWindow::writeFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(ed_.toJson()) < 0) {
        QMessageBox::warning(this, "Save", "Cannot write " + path);
        return false;
    }
    path_ = path;
    modified_ = false;
    updateUi();
    statusBar()->showMessage("Saved " + path, 4000);
    return true;
}

bool MainWindow::saveFile() { return path_.isEmpty() ? saveFileAs() : writeFile(path_); }

bool MainWindow::saveFileAs()
{
    const QString path = QFileDialog::getSaveFileName(this, "Save ladder program", path_, "Ladder program (*.json)");
    return !path.isEmpty() && writeFile(path);
}

void MainWindow::closeEvent(QCloseEvent *e)
{
    if (maybeSave()) e->accept(); else e->ignore();
}
