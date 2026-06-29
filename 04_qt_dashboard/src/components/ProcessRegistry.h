#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QProcess>
#include <QString>

class ProcessRegistry : public QObject {
    Q_OBJECT

public:
    explicit ProcessRegistry(QObject* parent = nullptr);

    void add(const QString& key, QProcess* process);
    void markReady(const QString& key);
    void stop(const QString& key);
    void stopAll();
    void remove(const QString& key);
    bool isRunning(const QString& key) const;

signals:
    void processReady(const QString& key);
    void processStopped(const QString& key);

private:
    QHash<QString, QPointer<QProcess>> processes;
};
