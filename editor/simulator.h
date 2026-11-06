#ifndef SIMULATOR_H
#define SIMULATOR_H

// simulator.h - runs the C engine on the PC, one scan every 10 ms
#pragma once
#include "backend.h"
#include <QTimer>
#include <memory>

class Simulator : public Backend {
    Q_OBJECT
public:
    explicit Simulator(QObject *parent = nullptr);
    ~Simulator() override;

    QString load(const QByteArray &image) override;
    void setInput(int index, bool on) override;
    void writeReg(uint16_t addr, uint16_t value) override;
    void setForce(uint16_t bitAddr, bool enabled, bool value) override;
    const PlcState &state() const override { return state_; }
    QString name() const override { return "Simulator"; }

    void step(int scans = 1);      // run scans now, then publish (used by tests)
    void setRunning(bool run);     // start or stop the 10 ms timer

private:
    void scanOnce();
    void publish();                // copy the engine state and emit stateChanged()

    std::unique_ptr<ld_engine_t> eng_;
    PlcState state_;
    uint8_t inputs_[8] = {};
    QTimer timer_;
    int scansSincePublish_ = 0;
};
#endif // SIMULATOR_H
