#pragma once

#include <QAbstractTableModel>
#include <QDateTime>

enum class TrustLevel {
    Reliable,
    Unreliable
};

struct ActivityEntry {
    QDateTime time;
    QString level;
    QString action;
    QString detail;
    QString output;
    int exitCode = 0;
    TrustLevel trustLevel = TrustLevel::Reliable;
};

class ActivityLogger : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit ActivityLogger(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void addEntry(const ActivityEntry& entry);
    void clear();
    ActivityEntry entryAt(int row) const;

private:
    QList<ActivityEntry> entries;
};
