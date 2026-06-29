#include "ProcessNetworkPage.h"

#include "../components/FileExplorer.h"
#include "../components/SimpleFormDialog.h"

#include <QAbstractItemView>
#include <QAction>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHostAddress>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSplitter>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextCursor>
#include <QToolButton>
#include <QUdpSocket>
#include <QVBoxLayout>

namespace {
QString routeHexToIpv4(const QString& hex)
{
    bool ok = false;
    const quint32 value = hex.toUInt(&ok, 16);
    if (!ok || hex.size() > 8) {
        return hex;
    }

    return QString("%1.%2.%3.%4")
        .arg(value & 0xFF)
        .arg((value >> 8) & 0xFF)
        .arg((value >> 16) & 0xFF)
        .arg((value >> 24) & 0xFF);
}

QString routeFlagsText(const QString& hex)
{
    bool ok = false;
    const quint32 flags = hex.toUInt(&ok, 16);
    if (!ok) {
        return hex;
    }

    QString text;
    if (flags & 0x0001) text += "U";
    if (flags & 0x0002) text += "G";
    if (flags & 0x0004) text += "H";
    if (flags & 0x0010) text += "D";
    if (flags & 0x0020) text += "M";
    if (text.isEmpty()) text = "-";
    return text;
}

int routePrefixLength(const QString& maskHex)
{
    bool ok = false;
    quint32 mask = maskHex.toUInt(&ok, 16);
    if (!ok) {
        return -1;
    }

    mask = ((mask & 0x000000FF) << 24)
        | ((mask & 0x0000FF00) << 8)
        | ((mask & 0x00FF0000) >> 8)
        | ((mask & 0xFF000000) >> 24);

    int prefix = 0;
    while (mask & 0x80000000U) {
        ++prefix;
        mask <<= 1;
    }
    return prefix;
}

QString routeNetworkText(const QString& destinationHex, const QString& maskHex)
{
    const QString destination = routeHexToIpv4(destinationHex);
    const int prefix = routePrefixLength(maskHex);
    if (destination == "0.0.0.0" && prefix == 0) {
        return "default";
    }
    return prefix >= 0 ? QString("%1/%2").arg(destination).arg(prefix) : destination;
}

QString routeGatewayText(const QString& gatewayHex)
{
    const QString gateway = routeHexToIpv4(gatewayHex);
    return gateway == "0.0.0.0" ? "direct" : gateway;
}
}

ProcessNetworkPage::ProcessNetworkPage(CommandRunner* runner, QWidget* parent)
    : QWidget(parent)
    , commandRunner(runner)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    layout->addWidget(buildTabBar());

    tabPages = new QStackedWidget(this);
    tabPages->addWidget(buildProcessesTab());
    tabPages->addWidget(buildFilesTab());
    tabPages->addWidget(buildNetworkTab());
    tabPages->addWidget(buildSocketsTab());

    auto* topArea = new QWidget(this);
    auto* topLayout = new QVBoxLayout(topArea);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(0);
    topLayout->addWidget(tabPages);

    outputPanel = buildOutputPanel();
    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(topArea);
    splitter->addWidget(outputPanel);
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 1);
    splitter->setChildrenCollapsible(false);
    outputPanel->setMinimumHeight(48);
    outputPanel->setMaximumHeight(48);
    layout->addWidget(splitter, 1);

    connect(tabGroup, &QButtonGroup::idClicked, this, [this](int tab) {
        output->clear();
        outputPanel->setProperty("permissionError", false);
        outputPanel->style()->unpolish(outputPanel);
        outputPanel->style()->polish(outputPanel);
        tabPages->setCurrentIndex(tab);
        onTabEntered(tab);
    });
    connect(commandRunner, &CommandRunner::commandFinished, this, [this](const CommandResult& result) {
        if (result.action.startsWith("Lab2") || result.action.startsWith("DNS")) {
            applyCommandResult(result);
        }
    });
    connect(commandRunner, &CommandRunner::commandOutput, this, [this](const QString& action, const QString& text) {
        if ((action.startsWith("Lab2") || action.startsWith("DNS")) && !text.trimmed().isEmpty()) {
            output->appendPlainText(text.trimmed());
        }
    });

    onTabEntered(0);
}

QWidget* ProcessNetworkPage::buildTabBar()
{
    auto* widget = new QWidget(this);
    widget->setObjectName("tabBar");
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);
    tabGroup = new QButtonGroup(widget);
    const QStringList labels = {"Processes", "Files", "Network", "Sockets"};
    for (int i = 0; i < labels.size(); ++i) {
        auto* button = new QPushButton(labels.at(i), widget);
        button->setObjectName("tabBtn");
        button->setCheckable(true);
        tabGroup->addButton(button, i);
        layout->addWidget(button);
    }
    layout->addStretch();
    tabGroup->button(0)->setChecked(true);
    return widget;
}

QWidget* ProcessNetworkPage::buildProcessesTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* actionBar = new QHBoxLayout();
    auto* psButton = actionButton("ps");
    connect(psButton, &QPushButton::clicked, this, &ProcessNetworkPage::loadProcessList);
    actionBar->addWidget(psButton);

    auto* forkExecButton = actionButton("Fork / Exec");
    connect(forkExecButton, &QPushButton::clicked, this, [this]() {
        SimpleFormDialog dialog("Fork / Exec", {{"Command", "sleep 5"}}, false, this);
        if (dialog.exec() == QDialog::Accepted) {
            runLab2("start-process",
                    {dialog.values().value(0)},
                    "Lab2 start-process",
                    CommandType::LongRunning,
                    "Da tao tien trinh con PID=");
        }
    });
    actionBar->addWidget(forkExecButton);

    auto* killButton = actionButton("Kill");
    killButton->setObjectName("dangerBtn");
    connect(killButton, &QPushButton::clicked, this, [this]() {
        SimpleFormDialog dialog("Kill", {{"PID", "1234"}, {"Signal", "15"}}, true, this);
        if (dialog.exec() == QDialog::Accepted) runLab2("kill", dialog.values(), "Lab2 kill");
    });
    actionBar->addWidget(killButton);
    actionBar->addStretch();
    layout->addLayout(actionBar);

    procStateLabel = new QLabel("Dang tai danh sach tien trinh...", page);
    procStateLabel->setObjectName("card");
    procStateLabel->setAlignment(Qt::AlignCenter);
    procTable = new QTableWidget(0, 3, page);
    procTable->setHorizontalHeaderLabels({"PID", "Name", "Status"});
    procTable->horizontalHeader()->setStretchLastSection(true);
    procTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    procTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(procStateLabel);
    layout->addWidget(procTable, 1);
    return page;
}

QWidget* ProcessNetworkPage::buildFilesTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    const QString repoRoot = QDir(QString::fromUtf8(LAB_REPO_ROOT)).absolutePath();
    fileExplorer = new FileExplorer(repoRoot, page);
    connect(fileExplorer, &FileExplorer::listRequested, this, [this](const QString& path) {
        output->clear();
        commandRunner->run("ls", {"-la", path}, CommandType::OneShot, TrustLevel::Reliable, "Lab2 file-list");
    });

    auto* actionBar = new QHBoxLayout();
    actionBar->setSpacing(8);
    auto* statButton = actionButton("Stat");
    connect(statButton, &QPushButton::clicked, this, [this]() {
        runLab2("stat", {selectedFilePathOrCurrentDir()}, "Lab2 stat");
    });
    actionBar->addWidget(statButton);
    auto* readButton = actionButton("Read file");
    connect(readButton, &QPushButton::clicked, this, [this]() {
        currentReadFilePath = selectedFilePathOrCurrentDir();
        if (fileExplorer) {
            fileExplorer->showTextPreview("Read file", currentReadFilePath, "Dang doc file...");
        }
        runLab2("read-file", {currentReadFilePath}, "Lab2 read-file");
    });
    actionBar->addWidget(readButton);

    auto* refreshButton = actionButton("Refresh");
    connect(refreshButton, &QPushButton::clicked, fileExplorer, &FileExplorer::refreshList);
    actionBar->addWidget(refreshButton);

    auto* moreButton = new QToolButton(page);
    moreButton->setObjectName("moreBtn");
    moreButton->setText(QString::fromUtf8("More ▾"));
    moreButton->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(moreButton);
    menu->setObjectName("moreMenu");
    auto* showHidden = menu->addAction("Show hidden files");
    showHidden->setCheckable(true);
    connect(showHidden, &QAction::toggled, fileExplorer, &FileExplorer::setShowHidden);
    menu->addSeparator();
    menu->addAction("Write file...", this, [this]() {
        const QString current = selectedFilePathOrCurrentDir();
        const QString target = QFileInfo(current).isDir() ? QDir(current).filePath("new-file.txt") : current;
        SimpleFormDialog dialog("Write file", {{"Path", target}, {"Text", "hello"}}, false, this);
        if (dialog.exec() == QDialog::Accepted) {
            const auto values = dialog.values();
            runLab2("write-file", {filePathInCurrentDir(values.value(0)), values.value(1)}, "Lab2 write-file");
        }
    });
    menu->addAction("Copy file...", this, [this]() {
        const QString source = selectedFilePathOrCurrentDir();
        const QString destination = QDir(fileExplorer->currentPath()).filePath(QFileInfo(source).fileName() + ".copy");
        SimpleFormDialog dialog("Copy file", {{"Source", source}, {"Destination", destination}}, false, this);
        if (dialog.exec() == QDialog::Accepted) {
            const auto values = dialog.values();
            runLab2("copy-file", {filePathInCurrentDir(values.value(0)), filePathInCurrentDir(values.value(1))}, "Lab2 copy-file");
        }
    });
    menu->addAction("Chmod...", this, [this]() {
        SimpleFormDialog dialog("Chmod", {{"Path", selectedFilePathOrCurrentDir()}, {"Mode", "644"}}, false, this);
        if (dialog.exec() == QDialog::Accepted) {
            const auto values = dialog.values();
            runLab2("chmod", {filePathInCurrentDir(values.value(0)), values.value(1)}, "Lab2 chmod");
        }
    });
    menu->addSeparator();
    menu->addAction("Delete file...", this, [this]() {
        SimpleFormDialog dialog("Delete file", {{"Path", selectedFilePathOrCurrentDir()}}, true, this);
        if (dialog.exec() == QDialog::Accepted) {
            runLab2("delete-file", {filePathInCurrentDir(dialog.values().value(0)), "--yes"}, "Lab2 delete-file");
        }
    });
    moreButton->setMenu(menu);
    actionBar->addWidget(moreButton);
    actionBar->addStretch();
    layout->addLayout(actionBar);

    layout->addWidget(fileExplorer, 1);
    return page;
}

QWidget* ProcessNetworkPage::buildNetworkTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* actionBar = new QHBoxLayout();
    auto* interfacesButton = actionButton("Interfaces");
    connect(interfacesButton, &QPushButton::clicked, this, [this]() {
        runLab2("interfaces", {}, "Lab2 interfaces");
    });
    actionBar->addWidget(interfacesButton);

    auto* routesButton = actionButton("Routes");
    connect(routesButton, &QPushButton::clicked, this, [this]() {
        runLab2("routes", {}, "Lab2 routes");
    });
    actionBar->addWidget(routesButton);

    auto* resolveButton = actionButton("DNS Resolve");
    connect(resolveButton, &QPushButton::clicked, this, &ProcessNetworkPage::runDnsResolve);
    actionBar->addWidget(resolveButton);

    auto* dnsInfoButton = actionButton("DNS Info");
    connect(dnsInfoButton, &QPushButton::clicked, this, &ProcessNetworkPage::runDnsInfo);
    actionBar->addWidget(dnsInfoButton);

    auto* setDnsButton = actionButton("Set DNS");
    setDnsButton->setObjectName("dangerBtn");
    connect(setDnsButton, &QPushButton::clicked, this, &ProcessNetworkPage::runDnsSet);
    actionBar->addWidget(setDnsButton);
    actionBar->addStretch();
    layout->addLayout(actionBar);

    networkStack = new QStackedWidget(page);
    interfaceTable = new QTableWidget(0, 3, networkStack);
    interfaceTable->setHorizontalHeaderLabels({"Interface", "Family", "Address"});
    interfaceTable->horizontalHeader()->setStretchLastSection(true);
    interfaceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    interfaceTable->setSelectionMode(QAbstractItemView::SingleSelection);
    networkStack->addWidget(interfaceTable);

    networkTextView = new QPlainTextEdit(networkStack);
    networkTextView->setReadOnly(true);
    networkTextView->setLineWrapMode(QPlainTextEdit::NoWrap);
    networkTextView->setPlainText("Chon DNS Resolve hoac DNS Info de xem ket qua.");
    networkStack->addWidget(networkTextView);

    routeTable = new QTableWidget(0, 6, networkStack);
    routeTable->setHorizontalHeaderLabels({"Type", "Network", "Via", "Device", "Metric", "Flags"});
    routeTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    routeTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    routeTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    routeTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    routeTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    routeTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    routeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    routeTable->setSelectionMode(QAbstractItemView::SingleSelection);
    routeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    networkStack->addWidget(routeTable);

    layout->addWidget(networkStack, 1);
    return page;
}

#if 0
QWidget* ProcessNetworkPage::buildSocketsTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* actionBar = new QHBoxLayout();
    auto* tcpServer = actionButton("Start/Stop TCP server");
    connect(tcpServer, &QPushButton::clicked, this, [this]() {
        if (commandRunner->isRunning("Lab2 tcp-server")) {
            commandRunner->stop("Lab2 tcp-server");
            return;
        }
        SimpleFormDialog dialog("TCP server", {{"Port", "60000"}}, false, this);
        if (dialog.exec() == QDialog::Accepted) runLab2("tcp-server", dialog.values(), "Lab2 tcp-server", CommandType::LongRunning, "Dang listen TCP port");
    });
    auto* udpReceiver = actionButton("Start/Stop UDP receiver");
    connect(udpReceiver, &QPushButton::clicked, this, [this]() {
        if (commandRunner->isRunning("Lab2 udp-receiver")) {
            commandRunner->stop("Lab2 udp-receiver");
            return;
        }
        SimpleFormDialog dialog("UDP receiver", {{"Port", "60001"}}, false, this);
        if (dialog.exec() == QDialog::Accepted) runLab2("udp-receiver", dialog.values(), "Lab2 udp-receiver", CommandType::LongRunning, "Dang nhan UDP port");
    });
    auto* moreButton = new QToolButton(page);
    moreButton->setObjectName("moreBtn");
    moreButton->setText(QString::fromUtf8("More ▾"));
    moreButton->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(moreButton);
    menu->setObjectName("moreMenu");
    menu->addAction("TCP client test...", this, [this]() {
        SimpleFormDialog dialog("TCP client", {{"Host", "127.0.0.1"}, {"Port", "60000"}, {"Message", "hello"}}, false, this);
        if (dialog.exec() == QDialog::Accepted) runLab2("tcp-client", dialog.values(), "Lab2 tcp-client");
    });
    menu->addAction("UDP send...", this, [this]() {
        SimpleFormDialog dialog("UDP send", {{"IPv4", "127.0.0.1"}, {"Port", "60001"}, {"Message", "hello"}}, false, this);
        if (dialog.exec() == QDialog::Accepted) runLab2("udp-send", dialog.values(), "Lab2 udp-send");
    });
    moreButton->setMenu(menu);
    actionBar->addWidget(tcpServer);
    actionBar->addWidget(udpReceiver);
    actionBar->addWidget(moreButton);
    actionBar->addStretch();
    layout->addLayout(actionBar);
    auto* card = new QLabel("Socket server/receiver la long-running process va duoc ProcessRegistry track de dung khi dong app.", page);
    card->setObjectName("card");
    card->setWordWrap(true);
    layout->addWidget(card, 1);
    return page;
}

#endif

QWidget* ProcessNetworkPage::buildSocketsTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* cardsLayout = new QHBoxLayout();
    cardsLayout->setSpacing(12);
    cardsLayout->addWidget(buildSocketCard("TCP Server", true), 1);
    cardsLayout->addWidget(buildSocketCard("UDP Receiver", false), 1);
    layout->addLayout(cardsLayout, 1);

    updateSocketUi();
    return page;
}

QWidget* ProcessNetworkPage::buildSocketCard(const QString& title, bool tcpCard)
{
    auto* card = new QWidget(this);
    card->setObjectName("card");
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    auto* heading = new QLabel(title, card);
    heading->setObjectName("sectionHeading");
    layout->addWidget(heading);

    auto* sep1 = new QFrame(card);
    sep1->setFrameShape(QFrame::HLine);
    layout->addWidget(sep1);

    auto* status = new QLabel(card);
    layout->addWidget(status);

    auto* portRow = new QHBoxLayout();
    auto* portLabel = new QLabel("Port:", card);
    auto* portSpin = new QSpinBox(card);
    portSpin->setRange(1024, 65535);
    portSpin->setValue(tcpCard ? 8080 : 9090);
    portRow->addWidget(portLabel);
    portRow->addWidget(portSpin);
    portRow->addStretch();
    layout->addLayout(portRow);

    auto* toggle = new QPushButton(card);
    connect(toggle, &QPushButton::clicked, this, [this, tcpCard]() {
        const QString action = tcpCard ? "Lab2 tcp-server" : "Lab2 udp-receiver";
        if (commandRunner->isRunning(action)) {
            commandRunner->stop(action);
            updateSocketUi();
            return;
        }

        const QString port = QString::number(tcpCard ? tcpPortSpin->value() : udpPortSpin->value());
        runLab2(tcpCard ? "tcp-server" : "udp-receiver",
                {port},
                action,
                CommandType::LongRunning,
                tcpCard ? "Dang listen TCP port" : "Dang nhan UDP port");
        updateSocketUi();
    });
    layout->addWidget(toggle);
    layout->addStretch();

    auto* sep2 = new QFrame(card);
    sep2->setFrameShape(QFrame::HLine);
    layout->addWidget(sep2);

    auto* testButton = actionButton(tcpCard ? "TCP Client Test..." : "UDP Send...");
    connect(testButton, &QPushButton::clicked, this, [this, tcpCard]() {
        if (tcpCard) {
            SimpleFormDialog dialog("TCP client", {{"Host", "127.0.0.1"}, {"Port", QString::number(tcpPortSpin->value())}, {"Message", "hello"}}, false, this);
            if (dialog.exec() == QDialog::Accepted) runLab2("tcp-client", dialog.values(), "Lab2 tcp-client");
        } else {
            SimpleFormDialog dialog("UDP send", {{"IPv4", "127.0.0.1"}, {"Port", QString::number(udpPortSpin->value())}, {"Message", "hello"}}, false, this);
            if (dialog.exec() == QDialog::Accepted) runLab2("udp-send", dialog.values(), "Lab2 udp-send");
        }
    });
    layout->addWidget(testButton);

    if (tcpCard) {
        tcpStatusLabel = status;
        tcpPortSpin = portSpin;
        tcpToggleButton = toggle;
    } else {
        udpStatusLabel = status;
        udpPortSpin = portSpin;
        udpToggleButton = toggle;
    }

    return card;
}

QWidget* ProcessNetworkPage::buildOutputPanel()
{
    auto* panel = new QWidget(this);
    panel->setObjectName("outputPanel");
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* headerWidget = new QWidget(panel);
    headerWidget->setFixedHeight(48);
    auto* header = new QHBoxLayout(headerWidget);
    header->setContentsMargins(12, 4, 12, 4);
    auto* label = new QLabel("Output", headerWidget);
    auto* toggle = new QPushButton("Show", headerWidget);
    toggle->setObjectName("secondaryBtn");
    auto* clear = new QPushButton("Clear", headerWidget);
    clear->setObjectName("secondaryBtn");
    header->addWidget(label);
    header->addStretch();
    header->addWidget(toggle);
    header->addWidget(clear);
    layout->addWidget(headerWidget);
    output = new QPlainTextEdit(panel);
    output->setReadOnly(true);
    output->setLineWrapMode(QPlainTextEdit::NoWrap);
    output->setVisible(false);
    layout->addWidget(output, 1);
    connect(clear, &QPushButton::clicked, this, [this]() { output->clear(); });
    connect(toggle, &QPushButton::clicked, this, [this, panel, toggle]() {
        const bool willHide = output->isVisible();
        output->setVisible(!willHide);
        toggle->setText(willHide ? "Show" : "Hide");
        panel->setMaximumHeight(willHide ? 48 : QWIDGETSIZE_MAX);
        panel->setMinimumHeight(willHide ? 48 : 80);
    });
    return panel;
}

QPushButton* ProcessNetworkPage::actionButton(const QString& text)
{
    auto* button = new QPushButton(text, this);
    button->setObjectName("actionBarBtn");
    return button;
}

void ProcessNetworkPage::onTabEntered(int tab)
{
    if (tab == 0 && !processesLoaded) {
        loadProcessList();
    } else if (tab == 2 && !networkLoaded) {
        loadNetworkInfo();
    }
}

void ProcessNetworkPage::loadProcessList()
{
    if (processListInFlight) return;
    processListInFlight = true;
    runLab2("ps", {}, "Lab2 ps");
}

void ProcessNetworkPage::loadNetworkInfo()
{
    if (networkInFlight) return;
    networkInFlight = true;
    runLab2("interfaces", {}, "Lab2 interfaces");
}

void ProcessNetworkPage::showNetworkLoading(const QString& action)
{
    if (!networkStack) {
        return;
    }

    if (action == "Lab2 interfaces") {
        networkStack->setCurrentIndex(0);
        interfaceTable->setRowCount(0);
    } else if (action == "Lab2 routes") {
        networkStack->setCurrentIndex(2);
        routeTable->setRowCount(0);
    } else if (action == "Lab2 resolve" || action.startsWith("DNS")) {
        networkStack->setCurrentIndex(1);
        networkTextView->setPlainText("Dang tai...");
    }
}

void ProcessNetworkPage::populateInterfaceTable(const QString& outputText)
{
    if (!interfaceTable) {
        return;
    }

    interfaceTable->setRowCount(0);
    const QStringList lines = outputText.split('\n', Qt::SkipEmptyParts);
    for (const auto& rawLine : lines) {
        const QString line = rawLine.simplified();
        if (line.isEmpty() || line.startsWith("Interface ") || line.startsWith("---")) {
            continue;
        }

        const QStringList cols = line.split(' ');
        if (cols.size() < 3) {
            continue;
        }

        const int row = interfaceTable->rowCount();
        interfaceTable->insertRow(row);
        interfaceTable->setItem(row, 0, new QTableWidgetItem(cols.value(0)));
        interfaceTable->setItem(row, 1, new QTableWidgetItem(cols.value(1)));
        interfaceTable->setItem(row, 2, new QTableWidgetItem(cols.mid(2).join(' ')));
    }
}

void ProcessNetworkPage::populateRoutesTable(const QString& outputText)
{
    if (!routeTable) {
        return;
    }

    routeTable->setRowCount(0);

    const QStringList lines = outputText.split('\n', Qt::SkipEmptyParts);
    for (const QString& rawLine : lines) {
        const QString line = rawLine.simplified();
        if (line.isEmpty() || line.startsWith("Iface")) {
            continue;
        }

        const QStringList cols = line.split(' ', Qt::SkipEmptyParts);
        if (cols.size() < 11) {
            continue;
        }

        const QString network = routeNetworkText(cols.value(1), cols.value(7));
        const QString via = routeGatewayText(cols.value(2));
        const QString type = network == "default" ? "Default" : "Connected";

        const int row = routeTable->rowCount();
        routeTable->insertRow(row);
        routeTable->setItem(row, 0, new QTableWidgetItem(type));
        routeTable->setItem(row, 1, new QTableWidgetItem(network));
        routeTable->setItem(row, 2, new QTableWidgetItem(via));
        routeTable->setItem(row, 3, new QTableWidgetItem(cols.value(0)));
        routeTable->setItem(row, 4, new QTableWidgetItem(cols.value(6)));
        routeTable->setItem(row, 5, new QTableWidgetItem(routeFlagsText(cols.value(3))));
    }

    if (routeTable->rowCount() == 0) {
        routeTable->insertRow(0);
        routeTable->setItem(0, 0, new QTableWidgetItem("No routes"));
    }
}

void ProcessNetworkPage::runDnsInfo()
{
    output->clear();
    showNetworkLoading("DNS info");
    const QString script = QStringLiteral(
        "echo '=== /etc/resolv.conf ==='; "
        "cat /etc/resolv.conf 2>&1; "
        "echo; "
        "if command -v resolvectl >/dev/null 2>&1; then "
        "echo '=== resolvectl status ==='; resolvectl status 2>&1; "
        "else echo 'resolvectl: not found'; fi");
    commandRunner->run("sh", {"-lc", script}, CommandType::OneShot, TrustLevel::Reliable, "DNS info");
}

void ProcessNetworkPage::runDnsResolve()
{
    SimpleFormDialog dialog("DNS resolve", {{"Host", "example.com"}, {"DNS server (optional)", ""}}, false, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString host = dialog.values().value(0).trimmed();
    const QString server = dialog.values().value(1).trimmed();
    if (host.isEmpty()) {
        return;
    }

    output->clear();
    showNetworkLoading("DNS resolve");
    const QString script = QStringLiteral(
        "host=$1; server=$2; "
        "echo '=== Query ==='; echo \"Host: $host\"; "
        "if [ -n \"$server\" ]; then echo \"DNS server: $server\"; else echo 'DNS server: system default'; fi; echo; "
        "if command -v dig >/dev/null 2>&1; then "
        "if [ -n \"$server\" ]; then "
        "echo '=== A ==='; dig +noall +answer \"@$server\" \"$host\" A; echo; "
        "echo '=== AAAA ==='; dig +noall +answer \"@$server\" \"$host\" AAAA; echo; "
        "echo '=== CNAME/MX/NS/TXT ==='; dig +noall +answer \"@$server\" \"$host\" CNAME MX NS TXT; "
        "else "
        "echo '=== A ==='; dig +noall +answer \"$host\" A; echo; "
        "echo '=== AAAA ==='; dig +noall +answer \"$host\" AAAA; echo; "
        "echo '=== CNAME/MX/NS/TXT ==='; dig +noall +answer \"$host\" CNAME MX NS TXT; "
        "fi; "
        "elif [ -z \"$server\" ] && command -v resolvectl >/dev/null 2>&1; then "
        "resolvectl query \"$host\"; "
        "elif command -v nslookup >/dev/null 2>&1; then "
        "if [ -n \"$server\" ]; then nslookup \"$host\" \"$server\"; else nslookup \"$host\"; fi; "
        "elif [ -z \"$server\" ]; then "
        "getent ahosts \"$host\"; "
        "else echo 'Can dung dig hoac nslookup de resolve bang DNS server tuy chon.'; exit 127; fi");
    commandRunner->run("sh", {"-lc", script, "dns-resolve", host, server}, CommandType::OneShot, TrustLevel::Reliable, "DNS resolve");
}

void ProcessNetworkPage::runDnsSet()
{
    SimpleFormDialog dialog("Set DNS server",
                            {{"Interface", "ens33"}, {"DNS server", "8.8.8.8"}},
                            true,
                            this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString interface = dialog.values().value(0).trimmed();
    const QString server = dialog.values().value(1).trimmed();
    if (interface.isEmpty() || server.isEmpty()) {
        return;
    }

    output->clear();
    showNetworkLoading("DNS set");
    const QString script = QStringLiteral(
        "iface=$1; server=$2; "
        "if ! command -v resolvectl >/dev/null 2>&1; then "
        "echo 'resolvectl not found. Khong set DNS de tranh sua /etc/resolv.conf truc tiep.'; exit 127; fi; "
        "echo \"Set DNS server $server for interface $iface\"; "
        "resolvectl dns \"$iface\" \"$server\" && "
        "resolvectl domain \"$iface\" '~.' && "
        "resolvectl flush-caches && "
        "echo && resolvectl status \"$iface\"");
    commandRunner->run("sh", {"-lc", script, "dns-set", interface, server}, CommandType::OneShot, TrustLevel::Unreliable, "DNS set");
}

QString ProcessNetworkPage::filePathInCurrentDir(const QString& value) const
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty() || !fileExplorer) {
        return {};
    }

    QFileInfo info(trimmed);
    if (info.isAbsolute()) {
        return info.absoluteFilePath();
    }
    return QFileInfo(QDir(fileExplorer->currentPath()).filePath(trimmed)).absoluteFilePath();
}

QString ProcessNetworkPage::selectedFilePathOrCurrentDir() const
{
    if (!fileExplorer) {
        return {};
    }

    const QString selected = fileExplorer->selectedPath();
    return selected.isEmpty() ? fileExplorer->currentPath() : selected;
}

void ProcessNetworkPage::updateSocketUi()
{
    const bool tcpRunning = commandRunner && commandRunner->isRunning("Lab2 tcp-server");
    const bool udpRunning = commandRunner && commandRunner->isRunning("Lab2 udp-receiver");

    if (tcpStatusLabel) {
        tcpStatusLabel->setText(tcpRunning ? QString::fromUtf8("● Đang chạy") : QString::fromUtf8("● Dừng"));
    }
    if (udpStatusLabel) {
        udpStatusLabel->setText(udpRunning ? QString::fromUtf8("● Đang chạy") : QString::fromUtf8("● Dừng"));
    }
    if (tcpPortSpin) {
        tcpPortSpin->setEnabled(!tcpRunning);
    }
    if (udpPortSpin) {
        udpPortSpin->setEnabled(!udpRunning);
    }
    if (tcpToggleButton) {
        tcpToggleButton->setText(tcpRunning ? "Stop" : "Start TCP Server");
        tcpToggleButton->setObjectName(tcpRunning ? "dangerBtn" : "primaryBtn");
        tcpToggleButton->style()->unpolish(tcpToggleButton);
        tcpToggleButton->style()->polish(tcpToggleButton);
    }
    if (udpToggleButton) {
        udpToggleButton->setText(udpRunning ? "Stop" : "Start UDP Receiver");
        udpToggleButton->setObjectName(udpRunning ? "dangerBtn" : "primaryBtn");
        udpToggleButton->style()->unpolish(udpToggleButton);
        udpToggleButton->style()->polish(udpToggleButton);
    }
}

void ProcessNetworkPage::runLab2(const QString& subcommand, const QStringList& args, const QString& action, CommandType type, const QString& readySignal)
{
    output->clear();
    showNetworkLoading(action);
    const QString program = QDir(QString::fromUtf8(LAB_REPO_ROOT)).filePath("02_ubuntu_process_file_socket_network/ubuntu_sys_tool");
    commandRunner->run(program,
                       QStringList{"--cmd", subcommand} + args,
                       type,
                       TrustLevel::Reliable,
                       action,
                       QString(),
                       readySignal);
}

void ProcessNetworkPage::applyCommandResult(const CommandResult& result)
{
    const QString combined = result.output + result.error;
    if (result.action == "Lab2 ps") {
        processListInFlight = false;
        processesLoaded = true;
        procTable->setRowCount(0);
        const QStringList lines = result.output.split('\n', Qt::SkipEmptyParts);
        for (int i = 1; i < lines.size(); ++i) {
            const QStringList cols = lines.at(i).simplified().split(' ');
            if (cols.size() < 3) continue;
            const int row = procTable->rowCount();
            procTable->insertRow(row);
            procTable->setItem(row, 0, new QTableWidgetItem(cols.value(0)));
            procTable->setItem(row, 1, new QTableWidgetItem(cols.value(1)));
            procTable->setItem(row, 2, new QTableWidgetItem(cols.mid(2).join(' ')));
        }
        procStateLabel->setText(procTable->rowCount() ? "Danh sach tien trinh" : "Khong co du lieu tien trinh.");
    } else if (result.action == "Lab2 read-file") {
        if (fileExplorer) {
            fileExplorer->showTextPreview("Read file", currentReadFilePath, combined);
        }
    } else if (result.action == "Lab2 interfaces") {
        networkInFlight = false;
        networkLoaded = true;
        populateInterfaceTable(combined);
    } else if (result.action == "Lab2 routes") {
        if (routeTable) {
            networkStack->setCurrentIndex(2);
            populateRoutesTable(combined);
        }
    } else if (result.action == "Lab2 resolve") {
        if (networkTextView) {
            networkStack->setCurrentIndex(1);
            networkTextView->setPlainText(QString("=== System DNS resolve ===\n%1").arg(combined));
        }
    } else if (result.action.startsWith("DNS")) {
        if (networkTextView) {
            networkStack->setCurrentIndex(1);
            networkTextView->setPlainText(combined);
        }
    } else if (result.action == "Lab2 tcp-server" || result.action == "Lab2 udp-receiver") {
        updateSocketUi();
    }
    setOutput(combined, result.permissionError);
}

void ProcessNetworkPage::setOutput(const QString& text, bool permissionError)
{
    outputPanel->setProperty("permissionError", permissionError);
    outputPanel->style()->unpolish(outputPanel);
    outputPanel->style()->polish(outputPanel);
    output->setPlainText(text);
}
