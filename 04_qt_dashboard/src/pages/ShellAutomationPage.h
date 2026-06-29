#pragma once

#include "../components/CommandRunner.h"

#include <QWidget>

class FileExplorer;
class QButtonGroup;
class QLabel;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;

class ShellAutomationPage : public QWidget {
    Q_OBJECT

public:
    ShellAutomationPage(CommandRunner* runner, bool rootMode, QWidget* parent = nullptr);

private:
    QWidget* buildTabBar();
    QWidget* buildFilesTab();
    QWidget* buildCronTab();
    QWidget* buildTimeTab();
    QWidget* buildPackagesTab();
    QWidget* buildOutputPanel();
    QPushButton* actionButton(const QString& text);
    QLabel* sudoBadge();

    void onTabEntered(int tab);
    void loadFilesList();
    void loadCronList();
    void loadTimeShow();
    void loadPackageList();
    void showTimezoneDialog();
    QString pathInCurrentDir(const QString& value) const;
    QString selectedPathOrCurrentDir() const;
    void runLab1(const QString& subcommand, const QStringList& args, const QString& action, bool unreliable = false);
    void applyCommandResult(const CommandResult& result);
    void setOutput(const QString& text, bool permissionError = false);

    CommandRunner* commandRunner = nullptr;
    bool rootMode = false;
    QButtonGroup* tabGroup = nullptr;
    QStackedWidget* tabPages = nullptr;
    QWidget* outputPanel = nullptr;
    QPlainTextEdit* output = nullptr;
    FileExplorer* fileExplorer = nullptr;
    QTableWidget* cronTable = nullptr;
    QLabel* cronStateLabel = nullptr;
    QLabel* timeLabel = nullptr;
    QLabel* tzLabel = nullptr;
    QLabel* ntpLabel = nullptr;
    QTableWidget* packageTable = nullptr;
    QLabel* packageStateLabel = nullptr;
    bool filesListInFlight = false;
    bool cronListInFlight = false;
    bool timeShowInFlight = false;
    bool packageListInFlight = false;
    bool filesLoaded = false;
    bool cronLoaded = false;
    bool timeLoaded = false;
    bool packagesLoaded = false;
};
