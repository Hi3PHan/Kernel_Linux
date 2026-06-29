#include "ProcessRegistry.h"

ProcessRegistry::ProcessRegistry(QObject* parent)
    : QObject(parent)
{
}

void ProcessRegistry::add(const QString& key, QProcess* process)
{
    processes.insert(key, process);
}

void ProcessRegistry::markReady(const QString& key)
{
    emit processReady(key);
}

void ProcessRegistry::stop(const QString& key)
{
    auto process = processes.value(key);
    if (!process) {
        return;
    }
    process->terminate();
    if (!process->waitForFinished(1500)) {
        process->kill();
    }
    emit processStopped(key);
}

void ProcessRegistry::stopAll()
{
    const auto keys = processes.keys();
    for (const auto& key : keys) {
        stop(key);
    }
}

void ProcessRegistry::remove(const QString& key)
{
    processes.remove(key);
}

bool ProcessRegistry::isRunning(const QString& key) const
{
    return processes.contains(key) && processes.value(key);
}
