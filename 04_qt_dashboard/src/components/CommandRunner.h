#pragma once

#include "ActivityLogger.h"

#include <QObject>
#include <QProcess>

class ProcessRegistry;

enum class CommandType {
    OneShot,
    LongRunning
};

struct CommandResult {
    QString action;
    QString output;
    QString error;
    int exitCode = 0;
    TrustLevel trustLevel = TrustLevel::Reliable;
    bool permissionError = false;
};

class CommandRunner : public QObject {
    Q_OBJECT

public:
    CommandRunner(ActivityLogger* logger, ProcessRegistry* registry, QObject* parent = nullptr);

    void run(const QString& program,
             const QStringList& arguments,
             CommandType type,
             TrustLevel trustLevel,
             const QString& action,
             const QString& workingDirectory = QString(),
             const QString& readySignal = QString());
    void stop(const QString& action);
    bool isRunning(const QString& action) const;

signals:
    void commandFinished(const CommandResult& result);
    void commandOutput(const QString& action, const QString& text);

private:
    bool hasPermissionError(const QString& text) const;
    QString commandDetail(const QString& program, const QStringList& arguments) const;

    ActivityLogger* logger = nullptr;
    ProcessRegistry* registry = nullptr;
};
