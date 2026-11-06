#include "watchmodel.h"
#include <QBrush>
#include <QColor>

WatchModel::WatchModel(QObject *parent) : QAbstractTableModel(parent)
{
    for (int i = 0; i < LD_NUM_INPUTS; ++i) {
        Row r;
        r.name = QStringLiteral("I%1").arg(i);
        r.addr = static_cast<uint16_t>(LD_BIT_I + i);
        rows_.push_back(r);
    }
    for (int i = 0; i < LD_NUM_OUTPUTS; ++i) {
        Row r;
        r.name = QStringLiteral("Q%1").arg(i);
        r.addr = static_cast<uint16_t>(LD_BIT_Q + i);
        rows_.push_back(r);
    }
}

int WatchModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : rows_.size();
}

int WatchModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColCount;
}

QVariant WatchModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rows_.size())
        return {};
    const Row &r = rows_[index.row()];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case ColName:    return r.name;
        case ColAddress: return static_cast<int>(r.addr);
        case ColValue:   return r.value ? QStringLiteral("1") : QStringLiteral("0");
        case ColForced:  return r.forced ? QStringLiteral("FORCED") : QString();
        }
    } else if (role == Qt::TextAlignmentRole) {
        if (index.column() != ColName)
            return static_cast<int>(Qt::AlignCenter);
    } else if (role == Qt::BackgroundRole) {
        if (index.column() == ColForced && r.forced)
            return QBrush(QColor(255, 200, 120));      // orange: forced
        if (index.column() == ColValue && r.value)
            return QBrush(QColor(150, 220, 150));      // green: on
    } else if (role == Qt::ForegroundRole) {
        // Dark text on the light cells above, readable in dark themes too.
        if ((index.column() == ColForced && r.forced) ||
            (index.column() == ColValue && r.value))
            return QBrush(QColor(20, 20, 20));
    }
    return {};
}

QVariant WatchModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    switch (section) {
    case ColName:    return tr("Name");
    case ColAddress: return tr("Address");
    case ColValue:   return tr("Value");
    case ColForced:  return tr("Force");
    }
    return {};
}

void WatchModel::setState(const PlcState &s)
{
    for (int i = 0; i < rows_.size(); ++i) {
        Row &r = rows_[i];
        if (r.addr >= LD_BIT_SPACE)
            continue;                                   // defensive: never read out of range
        const bool v = s.bits[r.addr] != 0;
        const bool f = s.forced[r.addr] != 0;
        if (v != r.value || f != r.forced) {
            r.value = v;
            r.forced = f;
            emit dataChanged(index(i, ColValue), index(i, ColForced));
        }
    }
}

uint16_t WatchModel::bitAddrForRow(int row) const { return rows_.at(row).addr; }
bool WatchModel::valueAt(int row) const           { return rows_.at(row).value; }
bool WatchModel::isForced(int row) const          { return rows_.at(row).forced; }