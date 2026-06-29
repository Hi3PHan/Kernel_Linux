#pragma once

#include <QWidget>

class QFileSystemModel;
class QLabel;
class QLineEdit;
class QModelIndex;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTreeView;

class FileExplorer : public QWidget {
    Q_OBJECT

public:
    explicit FileExplorer(const QString& startPath, QWidget* parent = nullptr);

    QString currentPath() const;
    QString selectedPath() const;
    void refreshList();
    void setShowHidden(bool showHidden);
    void showTextPreview(const QString& title, const QString& path, const QString& text);

signals:
    void listRequested(const QString& path);

private:
    QString normalizePath(const QString& path) const;
    void navigateTo(const QString& path, bool pushHistory = true);
    void applyPathEdit();
    void updateNavButtons();
    void showDetails(const QModelIndex& index);
    void resetDetails();
    void flashInvalidPath();

    QFileSystemModel* fsModel = nullptr;
    QTreeView* fileTree = nullptr;
    QLineEdit* pathEdit = nullptr;
    QPushButton* backButton = nullptr;
    QPushButton* forwardButton = nullptr;
    QPushButton* upButton = nullptr;
    QStackedWidget* detailStack = nullptr;
    QLabel* detailTitle = nullptr;
    QLabel* detailPath = nullptr;
    QLabel* detailSize = nullptr;
    QLabel* detailModified = nullptr;
    QLabel* previewTitle = nullptr;
    QLabel* previewPath = nullptr;
    QPlainTextEdit* detailPreview = nullptr;
    QString rootBasePath;
    QString currentDir;
    QStringList historyBack;
    QStringList historyForward;
    bool showHiddenFiles = false;
};
