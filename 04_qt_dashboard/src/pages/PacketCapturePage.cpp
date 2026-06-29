#include "PacketCapturePage.h"

#include <QAbstractItemModel>
#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QSplitter>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int MaxPacketRows = 2000;
constexpr int MaxPendingPacketLines = 1500;
constexpr int PacketFlushBatchSize = 60;
constexpr auto StreamAction = "PacketCapture stream";

bool isHiddenDetailSection(const QString& line)
{
    static const QStringList hiddenSections = {
        "Packet Summary",
        "Network",
        "Transport",
        "Payload",
    };
    return hiddenSections.contains(line);
}

bool isUsefulDetailField(const QString& key)
{
    static const QStringList usefulKeys = {
        "Direction",
        "Protocol",
        "Flow",
        "IP length",
        "TCP",
        "UDP",
        "ICMP",
        "Type/Code",
        "Identifier",
        "Sequence",
        "Seq",
        "Ack",
        "Flags",
        "Meaning",
        "Length",
        "Preview",
        "Fragment",
    };
    return usefulKeys.contains(key);
}
}

PacketCapturePage::PacketCapturePage(CommandRunner* runner, bool rootMode, QWidget* parent)
    : QWidget(parent)
    , commandRunner(runner)
    , rootMode(rootMode)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(rootMode ? buildRootModePage() : buildLimitedModePage(), 1);

    packetFlushTimer = new QTimer(this);
    packetFlushTimer->setInterval(16);
    connect(packetFlushTimer, &QTimer::timeout, this, &PacketCapturePage::flushPendingPackets);

    connect(commandRunner, &CommandRunner::commandFinished, this, [this](const CommandResult& result) {
        if (result.action.startsWith("PacketCapture")) {
            handleCommandResult(result);
        }
    });
    connect(commandRunner, &CommandRunner::commandOutput, this, [this](const QString& action, const QString& text) {
        if (action == StreamAction) {
            handleStreamOutput(text);
        }
    });
}

void PacketCapturePage::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    ensureCaptureStarted();
}

QWidget* PacketCapturePage::buildLimitedModePage()
{
    auto* card = new QWidget(this);
    card->setObjectName("card");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    auto* title = new QLabel("Packet Capture disabled in Limited mode", card);
    title->setObjectName("sectionHeading");
    auto* body = new QLabel("Run the dashboard as root to load the packet_capture module:\n\nxhost +SI:localuser:root && sudo -E QT_QPA_PLATFORM=xcb ./qt_dashboard", card);
    body->setWordWrap(true);
    layout->addWidget(title);
    layout->addWidget(body);
    layout->addStretch();
    return card;
}

QWidget* PacketCapturePage::buildRootModePage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* topArea = new QWidget(page);
    auto* topLayout = new QVBoxLayout(topArea);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(8);
    topLayout->addWidget(buildFilterPanel());

    statusLabel = new QLabel(topArea);
    statusLabel->setObjectName("card");
    statusLabel->setContentsMargins(12, 8, 12, 8);
    statusLabel->setVisible(false);
    topLayout->addWidget(statusLabel);

    packetTable = new QTableWidget(0, 7, topArea);
    packetTable->setHorizontalHeaderLabels({"Seq", "Time", "Dir", "Proto", "Source", "Destination", "Len"});
    if (auto* timeHeader = packetTable->horizontalHeaderItem(1)) {
        timeHeader->setToolTip("Elapsed time since the first displayed packet");
    }
    packetTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    packetTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    packetTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    packetTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    packetTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    packetTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    packetTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    packetTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    packetTable->setSelectionMode(QAbstractItemView::SingleSelection);
    packetTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(packetTable, &QTableWidget::itemSelectionChanged, this, &PacketCapturePage::loadSelectedPacketDetail);

    auto* contentSplitter = new QSplitter(Qt::Horizontal, topArea);
    contentSplitter->addWidget(packetTable);
    contentSplitter->addWidget(buildDetailPanel(contentSplitter));
    contentSplitter->setStretchFactor(0, 1);
    contentSplitter->setStretchFactor(1, 1);
    contentSplitter->setChildrenCollapsible(false);
    topLayout->addWidget(contentSplitter, 1);

    layout->addWidget(topArea, 1);

    updateStreamingUi();
    updateStatusLabel();
    return page;
}

QWidget* PacketCapturePage::buildFilterPanel()
{
    auto* card = new QWidget(this);
    card->setObjectName("card");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* filterRow = new QHBoxLayout();
    filterRow->setSpacing(8);

    filterRow->addWidget(new QLabel("Protocol", card));
    protocolCombo = new QComboBox(card);
    protocolCombo->addItems(QStringList{"any", "icmp", "tcp", "udp"});
    filterRow->addWidget(protocolCombo);

    filterRow->addWidget(new QLabel("Src IP", card));
    sourceIpEdit = new QLineEdit(card);
    sourceIpEdit->setPlaceholderText("any");
    sourceIpEdit->setText("any");
    filterRow->addWidget(sourceIpEdit, 1);

    filterRow->addWidget(new QLabel("Dst IP", card));
    destIpEdit = new QLineEdit(card);
    destIpEdit->setPlaceholderText("any");
    destIpEdit->setText("any");
    filterRow->addWidget(destIpEdit, 1);

    applyButton = actionButton("OK");
    applyButton->setObjectName("primaryBtn");
    applyButton->setFixedWidth(64);
    connect(applyButton, &QPushButton::clicked, this, &PacketCapturePage::submitFilterChange);
    filterRow->addWidget(applyButton);

    filterRow->addWidget(new QLabel("Direction", card));
    directionCombo = new QComboBox(card);
    directionCombo->addItems(QStringList{"both", "out", "in"});
    filterRow->addWidget(directionCombo);

    clearButton = actionButton("Clear");
    clearButton->setObjectName("dangerBtn");
    connect(clearButton, &QPushButton::clicked, this, &PacketCapturePage::clearCapture);
    filterRow->addWidget(clearButton);
    filterRow->addStretch();
    layout->addLayout(filterRow);

    connect(protocolCombo, &QComboBox::currentTextChanged, this, [this]() {
        restartCaptureWithFilter();
    });
    connect(directionCombo, &QComboBox::currentTextChanged, this, [this]() {
        restartCaptureWithFilter();
    });
    connect(sourceIpEdit, &QLineEdit::returnPressed, this, &PacketCapturePage::submitFilterChange);
    connect(destIpEdit,   &QLineEdit::returnPressed, this, &PacketCapturePage::submitFilterChange);

    return card;
}

QWidget* PacketCapturePage::buildDetailPanel(QWidget* parent)
{
    auto* panel = new QWidget(parent);
    panel->setObjectName("card");
    panel->setMinimumWidth(420);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);

    auto* title = new QLabel("Packet Detail", panel);
    title->setObjectName("sectionHeading");
    layout->addWidget(title);

    auto* detailSplitter = new QSplitter(Qt::Vertical, panel);

    auto* decodedPane = new QWidget(detailSplitter);
    auto* decodedLayout = new QVBoxLayout(decodedPane);
    decodedLayout->setContentsMargins(0, 0, 0, 0);
    decodedLayout->setSpacing(6);
    packetDetailTable = new QTableWidget(0, 2, decodedPane);
    packetDetailTable->setHorizontalHeaderLabels({"Field", "Value"});
    packetDetailTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    packetDetailTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    packetDetailTable->verticalHeader()->setVisible(false);
    packetDetailTable->setSelectionMode(QAbstractItemView::NoSelection);
    packetDetailTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    decodedLayout->addWidget(packetDetailTable, 1);
    detailSplitter->addWidget(decodedPane);

    auto* rawPane = new QWidget(detailSplitter);
    auto* rawLayout = new QVBoxLayout(rawPane);
    rawLayout->setContentsMargins(0, 0, 0, 0);
    rawLayout->setSpacing(6);
    auto* rawTitle = new QLabel("Raw Bytes", rawPane);
    rawTitle->setObjectName("sectionHeading");
    rawLayout->addWidget(rawTitle);
    packetHex = new QPlainTextEdit(rawPane);
    packetHex->setReadOnly(true);
    packetHex->setLineWrapMode(QPlainTextEdit::NoWrap);
    packetHex->setPlaceholderText("Hex / ASCII snapshot");
    rawLayout->addWidget(packetHex, 1);
    detailSplitter->addWidget(rawPane);

    detailSplitter->setStretchFactor(0, 1);
    detailSplitter->setStretchFactor(1, 1);
    detailSplitter->setChildrenCollapsible(false);
    layout->addWidget(detailSplitter, 1);

    return panel;
}

QPushButton* PacketCapturePage::actionButton(const QString& text)
{
    auto* button = new QPushButton(text, this);
    button->setObjectName("actionBarBtn");
    return button;
}

QString PacketCapturePage::captureDirectory() const
{
    return QDir(QString::fromUtf8(LAB_REPO_ROOT)).filePath("03_kernel_module_networking/packet_capture");
}

QString PacketCapturePage::captureToolPath() const
{
    return QDir(captureDirectory()).filePath("capturectl");
}

QString PacketCapturePage::modulePath() const
{
    return QDir(captureDirectory()).filePath("packet_capture.ko");
}

QString PacketCapturePage::protocolValue() const
{
    return protocolCombo ? protocolCombo->currentText() : "any";
}

QString PacketCapturePage::directionValue() const
{
    return directionCombo ? directionCombo->currentText() : "both";
}

QString PacketCapturePage::sourceIpValue() const
{
    const QString value = sourceIpEdit ? sourceIpEdit->text().trimmed().toLower() : QString();
    return value.isEmpty() ? "any" : value;
}

QString PacketCapturePage::destIpValue() const
{
    const QString value = destIpEdit ? destIpEdit->text().trimmed().toLower() : QString();
    return value.isEmpty() ? "any" : value;
}

QString PacketCapturePage::formatPacketTime(quint64 timestampNs)
{
    if (timestampNs == 0) {
        return "-";
    }

    if (firstTimestampNs == 0 || timestampNs < firstTimestampNs) {
        firstTimestampNs = timestampNs;
    }

    const quint64 deltaMs = (timestampNs - firstTimestampNs) / 1000000ULL;
    const quint64 hours = deltaMs / 3600000ULL;
    const quint64 minutes = (deltaMs / 60000ULL) % 60ULL;
    const quint64 seconds = (deltaMs / 1000ULL) % 60ULL;
    const quint64 milliseconds = deltaMs % 1000ULL;

    if (hours > 0) {
        return QString("%1:%2:%3.%4")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'))
            .arg(milliseconds, 3, 10, QChar('0'));
    }

    return QString("%1:%2.%3")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'))
        .arg(milliseconds, 3, 10, QChar('0'));
}

bool PacketCapturePage::isValidIpv4OrAny(const QString& value) const
{
    if (value == "any") {
        return true;
    }

    const QStringList parts = value.split('.');
    if (parts.size() != 4) {
        return false;
    }

    for (const auto& part : parts) {
        bool ok = false;
        const int octet = part.toInt(&ok);
        if (!ok || octet < 0 || octet > 255) {
            return false;
        }
    }

    return true;
}

bool PacketCapturePage::moduleLoadSucceeded(const CommandResult& result) const
{
    const QString combined = result.output + result.error;
    return result.exitCode == 0
        || combined.contains("File exists", Qt::CaseInsensitive)
        || combined.contains("already", Qt::CaseInsensitive);
}

bool PacketCapturePage::isIoctlMismatch(const QString& text) const
{
    return text.contains("Inappropriate ioctl", Qt::CaseInsensitive)
        || text.contains("ENOTTY", Qt::CaseInsensitive);
}

void PacketCapturePage::ensureCaptureStarted()
{
    if (!rootMode || autoStartRequested) {
        return;
    }

    autoStartRequested = true;
    commandRunner->run("insmod",
                       {modulePath()},
                       CommandType::OneShot,
                       TrustLevel::Unreliable,
                       "PacketCapture auto-load",
                       captureDirectory());
}

void PacketCapturePage::reloadCaptureModule()
{
    if (reloadingAfterIoctlMismatch) {
        return;
    }

    pauseStream();
    clearTable();
    startAfterApply = false;
    restartAfterClear = false;
    reloadingAfterIoctlMismatch = true;
    setOutput("Kernel module dang la ban cu. Dang reload packet_capture de khop voi GUI...");
    commandRunner->run("rmmod",
                       {"packet_capture"},
                       CommandType::OneShot,
                       TrustLevel::Unreliable,
                       "PacketCapture reload-unload",
                       captureDirectory());
}

void PacketCapturePage::applyFilterAndMaybeStart(bool shouldStart)
{
    const QString sourceIp = sourceIpValue();
    const QString destIp   = destIpValue();
    if (!isValidIpv4OrAny(sourceIp)) {
        setOutput(QString("Invalid source IPv4 address: %1").arg(sourceIp), true);
        return;
    }
    if (!isValidIpv4OrAny(destIp)) {
        setOutput(QString("Invalid dest IPv4 address: %1").arg(destIp), true);
        return;
    }

    startAfterApply = shouldStart;
    QStringList args{"set", protocolValue(), sourceIp, directionValue()};
    if (destIp != "any")
        args << "--dst-ip" << destIp;
    runCaptureCtl(args, "PacketCapture apply-filter");
}

void PacketCapturePage::restartCaptureWithFilter()
{
    if (!rootMode || !autoStartRequested || (!streaming && !commandRunner->isRunning(StreamAction))) {
        return;
    }

    const QString sourceIp = sourceIpValue();
    const QString destIp   = destIpValue();
    if (!isValidIpv4OrAny(sourceIp)) {
        setOutput(QString("Invalid source IPv4 address: %1").arg(sourceIp), true);
        return;
    }
    if (!isValidIpv4OrAny(destIp)) {
        setOutput(QString("Invalid dest IPv4 address: %1").arg(destIp), true);
        return;
    }

    pauseStream();
    clearTable();
    runCaptureCtl({"clear"}, "PacketCapture clear-filter-change");
}

void PacketCapturePage::submitFilterChange()
{
    if (!rootMode) {
        return;
    }

    if (!autoStartRequested) {
        ensureCaptureStarted();
        return;
    }

    if (streaming || commandRunner->isRunning(StreamAction)) {
        restartCaptureWithFilter();
        return;
    }

    const QString sourceIp = sourceIpValue();
    const QString destIp   = destIpValue();
    if (!isValidIpv4OrAny(sourceIp)) {
        setOutput(QString("Invalid source IPv4 address: %1").arg(sourceIp), true);
        return;
    }
    if (!isValidIpv4OrAny(destIp)) {
        setOutput(QString("Invalid dest IPv4 address: %1").arg(destIp), true);
        return;
    }

    clearTable();
    runCaptureCtl({"clear"}, "PacketCapture clear-filter-change");
}

void PacketCapturePage::startStream()
{
    if (streaming) {
        return;
    }

    streamBuffer.clear();
    streamStopRequested = false;
    streaming = true;
    updateStreamingUi();
    setOutput({});
    runCaptureCtl({"stream", "--since", QString::number(lastSeq), "--interval-ms", "100"},
                  StreamAction,
                  CommandType::LongRunning);
}

void PacketCapturePage::pauseStream()
{
    if (!streaming || !commandRunner->isRunning(StreamAction)) {
        streaming = false;
        updateStreamingUi();
        return;
    }

    streamStopRequested = true;
    commandRunner->stop(StreamAction);
}

void PacketCapturePage::clearCapture()
{
    const bool wasStreaming = streaming;
    pauseStream();
    clearTable();
    restartAfterClear = wasStreaming;
    runCaptureCtl({"clear"}, "PacketCapture clear");
}

void PacketCapturePage::runCaptureCtl(const QStringList& args, const QString& action, CommandType type)
{
    commandRunner->run(captureToolPath(),
                       args,
                       type,
                       TrustLevel::Unreliable,
                       action,
                       captureDirectory());
}

void PacketCapturePage::handleCommandResult(const CommandResult& result)
{
    const QString combined = result.output + result.error;

    if (result.action == StreamAction) {
        streaming = false;
        streamBuffer.clear();
        updateStreamingUi();
        if (!streamStopRequested && result.exitCode != 0 && isIoctlMismatch(combined)) {
            streamStopRequested = false;
            reloadCaptureModule();
            return;
        }
        if (!streamStopRequested && result.exitCode != 0) {
            const QString message = result.error.trimmed().isEmpty()
                ? QStringLiteral("Packet capture stream stopped.")
                : result.error;
            setOutput(message, result.permissionError);
        }
        streamStopRequested = false;
        return;
    }

    if (result.action == "PacketCapture auto-load") {
        if (moduleLoadSucceeded(result)) {
            setOutput({});
            clearTable();
            runCaptureCtl({"clear"}, "PacketCapture clear-auto-start");
        } else {
            setOutput(combined, result.permissionError);
        }
        return;
    }

    if (result.action == "PacketCapture reload-unload") {
        const bool unloadOk = result.exitCode == 0
            || combined.contains("No such", Qt::CaseInsensitive)
            || combined.contains("not currently loaded", Qt::CaseInsensitive)
            || combined.contains("not loaded", Qt::CaseInsensitive);
        if (unloadOk) {
            commandRunner->run("insmod",
                               {modulePath()},
                               CommandType::OneShot,
                               TrustLevel::Unreliable,
                               "PacketCapture reload-load",
                               captureDirectory());
        } else {
            reloadingAfterIoctlMismatch = false;
            setOutput(QString("Khong reload duoc packet_capture module:\n%1").arg(combined.trimmed()), result.permissionError);
        }
        return;
    }

    if (result.action == "PacketCapture reload-load") {
        reloadingAfterIoctlMismatch = false;
        if (moduleLoadSucceeded(result)) {
            setOutput({});
            runCaptureCtl({"clear"}, "PacketCapture clear-auto-start");
        } else {
            setOutput(QString("Khong load duoc packet_capture. Hay build lai module bang make roi chay lai GUI:\n%1").arg(combined.trimmed()),
                      result.permissionError);
        }
        return;
    }

    if (result.action == "PacketCapture clear-auto-start" || result.action == "PacketCapture clear-filter-change") {
        if (result.exitCode == 0) {
            setOutput({});
            applyFilterAndMaybeStart(true);
        } else if (isIoctlMismatch(combined)) {
            reloadCaptureModule();
        } else {
            setOutput(combined, result.permissionError);
        }
        return;
    }

    if (result.action == "PacketCapture detail") {
        if (result.exitCode == 0) {
            const QString detail = result.output.trimmed();
            const QString firstLine = detail.section('\n', 0, 0).trimmed();
            if (pendingDetailSeq.isEmpty() || firstLine == QString("SEQ %1").arg(pendingDetailSeq)) {
                setPacketDetail(detail);
            }
        } else {
            setPacketDetail(combined.trimmed());
        }
    } else if (result.action == "PacketCapture stats") {
        parseStatsOutput(combined);
    } else if (result.action == "PacketCapture clear") {
        parseStatsOutput(combined);
        if (result.exitCode == 0 && restartAfterClear) {
            setOutput({});
            restartAfterClear = false;
            startStream();
        } else {
            restartAfterClear = false;
            if (result.exitCode != 0) {
                setOutput(combined, result.permissionError);
            } else {
                setOutput({});
            }
        }
    } else if (result.action == "PacketCapture apply-filter" && result.exitCode == 0) {
        setOutput({});
        if (startAfterApply) {
            startAfterApply = false;
            startStream();
        }
    } else if (result.action == "PacketCapture apply-filter") {
        startAfterApply = false;
        if (isIoctlMismatch(combined)) {
            reloadCaptureModule();
            return;
        }
        setOutput(combined, result.permissionError);
    }
}

void PacketCapturePage::loadSelectedPacketDetail()
{
    if (!packetTable) {
        return;
    }

    const int row = packetTable->currentRow();
    if (row < 0) {
        return;
    }

    auto* seqItem = packetTable->item(row, 0);
    if (!seqItem) {
        return;
    }

    const QString seq = seqItem->text().trimmed();
    if (seq.isEmpty()) {
        return;
    }

    pendingDetailSeq = seq;
    setPacketDetail(QString("Loading packet %1...").arg(seq));
    runCaptureCtl({"detail", seq}, "PacketCapture detail");
}

void PacketCapturePage::setPacketDetail(const QString& text)
{
    const QString marker = "\nHex / ASCII Snapshot\n";
    const int markerIndex = text.indexOf(marker);

    if (markerIndex >= 0) {
        populatePacketDetailTable(text.left(markerIndex).trimmed());
        if (packetHex) {
            packetHex->setPlainText(text.mid(markerIndex + marker.length()).trimmed());
        }
        return;
    }

    populatePacketDetailTable(text.trimmed());
    if (packetHex) {
        packetHex->clear();
    }
}

void PacketCapturePage::populatePacketDetailTable(const QString& text)
{
    if (!packetDetailTable) {
        return;
    }

    packetDetailTable->setUpdatesEnabled(false);
    packetDetailTable->setRowCount(0);
    packetDetailTable->clearSpans();

    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const auto& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        QString key;
        QString value;

        if (line.startsWith("SEQ ")) {
            continue;
        } else {
            const int colon = line.indexOf(':');
            if (colon > 0) {
                key = line.left(colon).trimmed();
                value = line.mid(colon + 1).trimmed();
            } else {
                if (isHiddenDetailSection(line)) {
                    continue;
                }

                const int space = line.indexOf(' ');
                if (space > 0) {
                    key = line.left(space).trimmed();
                    value = line.mid(space + 1).trimmed();
                } else {
                    continue;
                }
            }
        }

        if (!isUsefulDetailField(key)) {
            continue;
        }

        const int row = packetDetailTable->rowCount();
        packetDetailTable->insertRow(row);

        auto* keyItem = new QTableWidgetItem(key);
        auto* valueItem = new QTableWidgetItem(value);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        packetDetailTable->setItem(row, 0, keyItem);
        packetDetailTable->setItem(row, 1, valueItem);
    }

    if (packetDetailTable->rowCount() == 0 && !text.isEmpty()) {
        packetDetailTable->insertRow(0);
        packetDetailTable->setItem(0, 0, new QTableWidgetItem("Status"));
        packetDetailTable->setItem(0, 1, new QTableWidgetItem(text));
    }

    packetDetailTable->resizeRowsToContents();
    packetDetailTable->setUpdatesEnabled(true);
}

void PacketCapturePage::handleStreamOutput(const QString& text)
{
    streamBuffer.append(text);
    streamBuffer.remove('\r');

    const QStringList lines = streamBuffer.split('\n');
    streamBuffer = lines.isEmpty() ? QString() : lines.last();

    for (int i = 0; i + 1 < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        if (line.isEmpty() || line.startsWith("SEQ ")) {
            continue;
        }

        queuePacketLine(line);
    }
}

void PacketCapturePage::queuePacketLine(const QString& line)
{
    pendingPacketLines.append(line);
    while (pendingPacketLines.size() > MaxPendingPacketLines) {
        pendingPacketLines.removeFirst();
        ++overwrittenRecords;
    }

    if (packetFlushTimer && !packetFlushTimer->isActive()) {
        packetFlushTimer->start();
    }
}

void PacketCapturePage::flushPendingPackets()
{
    if (!packetTable || pendingPacketLines.isEmpty()) {
        if (packetFlushTimer) {
            packetFlushTimer->stop();
        }
        return;
    }

    packetTable->setUpdatesEnabled(false);
    int processed = 0;
    while (!pendingPacketLines.isEmpty() && processed < PacketFlushBatchSize) {
        const QString line = pendingPacketLines.takeFirst();
        appendPacketLine(line);
        ++processed;
    }
    trimPacketRows();
    packetTable->setUpdatesEnabled(true);
    packetTable->scrollToBottom();
    updateStatusLabel();

    if (pendingPacketLines.isEmpty()) {
        packetFlushTimer->stop();
    }
}

bool PacketCapturePage::appendPacketLine(const QString& line)
{
    const QStringList columns = line.simplified().split(' ');
    if (columns.size() != 9) {
        return false;
    }

    bool seqOk = false;
    const quint64 seq = columns.value(0).toULongLong(&seqOk);
    if (!seqOk) {
        return false;
    }

    bool timestampOk = false;
    const quint64 timestampNs = columns.value(1).toULongLong(&timestampOk);
    const QString displayedTime = timestampOk ? formatPacketTime(timestampNs) : columns.value(1);

    const int row = packetTable->rowCount();
    packetTable->insertRow(row);

    const bool hasPorts = columns.value(3) == "tcp" || columns.value(3) == "udp";
    const QString source = hasPorts ? QString("%1:%2").arg(columns.value(4), columns.value(5)) : columns.value(4);
    const QString destination = hasPorts ? QString("%1:%2").arg(columns.value(6), columns.value(7)) : columns.value(6);
    const QStringList values = {
        columns.value(0),
        displayedTime,
        columns.value(2),
        columns.value(3),
        source,
        destination,
        columns.value(8),
    };

    for (int col = 0; col < values.size(); ++col) {
        auto* item = new QTableWidgetItem(values.at(col));
        if (col == 0) {
            item->setData(Qt::UserRole, line);
        }
        if (col == 1 && timestampOk) {
            item->setData(Qt::UserRole, timestampNs);
            item->setToolTip(QString("%1 ns").arg(columns.value(1)));
        }
        packetTable->setItem(row, col, item);
    }

    if (lastSeq != 0 && seq > lastSeq + 1) {
        overwrittenRecords += seq - lastSeq - 1;
    }
    lastSeq = seq;

    return true;
}

void PacketCapturePage::trimPacketRows()
{
    if (!packetTable) {
        return;
    }

    const int surplusRows = packetTable->rowCount() - MaxPacketRows;
    if (surplusRows <= 0) {
        return;
    }

    if (!packetTable->model()->removeRows(0, surplusRows)) {
        for (int i = 0; i < surplusRows; ++i) {
            packetTable->removeRow(0);
        }
    }
}

void PacketCapturePage::clearTable()
{
    if (packetTable) {
        packetTable->setRowCount(0);
    }
    pendingPacketLines.clear();
    pendingDetailSeq.clear();
    setPacketDetail({});
    lastSeq = 0;
    firstTimestampNs = 0;
    overwrittenRecords = 0;
    updateStatusLabel();
}

void PacketCapturePage::parseStatsOutput(const QString& text)
{
    const QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    for (const auto& rawLine : lines) {
        const QString line = rawLine.trimmed();
        if (!line.startsWith("overwritten_records:")) {
            continue;
        }

        bool ok = false;
        const quint64 value = line.section(':', 1).trimmed().toULongLong(&ok);
        if (ok) {
            overwrittenRecords = value;
        }
    }
    updateStatusLabel();
}

void PacketCapturePage::updateStreamingUi()
{
    // Capture starts automatically, so there are no visible stream controls.
}

void PacketCapturePage::updateStatusLabel()
{
    // The page intentionally stays quiet while capture is healthy.
}

void PacketCapturePage::setOutput(const QString& text, bool permissionError)
{
    if (!statusLabel) {
        return;
    }

    const QString trimmed = text.trimmed();
    statusLabel->setVisible(!trimmed.isEmpty());
    statusLabel->setText(trimmed);
    statusLabel->setProperty("permissionError", permissionError);
    statusLabel->style()->unpolish(statusLabel);
    statusLabel->style()->polish(statusLabel);
}
