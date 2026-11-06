#ifndef BACKEND_H
#define BACKEND_H

// backend.h - what the window needs from "something that runs a ladder program"
#pragma once
#include <QByteArray>
#include <QObject>
#include <QString>
#include <cstdint>
extern "C" {
#include "ladder_engine.h"
}

// A snapshot of everything the UI displays. Plain data, so it can be copied freely.
struct PlcState {
    uint8_t  bits[LD_BIT_SPACE] = {};
    uint16_t regs[LD_REG_SPACE] = {};
    uint8_t  elemPower[LD_NUM_ELEM] = {};
    uint8_t  forced[LD_BIT_SPACE] = {};        // 1 = this bit is forced (only I0..I7 and Q0..Q5 can be)
};

class Backend : public QObject {
    Q_OBJECT
public:
    explicit Backend(QObject *parent = nullptr) : QObject(parent) {}
    ~Backend() override = default;

    virtual QString load(const QByteArray &image) = 0;     // empty string = success
    virtual void setInput(int index, bool on) = 0;         // I0..I7
    virtual void writeReg(uint16_t addr, uint16_t value) = 0;
    virtual void setForce(uint16_t bitAddr, bool enabled, bool value) = 0;
    virtual const PlcState &state() const = 0;
    virtual QString name() const = 0;

signals:
    void stateChanged();
};
#endif // BACKEND_H
