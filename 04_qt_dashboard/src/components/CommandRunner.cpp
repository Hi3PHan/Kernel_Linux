#include "CommandRunner.h"

#include "ProcessRegistry.h"

#include <QDateTime>

namespace {
constexpr int MaxStoredLongRunningOutputChars = 64 * 1024;

void appendProcessText(QString* buffer, const QString& text, CommandType type)
{
    buffer->append(text);
    if (type != CommandType::LongRunning) {
        return;
    }

    const auto extraChars = buffer->size() - MaxStoredLongRunningOutputChars;
    if (extraChars > 0) {
        buffer->remove(0, extraChars);
    }
}
}

CommandRunner::CommandRunner(ActivityLogger* logger, ProcessRegistry* registry, QObject* parent)
    : QObject(parent)
    , logger(logger)
    , registry(registry)
{
}

void CommandRunner::run(const QString& program,
                        const QStringList& arguments,
                        CommandType type,
                        TrustLevel trustLevel,
                        const QString& action,
                        const QString& workingDirectory,
                        const QString& readySignal)
{
    auto* process = new QProcess(this);
    if (!workingDirectory.isEmpty()) {
        process->setWorkingDirectory(workingDirectory);
    }

    auto* output = new QString();
    auto* error = new QString();

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, output, type, action, readySignal]() {
        const QString text = QString::fromLocal8Bit(process->readAllStandardOutput());
        appendProcessText(output, text, type);
        emit commandOutput(action, text);
        if (!readySignal.isEmpty() && text.contains(readySignal)) {
            registry->markReady(action);
        }
    });

    connect(process, &QProcess::readyReadStandardError, this, [this, process, error, type, action]() {
        const QString text = QString::fromLocal8Bit(process->readAllStandardError());
        appendProcessText(error, text, type);
        emit commandOutput(action, text);
    });

    connect(process, &QProcess::finished, this, [=](int exitCode, QProcess::ExitStatus exitStatus) {
        const bool crashed = exitStatus == QProcess::CrashExit;
        const QString combined = *output + *error;
        const bool permission = hasPermissionError(combined);

        ActivityEntry entry;
        entry.time = QDateTime::currentDateTime();
        entry.level = permission ? "Permission" : (crashed || exitCode != 0 ? "Error" : "Info");
        entry.action = action;
        entry.detail = commandDetail(program, arguments);
        entry.output = combined;
        entry.exitCode = crashed ? -1 : exitCode;
        entry.trustLevel = trustLevel;
        logger->addEntry(entry);

        emit commandFinished({action, *output, *error, entry.exitCode, trustLevel, permission});

        registry->remove(action);
        delete output;
        delete error;
        process->deleteLater();
    });

    connect(process, &QProcess::errorOccurred, this, [=](QProcess::ProcessError) {
        error->append(process->errorString());
    });

    process->start(program, arguments);
    if (type == CommandType::LongRunning) {
        registry->add(action, process);
    }
}

void CommandRunner::stop(const QString& action)
{
    registry->stop(action);
}

bool CommandRunner::isRunning(const QString& action) const
{
    return registry->isRunning(action);
}

bool CommandRunner::hasPermissionError(const QString& text) const
{
    return text.contains("Operation not permitted", Qt::CaseInsensitive)
        || text.contains("Permission denied", Qt::CaseInsensitive);
}

QString CommandRunner::commandDetail(const QString& program, const QStringList& arguments) const
{
    QStringList parts;
    parts << program;
    for (const auto& argument : arguments) {
        parts << argument;
    }
    return parts.join(' ');
}
