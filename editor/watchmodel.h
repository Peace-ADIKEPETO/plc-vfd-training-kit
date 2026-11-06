#ifndef WATCHMODEL_H
#define WATCHMODEL_H

#pragma once
#include <QAbstractTableModel>
#include <QVector>
#include <QString>
#include "backend.h"

// Turns a PlcState into table rows: one row per input (I0..) and output (Q0..).
class WatchModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { ColName, ColAddress, ColValue, ColForced, ColCount };

    explicit WatchModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    // Call after every simulator step. Only rows that changed emit dataChanged.
    void setState(const PlcState &s);

    // For step 3c (force checkboxes): which bit does a row stand for?
    uint16_t bitAddrForRow(int row) const;
    bool valueAt(int row) const;
    bool isForced(int row) const;

private:
    struct Row {
        QString  name;
        uint16_t addr = 0;
        bool     value = false;
        bool     forced = false;
    };
    QVector<Row> rows_;
};

#endif // WATCHMODEL_H
