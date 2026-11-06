#include "watchmodel.h"
#include <cstdio>

static int g_checks = 0, g_fail = 0;
#define CHECK(c) do { ++g_checks; if (!(c)) { ++g_fail; \
std::printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

    static QString text(const WatchModel &m, int row, int col)
    {
        return m.data(m.index(row, col), Qt::DisplayRole).toString();
    }

int main()
{
    WatchModel m;
    int changes = 0;
    QObject::connect(&m, &QAbstractItemModel::dataChanged, [&](){ ++changes; });

    const int nIn = LD_NUM_INPUTS;
    CHECK(m.rowCount() == LD_NUM_INPUTS + LD_NUM_OUTPUTS);
    CHECK(m.columnCount() == WatchModel::ColCount);

    // names and addresses
    CHECK(text(m, 0, WatchModel::ColName) == "I0");
    CHECK(text(m, nIn - 1, WatchModel::ColName) == QString("I%1").arg(nIn - 1));
    CHECK(text(m, nIn, WatchModel::ColName) == "Q0");
    CHECK(m.bitAddrForRow(0) == LD_BIT_I + 0);
    CHECK(m.bitAddrForRow(nIn) == LD_BIT_Q + 0);
    CHECK(m.headerData(WatchModel::ColValue, Qt::Horizontal).toString() == "Value");

    // initial state: everything 0, nothing forced
    CHECK(text(m, 0, WatchModel::ColValue) == "0");
    CHECK(text(m, 0, WatchModel::ColForced).isEmpty());

    // a changed bit updates its row and emits exactly one signal
    PlcState s;
    s.bits[LD_BIT_I + 0] = 1;
    m.setState(s);
    CHECK(text(m, 0, WatchModel::ColValue) == "1");
    CHECK(m.valueAt(0));
    CHECK(changes == 1);

    // the same state again: no signal
    m.setState(s);
    CHECK(changes == 1);

    // a forced output shows FORCED and its value
    s.bits[LD_BIT_Q + 4] = 1;
    s.forced[LD_BIT_Q + 4] = 1;
    m.setState(s);
    const int q4 = nIn + 4;
    CHECK(m.isForced(q4));
    CHECK(text(m, q4, WatchModel::ColForced) == "FORCED");
    CHECK(text(m, q4, WatchModel::ColValue) == "1");
    CHECK(changes == 2);

    // releasing the force clears the flag
    s.forced[LD_BIT_Q + 4] = 0;
    m.setState(s);
    CHECK(!m.isForced(q4));
    CHECK(text(m, q4, WatchModel::ColForced).isEmpty());

    // out-of-range index gives an empty value, not a crash
    CHECK(!m.data(m.index(999, 0), Qt::DisplayRole).isValid());

    std::printf("%d checks, %d failures\n", g_checks, g_fail);
    return g_fail ? 1 : 0;
}