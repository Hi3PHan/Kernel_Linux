#pragma once

#include "../components/CommandRunner.h"

#include <QString>
#include <QStringList>
#include <QWidget>

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QShowEvent;
class QTableWidget;
class QTimer;

class PacketCapturePage : public QWidget {
    Q_OBJECT

public:
    PacketCapturePage(CommandRunner* runner, bool rootMode, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    QWidget* buildLimitedModePage();
    QWidget* buildRootModePage();
    QWidget* buildFilterPanel();
    QWidget* buildDetailPanel(QWidget* parent);
    QPushButton* actionButton(const QString& text);

    QString captureDirectory() const;
    QString captureToolPath() const;
    QString modulePath() const;
    QString protocolValue() const;
    QString directionValue() const;
    QString sourceIpValue() const;
    QString destIpValue() const;
    QString formatPacketTime(quint64 timestampNs);
    bool isValidIpv4OrAny(const QString& value) const;
    bool moduleLoadSucceeded(const CommandResult& result) const;
    bool isIoctlMismatch(const QString& text) const;

    void ensureCaptureStarted();
    void reloadCaptureModule();
    void applyFilterAndMaybeStart(bool shouldStart);
    void restartCaptureWithFilter();
    void submitFilterChange();
    void startStream();
    void pauseStream();
    void clearCapture();
    void runCaptureCtl(const QStringList& args, const QString& action, CommandType type = CommandType::OneShot);

    void handleCommandResult(const CommandResult& result);
    void handleStreamOutput(const QString& text);
    void loadSelectedPacketDetail();
    void setPacketDetail(const QString& text);
    void populatePacketDetailTable(const QString& text);
    void queuePacketLine(const QString& line);
    void flushPendingPackets();
    bool appendPacketLine(const QString& line);
    void trimPacketRows();
    void clearTable();
    void parseStatsOutput(const QString& text);
    void updateStreamingUi();
    void updateStatusLabel();
    void setOutput(const QString& text, bool permissionError = false);

    CommandRunner* commandRunner = nullptr;
    bool rootMode = false;
    bool streaming = false;
    bool streamStopRequested = false;
    bool restartAfterClear = false;
    bool autoStartRequested = false;
    bool startAfterApply = false;
    bool reloadingAfterIoctlMismatch = false;
    quint64 lastSeq = 0;
    quint64 firstTimestampNs = 0;
    quint64 overwrittenRecords = 0;
    QString streamBuffer;
    QStringList pendingPacketLines;

    QComboBox* protocolCombo = nullptr;
    QComboBox* directionCombo = nullptr;
    QLineEdit* sourceIpEdit = nullptr;
    QLineEdit* destIpEdit = nullptr;
    QLabel* statusLabel = nullptr;
    QTableWidget* packetTable = nullptr;
    QTableWidget* packetDetailTable = nullptr;
    QPlainTextEdit* packetHex = nullptr;
    QPushButton* applyButton = nullptr;
    QPushButton* clearButton = nullptr;
    QTimer* packetFlushTimer = nullptr;
    QString pendingDetailSeq;
};
