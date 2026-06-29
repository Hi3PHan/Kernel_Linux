#include "ShellAutomationPage.h"

#include "../components/FileExplorer.h"
#include "../components/SimpleFormDialog.h"

#include <QAbstractItemView>
#include <QAction>
#include <QButtonGroup>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QVBoxLayout>

ShellAutomationPage::ShellAutomationPage(CommandRunner* runner, bool rootMode, QWidget* parent)
    : QWidget(parent)
    , commandRunner(runner)
    , rootMode(rootMode)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    layout->addWidget(buildTabBar());

    tabPages = new QStackedWidget(this);
    tabPages->addWidget(buildFilesTab());
    tabPages->addWidget(buildCronTab());
    tabPages->addWidget(buildTimeTab());
    tabPages->addWidget(buildPackagesTab());

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
        tabPages->setCurrentIndex(tab);
        onTabEntered(tab);
    });
    connect(commandRunner, &CommandRunner::commandFinished, this, [this](const CommandResult& result) {
        if (result.action.startsWith("Lab1")) {
            applyCommandResult(result);
        }
    });
    connect(commandRunner, &CommandRunner::commandOutput, this, [this](const QString& action, const QString& text) {
        if (action.startsWith("Lab1") && !text.trimmed().isEmpty()) {
            output->appendPlainText(text.trimmed());
        }
    });

    onTabEntered(0);
}

QWidget* ShellAutomationPage::buildTabBar()
{
    auto* widget = new QWidget(this);
    widget->setObjectName("tabBar");
    auto* layout = new QHBoxLayout(widget);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    tabGroup = new QButtonGroup(widget);
    const QStringList labels = {"Files", "Cron Jobs", "System Time", "Packages"};
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

QWidget* ShellAutomationPage::buildFilesTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    const QString repoRoot = QDir(QString::fromUtf8(LAB_REPO_ROOT)).absolutePath();
    fileExplorer = new FileExplorer(repoRoot, page);
    connect(fileExplorer, &FileExplorer::listRequested, this, &ShellAutomationPage::loadFilesList);

    auto* actionBar = new QHBoxLayout();
    actionBar->setSpacing(8);
    auto* listButton = actionButton("List");
    connect(listButton, &QPushButton::clicked, this, &ShellAutomationPage::loadFilesList);
    actionBar->addWidget(listButton);

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
    menu->addAction("Create file...", this, [this]() {
        SimpleFormDialog dialog("Create file", {{"Name", "new-file.txt"}}, false, this);
        if (dialog.exec() == QDialog::Accepted) runLab1("create-file", {pathInCurrentDir(dialog.values().value(0))}, "Lab1 Create file");
    });
    menu->addAction("Create directory...", this, [this]() {
        SimpleFormDialog dialog("Create directory", {{"Name", "new-folder"}}, false, this);
        if (dialog.exec() == QDialog::Accepted) runLab1("create-dir", {pathInCurrentDir(dialog.values().value(0))}, "Lab1 Create directory");
    });
    menu->addSeparator();
    menu->addAction("Copy...", this, [this]() {
        const QString selected = fileExplorer->selectedPath();
        if (!selected.isEmpty()) {
            SimpleFormDialog dialog("Copy selected", {{"Destination", QDir(fileExplorer->currentPath()).filePath(QFileInfo(selected).fileName())}}, false, this);
            if (dialog.exec() == QDialog::Accepted) runLab1("copy", {selected, pathInCurrentDir(dialog.values().value(0))}, "Lab1 Copy");
            return;
        }
        SimpleFormDialog dialog("Copy", {{"Source", fileExplorer->currentPath()}, {"Destination", fileExplorer->currentPath()}}, false, this);
        if (dialog.exec() == QDialog::Accepted) {
            const auto values = dialog.values();
            runLab1("copy", {pathInCurrentDir(values.value(0)), pathInCurrentDir(values.value(1))}, "Lab1 Copy");
        }
    });
    menu->addAction("Move / Rename...", this, [this]() {
        const QString selected = fileExplorer->selectedPath();
        if (!selected.isEmpty()) {
            SimpleFormDialog dialog("Move / Rename selected", {{"New name or destination", QFileInfo(selected).fileName()}}, false, this);
            if (dialog.exec() == QDialog::Accepted) runLab1("move", {selected, pathInCurrentDir(dialog.values().value(0))}, "Lab1 Move");
            return;
        }
        SimpleFormDialog dialog("Move / Rename", {{"Source", fileExplorer->currentPath()}, {"Destination", fileExplorer->currentPath()}}, false, this);
        if (dialog.exec() == QDialog::Accepted) {
            const auto values = dialog.values();
            runLab1("move", {pathInCurrentDir(values.value(0)), pathInCurrentDir(values.value(1))}, "Lab1 Move");
        }
    });
    menu->addSeparator();
    menu->addAction("Find...", this, [this]() {
        SimpleFormDialog dialog("Find in current folder", {{"Pattern", "*.txt"}}, false, this);
        if (dialog.exec() == QDialog::Accepted) runLab1("find", {fileExplorer->currentPath(), dialog.values().value(0)}, "Lab1 Find");
    });
    menu->addAction("Backup...", this, [this]() {
        SimpleFormDialog dialog("Backup current folder", {{"Destination directory", QDir(fileExplorer->currentPath()).filePath("backups")}}, false, this);
        if (dialog.exec() == QDialog::Accepted) runLab1("backup", {fileExplorer->currentPath(), pathInCurrentDir(dialog.values().value(0))}, "Lab1 Backup");
    });
    menu->addSeparator();
    menu->addAction("Delete...", this, [this]() {
        const QString selected = fileExplorer->selectedPath();
        if (!selected.isEmpty()) {
            SimpleFormDialog dialog("Delete selected", {{"Selected path", selected}}, true, this);
            if (dialog.exec() == QDialog::Accepted) runLab1("delete", {selected, "--yes"}, "Lab1 Delete");
            return;
        }
        SimpleFormDialog dialog("Delete", {{"Path", fileExplorer->currentPath()}}, true, this);
        if (dialog.exec() == QDialog::Accepted) runLab1("delete", {pathInCurrentDir(dialog.values().value(0)), "--yes"}, "Lab1 Delete");
    });
    moreButton->setMenu(menu);
    actionBar->addWidget(moreButton);
    actionBar->addStretch();
    layout->addLayout(actionBar);
    layout->addWidget(fileExplorer, 1);
    return page;
}

QWidget* ShellAutomationPage::buildCronTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* actionBar = new QHBoxLayout();
    actionBar->setSpacing(8);
    auto* listButton = actionButton("Cron List");
    connect(listButton, &QPushButton::clicked, this, &ShellAutomationPage::loadCronList);
    actionBar->addWidget(listButton);
    auto* moreButton = new QToolButton(page);
    moreButton->setObjectName("moreBtn");
    moreButton->setText(QString::fromUtf8("More ▾"));
    moreButton->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(moreButton);
    menu->setObjectName("moreMenu");
    menu->addAction("Cron Add...", this, [this]() {
        SimpleFormDialog dialog("Cron Add", {{"Name", "job-name"}, {"Expression", "*/5 * * * *"}, {"Command", "/path/to/script.sh"}}, false, this);
        if (dialog.exec() == QDialog::Accepted) runLab1("cron-add", dialog.values(), "Lab1 Cron Add");
    });
    menu->addAction("Cron Remove...", this, [this]() {
        SimpleFormDialog dialog("Cron Remove", {{"Name", "job-name"}}, true, this);
        if (dialog.exec() == QDialog::Accepted) runLab1("cron-remove", dialog.values() << "--yes", "Lab1 Cron Remove");
    });
    moreButton->setMenu(menu);
    actionBar->addWidget(moreButton);
    actionBar->addStretch();
    layout->addLayout(actionBar);

    cronStateLabel = new QLabel("Dang tai danh sach cron...", page);
    cronStateLabel->setObjectName("card");
    cronStateLabel->setAlignment(Qt::AlignCenter);
    cronTable = new QTableWidget(0, 3, page);
    cronTable->setHorizontalHeaderLabels({"Name", "Expression", "Command"});
    cronTable->horizontalHeader()->setStretchLastSection(true);
    cronTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    cronTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(cronStateLabel);
    layout->addWidget(cronTable, 1);
    return page;
}

QWidget* ShellAutomationPage::buildTimeTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* actionBar = new QHBoxLayout();
    actionBar->setSpacing(8);
    auto* showButton = actionButton("Time Show");
    connect(showButton, &QPushButton::clicked, this, &ShellAutomationPage::loadTimeShow);
    actionBar->addWidget(showButton);

    auto* moreButton = new QToolButton(page);
    moreButton->setObjectName("moreBtn");
    moreButton->setText(QString::fromUtf8("More ▾"));
    moreButton->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(moreButton);
    menu->setObjectName("moreMenu");
    menu->addAction("Timezone Set...", this, [this]() {
        showTimezoneDialog();
    });
    menu->addAction("Time Set...", this, [this]() {
        SimpleFormDialog dialog("Time Set", {{"Time", "YYYY-MM-DD HH:MM:SS"}}, true, this);
        if (dialog.exec() == QDialog::Accepted) runLab1("time-set", dialog.values() << "--yes", "Lab1 Time Set", true);
    });
    menu->addAction("NTP Enable...", this, [this]() {
        runLab1("ntp-enable", {"--yes"}, "Lab1 NTP Enable", true);
    });
    moreButton->setMenu(menu);
    actionBar->addWidget(moreButton);
    actionBar->addWidget(sudoBadge());
    actionBar->addStretch();
    layout->addLayout(actionBar);

    auto* card = new QWidget(page);
    card->setObjectName("card");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 16, 16, 16);
    cardLayout->setSpacing(12);
    auto* heading = new QLabel("Thong tin thoi gian he thong", card);
    heading->setObjectName("sectionHeading");
    timeLabel = new QLabel("-", card);
    tzLabel = new QLabel("Timezone: -", card);
    ntpLabel = new QLabel("NTP: -", card);
    for (auto* label : {timeLabel, tzLabel, ntpLabel}) {
        label->setWordWrap(true);
    }
    cardLayout->addWidget(heading);
    cardLayout->addWidget(timeLabel);
    cardLayout->addWidget(tzLabel);
    cardLayout->addWidget(ntpLabel);
    cardLayout->addStretch();
    layout->addWidget(card, 1);
    return page;
}

QWidget* ShellAutomationPage::buildPackagesTab()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    auto* actionBar = new QHBoxLayout();
    actionBar->setSpacing(8);
    auto* listButton = actionButton("Package List");
    connect(listButton, &QPushButton::clicked, this, &ShellAutomationPage::loadPackageList);
    actionBar->addWidget(listButton);

    auto* moreButton = new QToolButton(page);
    moreButton->setObjectName("moreBtn");
    moreButton->setText(QString::fromUtf8("More ▾"));
    moreButton->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(moreButton);
    menu->setObjectName("moreMenu");
    menu->addAction("Update", this, [this]() { runLab1("pkg-update", {"--yes"}, "Lab1 Package Update", true); });
    menu->addAction("Install...", this, [this]() {
        SimpleFormDialog dialog("Package Install", {{"Packages", "curl git"}}, false, this);
        if (dialog.exec() == QDialog::Accepted) runLab1("pkg-install", dialog.values().first().split(' ', Qt::SkipEmptyParts) << "--yes", "Lab1 Package Install", true);
    });
    menu->addAction("Install from file...", this, [this]() {
        SimpleFormDialog dialog("Package Install File", {{"File", "/path/to/packages.txt"}}, false, this);
        if (dialog.exec() == QDialog::Accepted) runLab1("pkg-install-file", dialog.values() << "--yes", "Lab1 Package Install File", true);
    });
    menu->addSeparator();
    menu->addAction("Remove...", this, [this]() {
        SimpleFormDialog dialog("Package Remove", {{"Packages", "curl git"}}, true, this);
        if (dialog.exec() == QDialog::Accepted) runLab1("pkg-remove", dialog.values().first().split(' ', Qt::SkipEmptyParts) << "--yes", "Lab1 Package Remove", true);
    });
    moreButton->setMenu(menu);
    actionBar->addWidget(moreButton);
    actionBar->addWidget(sudoBadge());
    actionBar->addStretch();
    layout->addLayout(actionBar);

    packageStateLabel = new QLabel("Dang tai danh sach package...", page);
    packageStateLabel->setObjectName("card");
    packageStateLabel->setAlignment(Qt::AlignCenter);
    packageTable = new QTableWidget(0, 3, page);
    packageTable->setHorizontalHeaderLabels({"Package", "Version", "Status"});
    packageTable->horizontalHeader()->setStretchLastSection(true);
    packageTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    packageTable->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(packageStateLabel);
    layout->addWidget(packageTable, 1);
    return page;
}

QWidget* ShellAutomationPage::buildOutputPanel()
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

QPushButton* ShellAutomationPage::actionButton(const QString& text)
{
    auto* button = new QPushButton(text, this);
    button->setObjectName("actionBarBtn");
    return button;
}

QLabel* ShellAutomationPage::sudoBadge()
{
    auto* badge = new QLabel(rootMode ? "Root available" : QString::fromUtf8("Cần sudo"), this);
    badge->setObjectName(rootMode ? "badgeRoot" : "badgeLimited");
    return badge;
}

void ShellAutomationPage::onTabEntered(int tab)
{
    if (tab == 0 && !filesLoaded) {
        loadFilesList();
    } else if (tab == 1 && !cronLoaded) {
        loadCronList();
    } else if (tab == 2 && !timeLoaded) {
        loadTimeShow();
    } else if (tab == 3 && !packagesLoaded) {
        loadPackageList();
    }
}

void ShellAutomationPage::loadFilesList()
{
    if (filesListInFlight || !fileExplorer) return;
    filesListInFlight = true;
    runLab1("list", {fileExplorer->currentPath()}, "Lab1 List");
}

void ShellAutomationPage::loadCronList()
{
    if (cronListInFlight) return;
    cronListInFlight = true;
    if (cronStateLabel) cronStateLabel->setText("Dang tai danh sach cron...");
    runLab1("cron-list", {}, "Lab1 Cron List");
}

void ShellAutomationPage::loadTimeShow()
{
    if (timeShowInFlight) return;
    timeShowInFlight = true;
    runLab1("time-show", {}, "Lab1 Time Show");
}

void ShellAutomationPage::loadPackageList()
{
    if (packageListInFlight) return;
    packageListInFlight = true;
    output->clear();
    if (packageStateLabel) packageStateLabel->setText("Dang tai danh sach package...");
    commandRunner->run("dpkg-query",
                       {"-W", "-f=${binary:Package}\t${Version}\t${Status}\n"},
                       CommandType::OneShot,
                       TrustLevel::Reliable,
                       "Lab1 Package List");
}

void ShellAutomationPage::showTimezoneDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Timezone Set");
    auto* layout = new QVBoxLayout(&dialog);
    auto* label = new QLabel("Timezone", &dialog);
    auto* combo = new QComboBox(&dialog);
    combo->setEditable(true);
    combo->addItems(QStringList{
        "Asia/Ho_Chi_Minh",
        "Asia/Bangkok",
        "UTC",
        "Asia/Tokyo",
        "Asia/Seoul",
        "Asia/Singapore",
        "Europe/London",
        "Europe/Paris",
        "America/New_York",
        "America/Los_Angeles",
    });
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    if (auto* ok = buttons->button(QDialogButtonBox::Ok)) ok->setObjectName("primaryBtn");
    if (auto* cancel = buttons->button(QDialogButtonBox::Cancel)) cancel->setObjectName("secondaryBtn");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(label);
    layout->addWidget(combo);
    layout->addWidget(buttons);
    if (dialog.exec() == QDialog::Accepted) {
        runLab1("timezone-set", {combo->currentText().trimmed(), "--yes"}, "Lab1 Timezone Set", true);
    }
}

QString ShellAutomationPage::pathInCurrentDir(const QString& value) const
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

QString ShellAutomationPage::selectedPathOrCurrentDir() const
{
    if (!fileExplorer) {
        return {};
    }
    const QString selected = fileExplorer->selectedPath();
    return selected.isEmpty() ? fileExplorer->currentPath() : selected;
}

void ShellAutomationPage::runLab1(const QString& subcommand, const QStringList& args, const QString& action, bool unreliable)
{
    output->clear();
    const QString program = QDir(QString::fromUtf8(LAB_REPO_ROOT)).filePath("01_shell_system_admin/scripts/admin_tool.sh");
    commandRunner->run(program,
                       QStringList{"--cmd", subcommand} + args,
                       CommandType::OneShot,
                       unreliable ? TrustLevel::Unreliable : TrustLevel::Reliable,
                       action);
}

void ShellAutomationPage::applyCommandResult(const CommandResult& result)
{
    const QString combined = result.output + result.error;
    if (result.action == "Lab1 List") {
        filesListInFlight = false;
        filesLoaded = true;
    } else if (result.action == "Lab1 Cron List") {
        cronListInFlight = false;
        cronLoaded = true;
        if (cronTable) {
            cronTable->setRowCount(0);
            const QStringList lines = combined.split('\n', Qt::SkipEmptyParts);
            for (const auto& line : lines) {
                if (!line.contains("# LAB_ADMIN_TOOL:")) continue;
                const QString name = line.section("# LAB_ADMIN_TOOL:", 1).trimmed();
                const int row = cronTable->rowCount();
                cronTable->insertRow(row);
                cronTable->setItem(row, 0, new QTableWidgetItem(name));
                cronTable->setItem(row, 1, new QTableWidgetItem(line.section(' ', 0, 4)));
                cronTable->setItem(row, 2, new QTableWidgetItem(line.section(' ', 5)));
            }
            cronStateLabel->setText(cronTable->rowCount() ? "Cron jobs cua lab" : "Chua co job nao.");
        }
    } else if (result.action == "Lab1 Time Show") {
        timeShowInFlight = false;
        timeLoaded = true;
        const QStringList lines = combined.split('\n', Qt::SkipEmptyParts);
        if (timeLabel) {
            timeLabel->setText(lines.value(0, "-"));
            tzLabel->setText("Timezone: " + lines.filter("Time zone", Qt::CaseInsensitive).value(0, "-"));
            ntpLabel->setText("NTP: " + lines.filter("NTP", Qt::CaseInsensitive).value(0, "-"));
        }
    } else if (result.action.startsWith("Lab1 Cron") && result.action != "Lab1 Cron List") {
        cronLoaded = false;
    } else if (result.action == "Lab1 Package List") {
        packageListInFlight = false;
        packagesLoaded = true;
        if (packageTable) {
            packageTable->setRowCount(0);
            const QStringList lines = combined.split('\n', Qt::SkipEmptyParts);
            for (const auto& line : lines) {
                const QStringList cols = line.split('\t');
                if (cols.size() < 3 || !cols.at(2).contains("install ok installed")) {
                    continue;
                }
                const int row = packageTable->rowCount();
                packageTable->insertRow(row);
                packageTable->setItem(row, 0, new QTableWidgetItem(cols.at(0)));
                packageTable->setItem(row, 1, new QTableWidgetItem(cols.at(1)));
                packageTable->setItem(row, 2, new QTableWidgetItem(cols.at(2)));
            }
            packageStateLabel->setText(packageTable->rowCount()
                                           ? QString("Installed packages: %1").arg(packageTable->rowCount())
                                           : "Khong co du lieu package.");
        }
    }
    setOutput(combined, result.permissionError);
}

void ShellAutomationPage::setOutput(const QString& text, bool permissionError)
{
    outputPanel->setProperty("permissionError", permissionError);
    outputPanel->style()->unpolish(outputPanel);
    outputPanel->style()->polish(outputPanel);
    output->setPlainText(text);
}
