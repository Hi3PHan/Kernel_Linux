#pragma once

#include "../components/CommandRunner.h"

#include <QList>
#include <QWidget>

class FileExplorer;
class QButtonGroup;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTableWidget;
class QTcpServer;
class QTcpSocket;
class QUdpSocket;

class ProcessNetworkPage : public QWidget {
    Q_OBJECT

public:
    explicit ProcessNetworkPage(CommandRunner* runner, QWidget* parent = nullptr);

private:
    QWidget* buildTabBar();
    QWidget* buildProcessesTab();
    QWidget* buildFilesTab();
    QWidget* buildNetworkTab();
    QWidget* buildSocketsTab();
    QWidget* buildSocketServerPanel();
    QWidget* buildSocketClientPanel();
    QWidget* buildOutputPanel();
    QPushButton* actionButton(const QString& text);

    void onTabEntered(int tab);
    void loadProcessList();
    void loadNetworkInfo();
    void showNetworkLoading(const QString& action);
    void populateInterfaceTable(const QString& output);
    void populateRoutesTable(const QString& output);
    void runDnsInfo();
    void runDnsResolve();
    void runDnsSet();
    QString filePathInCurrentDir(const QString& value) const;
    QString selectedFilePathOrCurrentDir() const;
    bool serverIsRunning() const;
    bool clientIsActive() const;
    bool serverUsesTcp() const;
    bool clientUsesTcp() const;
    void appendSocketLog(QPlainTextEdit* target, const QString& text);
    void startOrStopChatServer();
    void startChatServer();
    void stopChatServer();
    void acceptChatClient();
    void readServerTcpData(QTcpSocket* socket);
    void readServerUdpData();
    void sendServerMessage();
    void startOrStopChatClient();
    void startChatClient();
    void stopChatClient();
    void readClientTcpData();
    void readClientUdpData();
    void sendClientMessage();
    void updateSocketUi();
    void runLab2(const QString& subcommand, const QStringList& args, const QString& action, CommandType type = CommandType::OneShot, const QString& readySignal = QString());
    void applyCommandResult(const CommandResult& result);
    void setOutput(const QString& text, bool permissionError);

    CommandRunner* commandRunner = nullptr;
    QButtonGroup* tabGroup = nullptr;
    QStackedWidget* tabPages = nullptr;
    QWidget* outputPanel = nullptr;
    QPlainTextEdit* output = nullptr;
    QTableWidget* procTable = nullptr;
    QLabel* procStateLabel = nullptr;
    QStackedWidget* networkStack = nullptr;
    QTableWidget* interfaceTable = nullptr;
    QTableWidget* routeTable = nullptr;
    QPlainTextEdit* networkTextView = nullptr;
    FileExplorer* fileExplorer = nullptr;
    QString currentReadFilePath;
    QComboBox* serverProtocolCombo = nullptr;
    QComboBox* clientProtocolCombo = nullptr;
    QSpinBox* serverPortSpin = nullptr;
    QSpinBox* clientPortSpin = nullptr;
    QLineEdit* clientHostEdit = nullptr;
    QLineEdit* serverMessageEdit = nullptr;
    QLineEdit* clientMessageEdit = nullptr;
    QLabel* serverStatusLabel = nullptr;
    QLabel* clientStatusLabel = nullptr;
    QPlainTextEdit* serverChatLog = nullptr;
    QPlainTextEdit* clientChatLog = nullptr;
    QPushButton* serverToggleButton = nullptr;
    QPushButton* clientToggleButton = nullptr;
    QPushButton* serverSendButton = nullptr;
    QPushButton* clientSendButton = nullptr;
    QTcpServer* chatTcpServer = nullptr;
    QList<QTcpSocket*> chatServerClients;
    QUdpSocket* chatUdpServer = nullptr;
    QTcpSocket* chatTcpClient = nullptr;
    QUdpSocket* chatUdpClient = nullptr;
    QString serverUdpPeerAddress;
    quint16 serverUdpPeerPort = 0;
    bool udpClientReady = false;
    bool processListInFlight = false;
    bool networkInFlight = false;
    bool processesLoaded = false;
    bool networkLoaded = false;
};
