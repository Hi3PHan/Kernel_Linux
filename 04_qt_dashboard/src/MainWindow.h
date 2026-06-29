#pragma once

#include <QMainWindow>

class ActivityLogger;
class CommandRunner;
class HeaderBar;
class ProcessRegistry;
class QCloseEvent;
class QSplitter;
class QStackedWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    bool detectRootMode() const;
    void restoreSplitterState();
    void saveSplitterState() const;

    bool rootMode = false;
    HeaderBar* header = nullptr;
    QSplitter* mainSplitter = nullptr;
    QStackedWidget* pages = nullptr;
    ActivityLogger* activityLogger = nullptr;
    ProcessRegistry* processRegistry = nullptr;
    CommandRunner* commandRunner = nullptr;
};
