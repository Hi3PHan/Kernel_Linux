#pragma once

#include <QElapsedTimer>
#include <QHash>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;
class QHideEvent;
class QShowEvent;
class QTimer;
class HistoryGraphWidget;
class CpuBarsWidget;
class ResourceBarWidget;

struct CpuSnapshot {
    unsigned long long idle = 0;
    unsigned long long total = 0;
};

class SystemMonitorPage : public QWidget {
    Q_OBJECT

public:
    explicit SystemMonitorPage(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QWidget* buildMetricCard(const QString& title, QLabel** valueLabel, QLabel** detailLabel = nullptr);
    void refresh();
    void updateCpu();
    void updateMemory();
    void updateDisk();
    void updateUptime();
    void updateNetwork();

    QVector<CpuSnapshot> readCpuSnapshots() const;
    QHash<QString, unsigned long long> readMeminfo() const;
    QString formatBytes(qulonglong bytes) const;
    QString formatRate(double bytesPerSecond) const;
    QString formatDuration(double seconds) const;

    QTimer* refreshTimer = nullptr;
    QElapsedTimer networkElapsed;
    QVector<CpuSnapshot> previousCpu;
    qulonglong previousRxBytes = 0;
    qulonglong previousTxBytes = 0;

    QLabel* cpuValue = nullptr;
    QLabel* cpuDetail = nullptr;
    QLabel* ramValue = nullptr;
    QLabel* ramDetail = nullptr;
    QLabel* swapValue = nullptr;
    QLabel* swapDetail = nullptr;
    QLabel* diskValue = nullptr;
    QLabel* diskDetail = nullptr;
    QLabel* uptimeValue = nullptr;
    QLabel* uptimeDetail = nullptr;
    QLabel* loadValue = nullptr;
    QLabel* loadDetail = nullptr;
    QLabel* networkDetail = nullptr;

    HistoryGraphWidget* cpuGraph = nullptr;
    HistoryGraphWidget* networkGraph = nullptr;
    CpuBarsWidget* cpuBars = nullptr;
    ResourceBarWidget* memoryBar = nullptr;
};
