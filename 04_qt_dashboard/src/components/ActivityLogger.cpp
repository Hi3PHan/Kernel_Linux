#include "ActivityLogger.h"

#include <QBrush>
#include <QColor>

ActivityLogger::ActivityLogger(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int ActivityLogger::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : entries.size();
}

int ActivityLogger::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : 5;
}

QVariant ActivityLogger::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= entries.size()) {
        return {};
    }

    const auto& entry = entries.at(index.row());
    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return entry.time.toString("yyyy-MM-dd HH:mm:ss");
        case 1:
            return entry.level;
        case 2:
            return entry.action;
        case 3:
            return entry.detail;
        case 4:
            return entry.exitCode;
        default:
            return {};
        }
    }

    if (role == Qt::ForegroundRole) {
        if (entry.exitCode != 0) {
            return QBrush(QColor("#C7162D"));
        }
        if (entry.trustLevel == TrustLevel::Unreliable) {
            return QBrush(QColor("#6E6E6E"));
        }
    }

    return {};
}

QVariant ActivityLogger::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
    case 0:
        return "Time";
    case 1:
        return "Level";
    case 2:
        return "Action";
    case 3:
        return "Detail";
    case 4:
        return "Exit";
    default:
        return {};
    }
}

void ActivityLogger::addEntry(const ActivityEntry& entry)
{
    beginInsertRows(QModelIndex(), entries.size(), entries.size());
    entries.append(entry);
    endInsertRows();
}

void ActivityLogger::clear()
{
    beginResetModel();
    entries.clear();
    endResetModel();
}

ActivityEntry ActivityLogger::entryAt(int row) const
{
    if (row < 0 || row >= entries.size()) {
        return {};
    }
    return entries.at(row);
}
