#include "SystemMonitorPage.h"

#include <QDir>
#include <QFile>
#include <QGridLayout>
#include <QHideEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QShowEvent>
#include <QStorageInfo>
#include <QStringList>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_UNIX
#include <sys/sysinfo.h>
#endif

namespace {
struct NetworkSnapshot {
    qulonglong rx = 0;
    qulonglong tx = 0;
    QStringList interfaces;
    bool valid = false;
};

unsigned long long firstNumber(const QString& text)
{
    QString digits;
    for (const QChar ch : text) {
        if (ch.isDigit()) {
            digits.append(ch);
        } else if (!digits.isEmpty()) {
            break;
        }
    }
    return digits.toULongLong();
}

qulonglong readUnsignedFile(const QString& path, bool* ok)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (ok) {
            *ok = false;
        }
        return 0;
    }

    bool parsed = false;
    const qulonglong value = QString::fromLocal8Bit(file.readAll()).trimmed().toULongLong(&parsed);
    if (ok) {
        *ok = parsed;
    }
    return parsed ? value : 0;
}

NetworkSnapshot readNetworkFromSysfs()
{
    NetworkSnapshot snapshot;
    const QDir netDir("/sys/class/net");
    const QStringList interfaces = netDir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& interface : interfaces) {
        const QString statsDir = netDir.filePath(interface + "/statistics");
        bool rxOk = false;
        bool txOk = false;
        const qulonglong rx = readUnsignedFile(statsDir + "/rx_bytes", &rxOk);
        const qulonglong tx = readUnsignedFile(statsDir + "/tx_bytes", &txOk);
        if (!rxOk || !txOk) {
            continue;
        }

        snapshot.valid = true;
        snapshot.rx += rx;
        snapshot.tx += tx;
        snapshot.interfaces.append(interface);
    }
    return snapshot;
}

NetworkSnapshot readNetworkFromProc()
{
    NetworkSnapshot snapshot;
    QFile file("/proc/net/dev");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return snapshot;
    }

    while (!file.atEnd()) {
        const QString line = QString::fromLocal8Bit(file.readLine()).trimmed();
        const int colon = line.indexOf(':');
        if (colon < 0) {
            continue;
        }

        const QString interface = line.left(colon).trimmed();
        const QStringList cols = line.mid(colon + 1).simplified().split(' ', Qt::SkipEmptyParts);
        if (cols.size() < 16) {
            continue;
        }

        snapshot.valid = true;
        snapshot.rx += cols.value(0).toULongLong();
        snapshot.tx += cols.value(8).toULongLong();
        snapshot.interfaces.append(interface);
    }
    return snapshot;
}
}

class HistoryGraphWidget : public QWidget {
public:
    explicit HistoryGraphWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(130);
    }

    void setSeries(const QVector<QVector<double>>& values, const QStringList& labels, const QVector<QColor>& colors, double maxValue)
    {
        series = values;
        seriesLabels = labels;
        seriesColors = colors;
        upperBound = maxValue;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#FFFFFF"));

        const QRect graph = rect().adjusted(12, 14, -12, -24);
        painter.setPen(QColor("#EFEEED"));
        for (int i = 0; i <= 4; ++i) {
            const int y = graph.top() + graph.height() * i / 4;
            painter.drawLine(graph.left(), y, graph.right(), y);
        }

        for (int i = 0; i < series.size(); ++i) {
            const auto& values = series.at(i);
            if (values.size() < 2) {
                continue;
            }

            QPainterPath path;
            for (int point = 0; point < values.size(); ++point) {
                const double normalized = upperBound > 0.0 ? qBound(0.0, values.at(point) / upperBound, 1.0) : 0.0;
                const double x = graph.left() + (graph.width() * point / double(values.size() - 1));
                const double y = graph.bottom() - (graph.height() * normalized);
                if (point == 0) {
                    path.moveTo(x, y);
                } else {
                    path.lineTo(x, y);
                }
            }
            QPen pen(seriesColors.value(i, QColor("#E95420")), 2);
            painter.setPen(pen);
            painter.drawPath(path);
        }

        painter.setPen(QColor("#6E6E6E"));
        int legendX = graph.left();
        const int legendY = height() - 16;
        for (int i = 0; i < seriesLabels.size(); ++i) {
            painter.setBrush(seriesColors.value(i, QColor("#E95420")));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(QRect(legendX, legendY - 8, 10, 10), 2, 2);
            painter.setPen(QColor("#6E6E6E"));
            painter.drawText(legendX + 16, legendY, seriesLabels.at(i));
            legendX += painter.fontMetrics().horizontalAdvance(seriesLabels.at(i)) + 34;
        }
    }

private:
    QVector<QVector<double>> series;
    QStringList seriesLabels;
    QVector<QColor> seriesColors;
    double upperBound = 100.0;
};

class CpuBarsWidget : public QWidget {
public:
    explicit CpuBarsWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(130);
    }

    void setValues(const QVector<double>& newValues)
    {
        values = newValues;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#FFFFFF"));
        if (values.isEmpty()) {
            painter.setPen(QColor("#6E6E6E"));
            painter.drawText(rect(), Qt::AlignCenter, "No CPU data");
            return;
        }

        const int columns = values.size() > 8 ? 2 : 1;
        const int rows = (values.size() + columns - 1) / columns;
        const int labelWidth = 42;
        const int gap = 8;
        const int rowHeight = qMax(18, (height() - 20 - gap * (rows - 1)) / rows);
        const int colWidth = (width() - 24 - gap * (columns - 1)) / columns;

        for (int i = 0; i < values.size(); ++i) {
            const int col = i / rows;
            const int row = i % rows;
            const int x = 12 + col * (colWidth + gap);
            const int y = 10 + row * (rowHeight + gap);
            const QRect labelRect(x, y, labelWidth, rowHeight);
            const QRect barRect(x + labelWidth, y + 3, colWidth - labelWidth - 46, rowHeight - 6);
            const double value = qBound(0.0, values.at(i), 100.0);

            painter.setPen(QColor("#6E6E6E"));
            painter.drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, QString("cpu%1").arg(i));
            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor("#F2F1F0"));
            painter.drawRoundedRect(barRect, 4, 4);
            QRect fillRect = barRect;
            fillRect.setWidth(int(barRect.width() * value / 100.0));
            painter.setBrush(value > 85.0 ? QColor("#C7162D") : QColor("#E95420"));
            painter.drawRoundedRect(fillRect, 4, 4);
            painter.setPen(QColor("#2C2C2C"));
            painter.drawText(QRect(barRect.right() + 8, y, 38, rowHeight),
                             Qt::AlignVCenter | Qt::AlignRight,
                             QString::number(value, 'f', 0) + "%");
        }
    }

private:
    QVector<double> values;
};

class ResourceBarWidget : public QWidget {
public:
    explicit ResourceBarWidget(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setMinimumHeight(72);
    }

    void setValues(double ramPercent, double swapPercent)
    {
        ram = qBound(0.0, ramPercent, 100.0);
        swap = qBound(0.0, swapPercent, 100.0);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#FFFFFF"));
        drawBar(painter, QRect(12, 10, width() - 24, 20), "RAM", ram, QColor("#E95420"));
        drawBar(painter, QRect(12, 42, width() - 24, 20), "Swap", swap, QColor("#77216F"));
    }

private:
    void drawBar(QPainter& painter, const QRect& rect, const QString& label, double value, const QColor& color)
    {
        const QRect labelRect(rect.left(), rect.top(), 42, rect.height());
        const QRect barRect(rect.left() + 48, rect.top(), rect.width() - 96, rect.height());
        painter.setPen(QColor("#6E6E6E"));
        painter.drawText(labelRect, Qt::AlignVCenter | Qt::AlignLeft, label);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor("#F2F1F0"));
        painter.drawRoundedRect(barRect, 5, 5);
        QRect fill = barRect;
        fill.setWidth(int(barRect.width() * value / 100.0));
        painter.setBrush(color);
        painter.drawRoundedRect(fill, 5, 5);
        painter.setPen(QColor("#2C2C2C"));
        painter.drawText(QRect(barRect.right() + 8, rect.top(), 40, rect.height()),
                         Qt::AlignVCenter | Qt::AlignRight,
                         QString::number(value, 'f', 0) + "%");
    }

    double ram = 0.0;
    double swap = 0.0;
};

SystemMonitorPage::SystemMonitorPage(QWidget* parent)
    : QWidget(parent)
    , refreshTimer(new QTimer(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* metrics = new QGridLayout();
    metrics->setSpacing(12);
    metrics->addWidget(buildMetricCard("CPU", &cpuValue, &cpuDetail), 0, 0);
    metrics->addWidget(buildMetricCard("RAM", &ramValue, &ramDetail), 0, 1);
    metrics->addWidget(buildMetricCard("Swap", &swapValue, &swapDetail), 0, 2);
    metrics->addWidget(buildMetricCard("Disk /", &diskValue, &diskDetail), 1, 0);
    metrics->addWidget(buildMetricCard("Uptime", &uptimeValue, &uptimeDetail), 1, 1);
    metrics->addWidget(buildMetricCard("Load", &loadValue, &loadDetail), 1, 2);
    layout->addLayout(metrics);

    auto* chartRow = new QHBoxLayout();
    chartRow->setSpacing(12);

    auto* cpuCard = new QWidget(this);
    cpuCard->setObjectName("card");
    auto* cpuLayout = new QVBoxLayout(cpuCard);
    cpuLayout->setContentsMargins(12, 12, 12, 12);
    auto* cpuTitle = new QLabel("CPU history", cpuCard);
    cpuTitle->setObjectName("sectionHeading");
    cpuGraph = new HistoryGraphWidget(cpuCard);
    cpuBars = new CpuBarsWidget(cpuCard);
    cpuLayout->addWidget(cpuTitle);
    cpuLayout->addWidget(cpuGraph, 1);
    cpuLayout->addWidget(cpuBars, 1);

    auto* memoryCard = new QWidget(this);
    memoryCard->setObjectName("card");
    auto* memoryLayout = new QVBoxLayout(memoryCard);
    memoryLayout->setContentsMargins(12, 12, 12, 12);
    auto* memoryTitle = new QLabel("Memory and network", memoryCard);
    memoryTitle->setObjectName("sectionHeading");
    memoryBar = new ResourceBarWidget(memoryCard);
    networkDetail = new QLabel("Interfaces: waiting for data", memoryCard);
    networkDetail->setWordWrap(true);
    networkGraph = new HistoryGraphWidget(memoryCard);
    memoryLayout->addWidget(memoryTitle);
    memoryLayout->addWidget(memoryBar);
    memoryLayout->addWidget(networkDetail);
    memoryLayout->addWidget(networkGraph, 1);

    chartRow->addWidget(cpuCard, 3);
    chartRow->addWidget(memoryCard, 2);
    layout->addLayout(chartRow, 1);

    connect(refreshTimer, &QTimer::timeout, this, &SystemMonitorPage::refresh);
    refreshTimer->setInterval(1000);
    networkElapsed.start();
    refresh();
}

void SystemMonitorPage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    previousRxBytes = 0;
    previousTxBytes = 0;
    networkElapsed.restart();
    refresh();
    refreshTimer->start();
}

void SystemMonitorPage::hideEvent(QHideEvent* event)
{
    refreshTimer->stop();
    QWidget::hideEvent(event);
}

QWidget* SystemMonitorPage::buildMetricCard(const QString& title, QLabel** valueLabel, QLabel** detailLabel)
{
    auto* card = new QWidget(this);
    card->setObjectName("card");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(4);

    auto* titleLabel = new QLabel(title, card);
    auto* value = new QLabel("--", card);
    value->setObjectName("sectionHeading");
    auto* detail = new QLabel("Waiting for data", card);
    detail->setWordWrap(true);

    layout->addWidget(titleLabel);
    layout->addWidget(value);
    layout->addWidget(detail);
    if (valueLabel) {
        *valueLabel = value;
    }
    if (detailLabel) {
        *detailLabel = detail;
    }
    return card;
}

void SystemMonitorPage::refresh()
{
    updateCpu();
    updateMemory();
    updateDisk();
    updateUptime();
    updateNetwork();
}

void SystemMonitorPage::updateCpu()
{
    const QVector<CpuSnapshot> current = readCpuSnapshots();
    QVector<double> usages;
    if (!previousCpu.isEmpty() && previousCpu.size() == current.size()) {
        for (int i = 0; i < current.size(); ++i) {
            const auto totalDelta = current.at(i).total - previousCpu.at(i).total;
            const auto idleDelta = current.at(i).idle - previousCpu.at(i).idle;
            const double usage = totalDelta > 0 ? (100.0 * double(totalDelta - idleDelta) / double(totalDelta)) : 0.0;
            usages.append(qBound(0.0, usage, 100.0));
        }
    }
    previousCpu = current;

    const double totalCpu = usages.isEmpty() ? 0.0 : usages.first();
    if (cpuValue) {
        cpuValue->setText(QString::number(totalCpu, 'f', 0) + "%");
    }
    if (cpuDetail) {
        cpuDetail->setText(QString("%1 cores").arg(qMax(0, int(current.size()) - 1)));
    }

    static QVector<double> cpuHistory;
    cpuHistory.append(totalCpu);
    while (cpuHistory.size() > 60) {
        cpuHistory.removeFirst();
    }
    if (cpuGraph) {
        QVector<QVector<double>> cpuSeries;
        cpuSeries.append(cpuHistory);
        cpuGraph->setSeries(cpuSeries, QStringList{"CPU %"}, QVector<QColor>{QColor("#E95420")}, 100.0);
    }
    if (cpuBars && usages.size() > 1) {
        cpuBars->setValues(usages.mid(1));
    }
}

void SystemMonitorPage::updateMemory()
{
    const auto mem = readMeminfo();
    const double memTotal = double(mem.value("MemTotal"));
    const double memAvailable = double(mem.value("MemAvailable"));
    const double memUsed = qMax(0.0, memTotal - memAvailable);
    const double memPercent = memTotal > 0.0 ? (100.0 * memUsed / memTotal) : 0.0;

    const double swapTotal = double(mem.value("SwapTotal"));
    const double swapFree = double(mem.value("SwapFree"));
    const double swapUsed = qMax(0.0, swapTotal - swapFree);
    const double swapPercent = swapTotal > 0.0 ? (100.0 * swapUsed / swapTotal) : 0.0;

    ramValue->setText(QString::number(memPercent, 'f', 0) + "%");
    ramDetail->setText(QString("%1 / %2")
                           .arg(formatBytes(qulonglong(memUsed) * 1024ULL),
                                formatBytes(qulonglong(memTotal) * 1024ULL)));
    swapValue->setText(swapTotal > 0.0 ? QString::number(swapPercent, 'f', 0) + "%" : "Off");
    swapDetail->setText(swapTotal > 0.0
                            ? QString("%1 / %2").arg(formatBytes(qulonglong(swapUsed) * 1024ULL),
                                                     formatBytes(qulonglong(swapTotal) * 1024ULL))
                            : "No swap configured");
    memoryBar->setValues(memPercent, swapPercent);
}

void SystemMonitorPage::updateDisk()
{
    const QStorageInfo storage = QStorageInfo::root();
    const qulonglong total = qulonglong(storage.bytesTotal());
    const qulonglong free = qulonglong(storage.bytesAvailable());
    const qulonglong used = total > free ? total - free : 0;
    const double percent = total > 0 ? (100.0 * double(used) / double(total)) : 0.0;
    diskValue->setText(QString::number(percent, 'f', 0) + "%");
    diskDetail->setText(QString("%1 / %2").arg(formatBytes(used), formatBytes(total)));
}

void SystemMonitorPage::updateUptime()
{
    QFile uptimeFile("/proc/uptime");
    if (uptimeFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString text = QString::fromLocal8Bit(uptimeFile.readAll()).simplified();
        const double seconds = text.section(' ', 0, 0).toDouble();
        uptimeValue->setText(formatDuration(seconds));
        uptimeDetail->setText("System uptime");
    }

    QFile loadFile("/proc/loadavg");
    if (loadFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QStringList parts = QString::fromLocal8Bit(loadFile.readAll()).simplified().split(' ');
        if (parts.size() >= 3) {
            loadValue->setText(parts.mid(0, 3).join(" "));
            loadDetail->setText("1m 5m 15m");
        }
    }
}

void SystemMonitorPage::updateNetwork()
{
    NetworkSnapshot snapshot = readNetworkFromSysfs();
    if (!snapshot.valid) {
        snapshot = readNetworkFromProc();
    }

    const double elapsedSeconds = qMax(0.001, networkElapsed.restart() / 1000.0);
    const double rxRate = previousRxBytes > 0 && snapshot.rx >= previousRxBytes ? double(snapshot.rx - previousRxBytes) / elapsedSeconds : 0.0;
    const double txRate = previousTxBytes > 0 && snapshot.tx >= previousTxBytes ? double(snapshot.tx - previousTxBytes) / elapsedSeconds : 0.0;
    previousRxBytes = snapshot.rx;
    previousTxBytes = snapshot.tx;
    if (networkDetail) {
        const QString interfaceText = snapshot.interfaces.isEmpty() ? "none" : snapshot.interfaces.join(", ");
        networkDetail->setText(QString("Interfaces: %1\nTotal RX %2 / TX %3\nNow RX %4 / TX %5")
                                   .arg(interfaceText,
                                        formatBytes(snapshot.rx),
                                        formatBytes(snapshot.tx),
                                        formatRate(rxRate),
                                        formatRate(txRate)));
    }

    static QVector<double> rxHistory;
    static QVector<double> txHistory;
    rxHistory.append(rxRate);
    txHistory.append(txRate);
    while (rxHistory.size() > 60) {
        rxHistory.removeFirst();
    }
    while (txHistory.size() > 60) {
        txHistory.removeFirst();
    }

    double maxRate = 1.0;
    for (double value : rxHistory) {
        maxRate = qMax(maxRate, value);
    }
    for (double value : txHistory) {
        maxRate = qMax(maxRate, value);
    }
    if (networkGraph) {
        QVector<QVector<double>> networkSeries;
        networkSeries.append(rxHistory);
        networkSeries.append(txHistory);
        networkGraph->setSeries(networkSeries,
                                QStringList{QString("RX %1").arg(formatRate(rxRate)), QString("TX %1").arg(formatRate(txRate))},
                                QVector<QColor>{QColor("#E95420"), QColor("#2C7BE5")},
                                maxRate);
    }
}

QVector<CpuSnapshot> SystemMonitorPage::readCpuSnapshots() const
{
    QVector<CpuSnapshot> snapshots;
    QFile file("/proc/stat");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return snapshots;
    }

    const QList<QByteArray> lines = file.readAll().split('\n');
    for (const QByteArray& rawLine : lines) {
        const QString line = QString::fromLocal8Bit(rawLine).simplified();
        if (!line.startsWith("cpu")) {
            break;
        }
        const QStringList cols = line.split(' ');
        if (cols.size() < 5) {
            continue;
        }

        CpuSnapshot snapshot;
        for (int i = 1; i < cols.size(); ++i) {
            const unsigned long long value = cols.at(i).toULongLong();
            snapshot.total += value;
            if (i == 4 || i == 5) {
                snapshot.idle += value;
            }
        }
        snapshots.append(snapshot);
    }
    return snapshots;
}

QHash<QString, unsigned long long> SystemMonitorPage::readMeminfo() const
{
    QHash<QString, unsigned long long> values;
    QFile file("/proc/meminfo");
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QList<QByteArray> lines = file.readAll().split('\n');
        for (const QByteArray& rawLine : lines) {
            const QString line = QString::fromLocal8Bit(rawLine).trimmed();
            const int colon = line.indexOf(':');
            if (colon < 0) {
                continue;
            }
            const QString key = line.left(colon).trimmed();
            const unsigned long long value = firstNumber(line.mid(colon + 1));
            values.insert(key, value);
        }
    }

#ifdef Q_OS_UNIX
    if (values.value("MemTotal") == 0 || values.value("MemAvailable") == 0) {
        struct sysinfo info;
        if (sysinfo(&info) == 0) {
            const unsigned long long unit = info.mem_unit ? info.mem_unit : 1;
            const unsigned long long totalKb = info.totalram * unit / 1024ULL;
            const unsigned long long freeKb = (info.freeram + info.bufferram) * unit / 1024ULL;
            values.insert("MemTotal", totalKb);
            values.insert("MemAvailable", freeKb);
            values.insert("SwapTotal", info.totalswap * unit / 1024ULL);
            values.insert("SwapFree", info.freeswap * unit / 1024ULL);
        }
    }
#endif

    return values;
}

QString SystemMonitorPage::formatBytes(qulonglong bytes) const
{
    const QStringList units = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = double(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < units.size() - 1) {
        value /= 1024.0;
        ++unit;
    }
    return QString("%1 %2").arg(QString::number(value, unit == 0 ? 'f' : 'f', unit == 0 ? 0 : 1), units.at(unit));
}

QString SystemMonitorPage::formatRate(double bytesPerSecond) const
{
    return formatBytes(qulonglong(qMax(0.0, bytesPerSecond))) + "/s";
}

QString SystemMonitorPage::formatDuration(double seconds) const
{
    const int days = int(seconds) / 86400;
    const int hours = (int(seconds) % 86400) / 3600;
    const int minutes = (int(seconds) % 3600) / 60;
    if (days > 0) {
        return QString("%1d %2h").arg(days).arg(hours);
    }
    return QString("%1h %2m").arg(hours).arg(minutes);
}
