#include "FileExplorer.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QItemSelectionModel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QStackedWidget>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>

FileExplorer::FileExplorer(const QString& startPath, QWidget* parent)
    : QWidget(parent)
    , rootBasePath(QDir(startPath).absolutePath())
{
    fsModel = new QFileSystemModel(this);
    fsModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot);
    fsModel->setRootPath(rootBasePath);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    auto* addressBar = new QHBoxLayout();
    addressBar->setSpacing(4);

    backButton = new QPushButton("<", this);
    forwardButton = new QPushButton(">", this);
    upButton = new QPushButton("^", this);
    for (auto* button : {backButton, forwardButton, upButton}) {
        button->setObjectName("iconBtn");
        addressBar->addWidget(button);
    }

    pathEdit = new QLineEdit(this);
    addressBar->addWidget(pathEdit, 1);

    auto* refreshButton = new QPushButton("Refresh", this);
    refreshButton->setObjectName("actionBarBtn");
    addressBar->addWidget(refreshButton);
    layout->addLayout(addressBar);

    fileTree = new QTreeView(this);
    fileTree->setModel(fsModel);
    fileTree->setSelectionMode(QAbstractItemView::SingleSelection);
    fileTree->hideColumn(2);

    detailStack = new QStackedWidget(this);
    auto* empty = new QLabel("Chọn file hoặc thư mục", detailStack);
    empty->setAlignment(Qt::AlignCenter);
    detailStack->addWidget(empty);

    auto* detailPage = new QWidget(detailStack);
    detailPage->setObjectName("card");
    auto* detailLayout = new QVBoxLayout(detailPage);
    detailLayout->setContentsMargins(16, 16, 16, 16);
    detailTitle = new QLabel(detailPage);
    detailTitle->setObjectName("sectionHeading");
    detailPath = new QLabel(detailPage);
    detailSize = new QLabel(detailPage);
    detailModified = new QLabel(detailPage);
    for (auto* label : {detailPath, detailSize, detailModified}) {
        label->setWordWrap(true);
    }
    detailLayout->addWidget(detailTitle);
    detailLayout->addWidget(detailPath);
    detailLayout->addWidget(detailSize);
    detailLayout->addWidget(detailModified);
    detailLayout->addStretch();
    detailStack->addWidget(detailPage);

    auto* previewPage = new QWidget(detailStack);
    previewPage->setObjectName("card");
    auto* previewLayout = new QVBoxLayout(previewPage);
    previewLayout->setContentsMargins(16, 16, 16, 16);
    previewTitle = new QLabel("Read file", previewPage);
    previewTitle->setObjectName("sectionHeading");
    previewPath = new QLabel(previewPage);
    previewPath->setWordWrap(true);
    detailPreview = new QPlainTextEdit(previewPage);
    detailPreview->setReadOnly(true);
    detailPreview->setLineWrapMode(QPlainTextEdit::NoWrap);
    previewLayout->addWidget(previewTitle);
    previewLayout->addWidget(previewPath);
    previewLayout->addWidget(detailPreview, 1);
    detailStack->addWidget(previewPage);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(fileTree);
    splitter->addWidget(detailStack);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);
    splitter->setChildrenCollapsible(false);
    layout->addWidget(splitter, 1);

    connect(pathEdit, &QLineEdit::returnPressed, this, &FileExplorer::applyPathEdit);
    connect(pathEdit, &QLineEdit::editingFinished, this, &FileExplorer::applyPathEdit);
    connect(refreshButton, &QPushButton::clicked, this, &FileExplorer::refreshList);
    connect(fileTree, &QTreeView::clicked, this, &FileExplorer::showDetails);
    connect(fileTree, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        const QFileInfo info = fsModel->fileInfo(index);
        if (info.isDir()) {
            navigateTo(info.absoluteFilePath());
        }
    });
    connect(backButton, &QPushButton::clicked, this, [this]() {
        if (historyBack.isEmpty()) return;
        historyForward.append(currentDir);
        const QString path = historyBack.takeLast();
        navigateTo(path, false);
    });
    connect(forwardButton, &QPushButton::clicked, this, [this]() {
        if (historyForward.isEmpty()) return;
        historyBack.append(currentDir);
        const QString path = historyForward.takeLast();
        navigateTo(path, false);
    });
    connect(upButton, &QPushButton::clicked, this, [this]() {
        QDir dir(currentDir);
        if (dir.cdUp()) {
            navigateTo(dir.absolutePath());
        }
    });

    navigateTo(rootBasePath, false);
}

QString FileExplorer::currentPath() const
{
    return currentDir;
}

QString FileExplorer::selectedPath() const
{
    const QModelIndex index = fileTree->currentIndex();
    if (!index.isValid()) {
        return {};
    }
    return fsModel->fileInfo(index).absoluteFilePath();
}

void FileExplorer::refreshList()
{
    const QModelIndex index = fileTree->currentIndex();
    if (index.isValid()) {
        showDetails(index);
    } else {
        resetDetails();
    }
    emit listRequested(currentDir);
}

void FileExplorer::setShowHidden(bool showHidden)
{
    showHiddenFiles = showHidden;
    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot;
    if (showHiddenFiles) {
        filters |= QDir::Hidden;
    }
    fsModel->setFilter(filters);
}

void FileExplorer::showTextPreview(const QString& title, const QString& path, const QString& text)
{
    previewTitle->setText(title);
    previewPath->setText(QString("Duong dan: %1").arg(path));
    detailPreview->setPlainText(text);
    detailStack->setCurrentIndex(2);
}

QString FileExplorer::normalizePath(const QString& path) const
{
    QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return currentDir;
    }
    QFileInfo info(trimmed);
    if (info.isRelative()) {
        info.setFile(QDir(rootBasePath), trimmed);
    }
    return info.absoluteFilePath();
}

void FileExplorer::navigateTo(const QString& path, bool pushHistory)
{
    const QString normalized = normalizePath(path);
    if (!QFileInfo(normalized).isDir()) {
        flashInvalidPath();
        return;
    }
    if (pushHistory && !currentDir.isEmpty() && currentDir != normalized) {
        historyBack.append(currentDir);
        historyForward.clear();
    }
    currentDir = QDir(normalized).absolutePath();
    pathEdit->setText(currentDir);
    fileTree->setRootIndex(fsModel->index(currentDir));
    resetDetails();
    updateNavButtons();
}

void FileExplorer::applyPathEdit()
{
    const QString normalized = normalizePath(pathEdit->text());
    if (QFileInfo(normalized).isDir()) {
        navigateTo(normalized);
    } else {
        flashInvalidPath();
        pathEdit->setText(currentDir);
    }
}

void FileExplorer::updateNavButtons()
{
    backButton->setEnabled(!historyBack.isEmpty());
    forwardButton->setEnabled(!historyForward.isEmpty());
    upButton->setEnabled(QDir(currentDir).cdUp());
}

void FileExplorer::showDetails(const QModelIndex& index)
{
    const QFileInfo info = fsModel->fileInfo(index);
    detailTitle->setText(QString("%1 %2").arg(info.isDir() ? "[DIR]" : "[FILE]", info.fileName()));
    detailPath->setText(QString("Duong dan: %1").arg(info.absoluteFilePath()));
    detailSize->setText(QString("Kich thuoc: %1 bytes").arg(info.size()));
    detailModified->setText(QString("Sua doi: %1").arg(info.lastModified().toString("yyyy-MM-dd HH:mm:ss")));
    detailStack->setCurrentIndex(1);
}

void FileExplorer::resetDetails()
{
    detailStack->setCurrentIndex(0);
}

void FileExplorer::flashInvalidPath()
{
    pathEdit->setStyleSheet("border:1px solid #C7162D;");
    QTimer::singleShot(600, pathEdit, [this]() { pathEdit->setStyleSheet(""); });
}
