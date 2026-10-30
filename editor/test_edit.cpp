// test_edit.cpp - tests for ProgramEditor (insert, delete, branches, undo, save/load) and the text parsers
#include "editor_core.h"
#include "ladder_compiler.h"
#include <cstdio>
extern "C" {
#include "ladder_engine.h"
}

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; std::printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
#define OK(expr) do { QString e_ = (expr); checks++; if (!e_.isEmpty()) { failures++; std::printf("FAIL line %d: %s -> %s\n", __LINE__, #expr, qPrintable(e_)); } } while (0)
#define FAILS(expr) do { QString e_ = (expr); checks++; if (e_.isEmpty()) { failures++; std::printf("FAIL line %d: %s should have failed\n", __LINE__, #expr); } } while (0)

static Node *root(ProgramEditor &e, int r = 0) { return e.program().rungs[static_cast<size_t>(r)].root.get(); }

static ld_engine_t eng;
static bool run(ProgramEditor &e)
{
    CompileResult r = compileProgram(e.program());
    if (!r.ok()) return false;
    ld_init(&eng);
    return ld_load(&eng, reinterpret_cast<const uint8_t *>(r.image.constData()), static_cast<size_t>(r.image.size())) == LD_OK;
}

static void testInsertOrder()
{
    ProgramEditor e; e.clear();
    OK(e.insert(LD_OP_XIC));
    OK(e.insert(LD_OP_XIO));
    OK(e.insert(LD_OP_OTE));
    CHECK(root(e)->kids.size() == 3);
    CHECK(root(e)->kids[0]->op == LD_OP_XIC && root(e)->kids[1]->op == LD_OP_XIO && root(e)->kids[2]->op == LD_OP_OTE);
    // OTE is selected: a new condition must still go before the output
    OK(e.insert(LD_OP_XIC));
    CHECK(root(e)->kids.size() == 4 && root(e)->kids[2]->op == LD_OP_XIC && root(e)->kids[3]->op == LD_OP_OTE);
    // a second output goes at the end
    OK(e.insert(LD_OP_TON));
    CHECK(root(e)->kids.back()->op == LD_OP_TON);
    CHECK(run(e));
}

static void testBranch()
{
    ProgramEditor e; e.clear();
    OK(e.insert(LD_OP_XIC));                         // I0
    Node *first = e.selected();
    OK(e.wrapInBranch());
    CHECK(root(e)->kids.size() == 1 && root(e)->kids[0]->kind == Node::Parallel);
    CHECK(e.selected()->kind == Node::Series && e.selected()->kids.empty());   // empty branch selected
    OK(e.insert(LD_OP_XIC));                         // goes into the empty branch
    Node *second = e.selected();
    CHECK(second->kind == Node::Element);
    uint16_t a;
    CHECK(parseBit("I1", a));
    OK(e.setOperands(second, a, 0, 0));
    OK(e.insert(LD_OP_OTE));                         // output: end of the rung, not in the branch
    CHECK(root(e)->kids.size() == 2 && root(e)->kids[1]->op == LD_OP_OTE);
    CHECK(run(e));
    ld_scan(&eng); CHECK(eng.bits[LD_BIT_Q] == 0);
    eng.bits[LD_BIT_I + 1] = 1; ld_scan(&eng); CHECK(eng.bits[LD_BIT_Q] == 1);          // I1 alone is enough
    eng.bits[LD_BIT_I + 1] = 0; eng.bits[LD_BIT_I] = 1; ld_scan(&eng); CHECK(eng.bits[LD_BIT_Q] == 1);

    // third branch
    e.select(first);
    OK(e.addBranch());
    CHECK(root(e)->kids[0]->kids.size() == 3);
    CHECK(run(e));

    // delete the empty third branch, then the group stays (2 branches)
    OK(e.remove());
    CHECK(root(e)->kids[0]->kids.size() == 2);
    // delete the second branch's element, then the empty branch: group unwraps to a plain contact
    e.select(second);
    OK(e.remove());
    e.select(root(e)->kids[0]->kids[1].get());       // the empty branch
    OK(e.remove());
    CHECK(root(e)->kids[0]->kind == Node::Element && root(e)->kids[0].get() == first);
    CHECK(run(e));
}

static void testLimits()
{
    ProgramEditor e; e.clear();
    OK(e.insert(LD_OP_XIC));
    for (int i = 0; i < 4; i++) {                    // nest 4 levels: wrap, then insert into the new branch, wrap again
        OK(e.wrapInBranch());
        OK(e.insert(LD_OP_XIC));
    }
    FAILS(e.wrapInBranch());                         // 5th level
    OK(e.insert(LD_OP_OTE));
    CHECK(run(e));
    FAILS(e.wrapInBranch());
    ProgramEditor f; f.clear();
    OK(f.insert(LD_OP_OTE));
    FAILS(f.wrapInBranch());                         // outputs cannot be wrapped
    ProgramEditor g; g.clear();
    OK(g.insert(LD_OP_XIC));
    FAILS(g.addBranch());                            // not inside a group
}

static void testRungs()
{
    ProgramEditor e; e.clear();
    OK(e.insert(LD_OP_XIC));
    OK(e.addRung());
    CHECK(e.program().rungs.size() == 2 && e.selected() == root(e, 1));
    OK(e.insert(LD_OP_OTE));
    OK(e.setComment(1, "second"));
    CHECK(e.program().rungs[1].comment == "second");
    OK(e.deleteRung());
    CHECK(e.program().rungs.size() == 1);
    e.select(root(e, 0)->kids[0].get());
    OK(e.deleteRung());
    CHECK(e.program().rungs.empty());
    OK(e.insert(LD_OP_XIC));                         // inserting into an empty program creates a rung
    CHECK(e.program().rungs.size() == 1);
    // an empty rung selected + remove deletes it
    OK(e.addRung());
    OK(e.remove());
    CHECK(e.program().rungs.size() == 1);
}

static void testOperands()
{
    ProgramEditor e; e.clear();
    OK(e.insert(LD_OP_TON));
    Node *t = e.selected();
    OK(e.setOperands(t, 3, 250, 0));
    CHECK(t->a == 3 && t->b == 250);
    FAILS(e.setOperands(t, 16, 5, 0));               // timer 16 does not exist
    OK(e.insert(LD_OP_MOV));
    FAILS(e.setOperands(e.selected(), 5, 0x9999, LD_MODE_A_CONST));   // destination out of range
}

static void testUndoRedo()
{
    ProgramEditor e; e.clear();
    CHECK(!e.canUndo());
    OK(e.insert(LD_OP_XIC));
    OK(e.insert(LD_OP_OTE));
    OK(e.addRung());
    CHECK(e.canUndo());
    e.undo(); CHECK(e.program().rungs.size() == 1 && root(e)->kids.size() == 2);
    e.undo(); CHECK(root(e)->kids.size() == 1);
    e.undo(); CHECK(root(e)->kids.empty());
    CHECK(!e.canUndo() && e.canRedo());
    e.redo(); e.redo(); e.redo();
    CHECK(e.program().rungs.size() == 2 && root(e)->kids.size() == 2);
    CHECK(!e.canRedo());
    OK(e.insert(LD_OP_XIC));
    e.undo();
    OK(e.insert(LD_OP_XIO));                         // a new edit clears redo
    CHECK(!e.canRedo());
}

static void testJson()
{
    ProgramEditor a; a.program() = makeDemoProgram();
    CompileResult ra = compileProgram(a.program());
    const QByteArray js = a.toJson();
    ProgramEditor b;
    OK(b.fromJson(js));
    CompileResult rb = compileProgram(b.program());
    CHECK(ra.ok() && rb.ok() && ra.image == rb.image);
    CHECK(b.program().rungs[0].comment == a.program().rungs[0].comment);
    CHECK(!b.canUndo());
    FAILS(b.fromJson("not json"));
    FAILS(b.fromJson("{\"format\":\"other\",\"version\":1,\"rungs\":[]}"));
    FAILS(b.fromJson("{\"format\":\"ladder\",\"version\":9,\"rungs\":[]}"));
    FAILS(b.fromJson("{\"format\":\"ladder\",\"version\":1,\"rungs\":[{\"root\":{\"k\":\"E\",\"op\":2,\"a\":0,\"b\":0,\"mode\":0}}]}"));      // element as rung root
    FAILS(b.fromJson("{\"format\":\"ladder\",\"version\":1,\"rungs\":[{\"root\":{\"k\":\"S\",\"kids\":[{\"k\":\"E\",\"op\":99,\"a\":0,\"b\":0,\"mode\":0}]}}]}"));  // unknown op
    FAILS(b.fromJson("{\"format\":\"ladder\",\"version\":1,\"rungs\":[{\"root\":{\"k\":\"S\",\"kids\":[{\"k\":\"E\",\"op\":2,\"a\":9999,\"b\":0,\"mode\":0}]}}]}"));  // bad address
    FAILS(b.fromJson("{\"format\":\"ladder\",\"version\":1,\"rungs\":[{\"root\":{\"k\":\"S\",\"kids\":[{\"k\":\"P\",\"kids\":[{\"k\":\"E\",\"op\":2,\"a\":0,\"b\":0,\"mode\":0}]}]}}]}"));  // element directly in a group
    CHECK(b.program().rungs.size() == 5);            // a failed load leaves the program untouched
}

static void testParsers()
{
    uint16_t v; bool c;
    CHECK(parseBit("q3", v) && v == LD_BIT_Q + 3);
    CHECK(parseBit(" I7 ", v) && v == LD_BIT_I + 7);
    CHECK(parseBit("M63", v) && v == LD_BIT_M + 63);
    CHECK(parseBit("T2.DN", v) && v == LD_BIT_TDN + 2);
    CHECK(parseBit("c15.dn", v) && v == LD_BIT_CDN + 15);
    CHECK(parseBit("VST7", v) && v == LD_BIT_VFD_ST + 7);
    CHECK(parseBit("VCT2", v) && v == LD_BIT_VFD_CT + 2);
    CHECK(!parseBit("I8", v) && !parseBit("Q6", v) && !parseBit("M64", v) && !parseBit("T16.DN", v));
    CHECK(!parseBit("", v) && !parseBit("Q", v) && !parseBit("Q-1", v) && !parseBit("hello", v));
    CHECK(parseRegister("D15", false, v) && v == LD_REG_D + 15);
    CHECK(parseRegister("VH18", true, v) && v == LD_REG_VFD_H + 18);
    CHECK(!parseRegister("VH19", true, v) && !parseRegister("T0.ACC", true, v) && parseRegister("T0.ACC", false, v));
    CHECK(parseValue("1500", true, v, c) && c && v == 1500);
    CHECK(parseValue("-1", true, v, c) && c && v == 0xFFFF);
    CHECK(parseValue("0x10", true, v, c) && c && v == 16);
    CHECK(!parseValue("40000", true, v, c));
    CHECK(parseValue("D3", true, v, c) && !c && v == LD_REG_D + 3);
    CHECK(!parseValue("500", false, v, c));
    int i;
    CHECK(parseIndex("T3", 'T', 16, i) && i == 3);
    CHECK(parseIndex("3", 'T', 16, i) && i == 3);
    CHECK(!parseIndex("T16", 'T', 16, i) && !parseIndex("C3", 'T', 16, i) && !parseIndex("", 'T', 16, i));
}

int main()
{
    testInsertOrder(); testBranch(); testLimits(); testRungs();
    testOperands(); testUndoRedo(); testJson(); testParsers();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
