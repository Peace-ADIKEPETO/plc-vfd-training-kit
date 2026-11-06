// test_backend.cpp - the Simulator through the Backend interface
#include "ladder_compiler.h"
#include "ladder_model.h"
#include "simulator.h"
#include <QCoreApplication>
#include <cstdio>

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; std::printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    Program prog = makeDemoProgram();
    const CompileResult r = compileProgram(prog);
    CHECK(r.ok());

    Simulator sim;
    Backend &be = sim;                       // use it only through the interface
    int signalCount = 0;
    QObject::connect(&be, &Backend::stateChanged, [&] { signalCount++; });

    CHECK(be.load(r.image).isEmpty());
    CHECK(signalCount == 1);                 // load publishes once

    be.setInput(0, true);
    sim.step(1);
    CHECK(be.state().bits[LD_BIT_Q + 0] == 1);
    CHECK(be.state().elemPower[0] == 1);     // the I0 contact carries power
    CHECK(signalCount == 2);                 // step publishes once per call

    be.setInput(0, false);
    sim.step(98);
    CHECK(be.state().bits[LD_BIT_Q + 0] == 1 && be.state().bits[LD_BIT_Q + 1] == 0);
    sim.step(1);
    CHECK(be.state().bits[LD_BIT_Q + 1] == 1);   // timer done after 100 scans

    be.setInput(2, true);
    sim.step(1);
    CHECK(be.state().regs[LD_REG_D + 0] == 1500 && be.state().bits[LD_BIT_Q + 2] == 1);

    // a bad image is refused and the old program keeps running
    QByteArray bad = r.image;
    bad[LD_HEADER_SIZE + 4] = static_cast<char>(bad[LD_HEADER_SIZE + 4] ^ 1);
    CHECK(!be.load(bad).isEmpty());
    be.setInput(1, true);                    // stop button
    sim.step(1);
    CHECK(be.state().bits[LD_BIT_Q + 0] == 0);   // the old latch program still ran

    // a good load starts from clean memory
    CHECK(be.load(r.image).isEmpty());
    CHECK(be.state().bits[LD_BIT_Q + 1] == 0 && be.state().regs[LD_REG_D + 0] == 0);

    // writeReg changes a register the ladder reads
    be.writeReg(LD_REG_D + 0, 1500);
    sim.step(1);
    CHECK(be.state().bits[LD_BIT_Q + 2] == 1);   // D0 > 1000

    // ---- forcing through the interface ----
    CHECK(be.load(r.image).isEmpty());
    be.setInput(1, false);                       // the simulator keeps its inputs across loads: release the earlier ones
    be.setInput(2, false);
    be.setForce(LD_BIT_I + 0, true, true);       // "press" start without touching the input
    sim.step(1);
    CHECK(be.state().forced[LD_BIT_I + 0] == 1);
    CHECK(be.state().bits[LD_BIT_Q + 0] == 1);   // the latch started
    be.setForce(LD_BIT_I + 0, false, false);     // release the force
    sim.step(1);
    CHECK(be.state().forced[LD_BIT_I + 0] == 0);
    CHECK(be.state().bits[LD_BIT_Q + 0] == 1);   // still latched

    be.setForce(LD_BIT_Q + 4, true, true);       // force an output that the ladder never writes
    sim.step(1);
    CHECK(be.state().bits[LD_BIT_Q + 4] == 1 && be.state().forced[LD_BIT_Q + 4] == 1);

    be.setForce(LD_BIT_M + 0, true, true);       // a memory bit cannot be forced: ignored
    sim.step(1);
    CHECK(be.state().forced[LD_BIT_M + 0] == 0 && be.state().bits[LD_BIT_M + 0] == 0);

    CHECK(be.load(r.image).isEmpty());           // a new program clears the forces
    CHECK(be.state().forced[LD_BIT_Q + 4] == 0 && be.state().bits[LD_BIT_Q + 4] == 0);
    std::printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}