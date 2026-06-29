#pragma once

#include <QWidget>

class ActivityLogger;
class QPlainTextEdit;

class ActivityLogPage : public QWidget {
    Q_OBJECT

public:
    explicit ActivityLogPage(ActivityLogger* logger, QWidget* parent = nullptr);

private:
    ActivityLogger* logger = nullptr;
    QPlainTextEdit* output = nullptr;
};
