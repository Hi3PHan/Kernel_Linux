#include "MainWindow.h"

#include "components/ActivityLogger.h"
#include "components/CommandRunner.h"
#include "components/HeaderBar.h"
#include "components/ProcessRegistry.h"
#include "components/Sidebar.h"
#include "pages/ActivityLogPage.h"
#include "pages/PacketCapturePage.h"
#include "pages/ProcessNetworkPage.h"
#include "pages/ShellAutomationPage.h"
#include "pages/SystemMonitorPage.h"

#include <QCloseEvent>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QVBoxLayout>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , rootMode(detectRootMode())
    , activityLogger(new ActivityLogger(this))
    , processRegistry(new ProcessRegistry(this))
    , commandRunner(new CommandRunner(activityLogger, processRegistry, this))
{
    setWindowTitle("Ubuntu System Manager");
    setMenuBar(nullptr);

    auto* root = new QWidget(this);
    auto* rootLayout = new QVBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    header = new HeaderBar(rootMode, root);
    rootLayout->addWidget(header);

    auto* sidebar = new Sidebar(root);
    pages = new QStackedWidget(root);
    pages->addWidget(new ShellAutomationPage(commandRunner, rootMode, pages));
    pages->addWidget(new ProcessNetworkPage(commandRunner, pages));
    pages->addWidget(new PacketCapturePage(commandRunner, rootMode, pages));
    pages->addWidget(new SystemMonitorPage(pages));
    pages->addWidget(new ActivityLogPage(activityLogger, pages));

    connect(sidebar, &Sidebar::pageSelected, pages, &QStackedWidget::setCurrentIndex);

    mainSplitter = new QSplitter(Qt::Horizontal, root);
    mainSplitter->addWidget(sidebar);
    mainSplitter->addWidget(pages);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 4);
    mainSplitter->setChildrenCollapsible(false);

    rootLayout->addWidget(mainSplitter, 1);
    setCentralWidget(root);
    restoreSplitterState();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveSplitterState();
    processRegistry->stopAll();
    QMainWindow::closeEvent(event);
}

bool MainWindow::detectRootMode() const
{
#ifdef Q_OS_UNIX
    return geteuid() == 0;
#else
    return false;
#endif
}

void MainWindow::restoreSplitterState()
{
    QSettings settings("LabCuoiKy", "QtDashboard");
    const auto key = QStringLiteral("mainSplitter/state");
    if (settings.contains(key)) {
        mainSplitter->restoreState(settings.value(key).toByteArray());
    }
}

void MainWindow::saveSplitterState() const
{
    QSettings settings("LabCuoiKy", "QtDashboard");
    settings.setValue("mainSplitter/state", mainSplitter->saveState());
}
