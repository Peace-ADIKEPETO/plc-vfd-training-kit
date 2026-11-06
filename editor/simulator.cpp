#include "simulator.h"
#include <cstring>

namespace {
constexpr int kPublishEvery = 10;      // 10 scans x 10 ms = the UI refreshes every 100 ms

QString loadErrorText(ld_status_t st)
{
    switch (st) {
    case LD_ERR_SIZE:    return "Load error: wrong image size";
    case LD_ERR_MAGIC:   return "Load error: not a ladder image";
    case LD_ERR_VERSION: return "Load error: unsupported image version";
    case LD_ERR_CRC:     return "Load error: CRC mismatch";
    case LD_ERR_OPCODE:  return "Load error: unknown instruction";
    case LD_ERR_ADDR:    return "Load error: address out of range";
    case LD_ERR_STRUCT:  return "Load error: bad rung or branch structure";
    default:             return QString("Load error %1").arg(static_cast<int>(st));
    }
}
} // namespace

Simulator::Simulator(QObject *parent) : Backend(parent), eng_(new ld_engine_t)
{
    ld_init(eng_.get());
    connect(&timer_, &QTimer::timeout, this, [this] {
        scanOnce();
        if (++scansSincePublish_ >= kPublishEvery) publish();
    });
}

Simulator::~Simulator() = default;

QString Simulator::load(const QByteArray &image)
{
    // Load into a fresh engine: a bad image leaves the running program untouched,
    // and a good one starts from clean memory (timers, counters, outputs).
    auto fresh = std::make_unique<ld_engine_t>();
    ld_init(fresh.get());
    const ld_status_t st = ld_load(fresh.get(), reinterpret_cast<const uint8_t *>(image.constData()),
                                   static_cast<size_t>(image.size()));
    if (st != LD_OK) return loadErrorText(st);
    eng_ = std::move(fresh);
    publish();
    return QString();
}

void Simulator::setInput(int index, bool on)
{
    if (index >= 0 && index < 8) inputs_[index] = on ? 1 : 0;
}

void Simulator::writeReg(uint16_t addr, uint16_t value)
{
    if (addr < LD_REG_SPACE) eng_->regs[addr] = value;
}

void Simulator::setRunning(bool run)
{
    if (run) timer_.start(10); else timer_.stop();
}

void Simulator::scanOnce()
{
    for (int i = 0; i < 8; ++i) eng_->bits[LD_BIT_I + i] = inputs_[i];
    ld_scan(eng_.get());
}

void Simulator::step(int scans)
{
    for (int n = 0; n < scans; ++n) scanOnce();
    publish();
}

void Simulator::publish()
{
    scansSincePublish_ = 0;
    std::memcpy(state_.bits, eng_->bits, sizeof state_.bits);
    std::memcpy(state_.regs, eng_->regs, sizeof state_.regs);
    std::memcpy(state_.elemPower, eng_->elem_power, sizeof state_.elemPower);
    for (int i = 0; i < LD_NUM_INPUTS; ++i)  state_.forced[LD_BIT_I + i] = eng_->force_i_en[i];
    for (int i = 0; i < LD_NUM_OUTPUTS; ++i) state_.forced[LD_BIT_Q + i] = eng_->force_q_en[i];
    emit stateChanged();
}

void Simulator::setForce(uint16_t bitAddr, bool enabled, bool value)
{
    if (bitAddr < LD_BIT_I + LD_NUM_INPUTS)
        ld_force_input(eng_.get(), bitAddr - LD_BIT_I, enabled, value);
    else if (bitAddr >= LD_BIT_Q && bitAddr < LD_BIT_Q + LD_NUM_OUTPUTS)
        ld_force_output(eng_.get(), bitAddr - LD_BIT_Q, enabled, value);
    // any other address cannot be forced: ignored
    // any other address cannot be forced: ignored
    publish();                       // so the table shows the change at once, even while paused
}