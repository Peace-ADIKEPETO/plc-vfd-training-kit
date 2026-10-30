// test_compiler.cpp - checks the compiler output by loading it into the real engine
#include "ladder_compiler.h"
#include "ladder_model.h"
#include <cstdio>
extern "C" {
#include "ladder_engine.h"
}

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; std::printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static ld_engine_t eng;
static void scans(int n) { for (int i = 0; i < n; i++) ld_scan(&eng); }

static void loadProgram(Program &p)
{
    CompileResult r = compileProgram(p);
    CHECK(r.ok());
    ld_init(&eng);
    CHECK(ld_load(&eng, reinterpret_cast<const uint8_t *>(r.image.constData()), static_cast<size_t>(r.image.size())) == LD_OK);
}

static void testDemo()
{
    Program p = makeDemoProgram();
    CompileResult r = compileProgram(p);
    CHECK(r.ok());
    // first rung must be: BEGIN, BR_OPEN, XIC, BR_NEXT, XIC, BR_CLOSE, XIO, OTE
    const uint8_t expect[8] = { LD_OP_RUNG_BEGIN, LD_OP_BR_OPEN, LD_OP_XIC, LD_OP_BR_NEXT, LD_OP_XIC, LD_OP_BR_CLOSE, LD_OP_XIO, LD_OP_OTE };
    for (int i = 0; i < 8; i++) CHECK(static_cast<uint8_t>(r.image[LD_HEADER_SIZE + i * LD_INSTR_SIZE]) == expect[i]);
    CHECK(r.elemCount == 12);

    loadProgram(p);
    eng.bits[LD_BIT_I + 0] = 1; scans(1);
    CHECK(eng.bits[LD_BIT_Q + 0] == 1);
    eng.bits[LD_BIT_I + 0] = 0; scans(98);                       // 99 scans since Q0 came on
    CHECK(eng.bits[LD_BIT_Q + 0] == 1 && eng.bits[LD_BIT_Q + 1] == 0);
    scans(1);                                                    // 100th scan
    CHECK(eng.bits[LD_BIT_Q + 1] == 1);
    eng.bits[LD_BIT_I + 2] = 1; scans(1);
    CHECK(eng.regs[LD_REG_D] == 1500 && eng.bits[LD_BIT_Q + 2] == 1);
    eng.bits[LD_BIT_I + 1] = 1; scans(1);                        // stop
    CHECK(eng.bits[LD_BIT_Q + 0] == 0 && eng.bits[LD_BIT_Q + 1] == 0);
    CHECK(eng.elem_power[0] == 0);                               // first contact of rung 1 has no power
}

static void testErrors()
{
    auto el = [](uint8_t op, uint16_t a, uint16_t b = 0) { return Node::element(op, a, b); };
    {   // output inside a branch
        Program p; Rung r;
        Node *par = r.root->add(Node::parallel());
        par->add(Node::series())->add(el(LD_OP_OTE, LD_BIT_Q));
        par->add(Node::series())->add(el(LD_OP_XIC, LD_BIT_I));
        p.rungs.push_back(std::move(r));
        CHECK(!compileProgram(p).ok());
    }
    {   // condition after an output
        Program p; Rung r;
        r.root->add(el(LD_OP_OTE, LD_BIT_Q));
        r.root->add(el(LD_OP_XIC, LD_BIT_I));
        p.rungs.push_back(std::move(r));
        CHECK(!compileProgram(p).ok());
    }
    {   // branches nested 5 deep
        Program p; Rung r;
        Node *n = r.root.get();
        for (int i = 0; i < 5; i++) { Node *par = n->add(Node::parallel()); n = par->add(Node::series()); par->add(Node::series()); }
        p.rungs.push_back(std::move(r));
        CHECK(!compileProgram(p).ok());
    }
    {   // too many elements (256)
        Program p;
        for (int i = 0; i < 256; i++) { Rung r; r.root->add(el(LD_OP_XIC, LD_BIT_I)); p.rungs.push_back(std::move(r)); }
        CHECK(!compileProgram(p).ok());
    }
    {   // an empty branch is a plain wire: (I0 OR wire) -> Q0 is always on
        Program p; Rung r;
        Node *par = r.root->add(Node::parallel());
        par->add(Node::series())->add(el(LD_OP_XIC, LD_BIT_I));
        par->add(Node::series());
        r.root->add(el(LD_OP_OTE, LD_BIT_Q));
        p.rungs.push_back(std::move(r));
        loadProgram(p);
        scans(1);
        CHECK(eng.bits[LD_BIT_Q] == 1);
    }
}

int main()
{
    testDemo();
    testErrors();
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
