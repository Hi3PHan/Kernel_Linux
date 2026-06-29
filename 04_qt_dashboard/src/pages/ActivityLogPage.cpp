#include "ActivityLogPage.h"

#include "../components/ActivityLogger.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTableView>
#include <QVBoxLayout>

ActivityLogPage::ActivityLogPage(ActivityLogger* logger, QWidget* parent)
    : QWidget(parent)
    , logger(logger)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto* topArea = new QWidget(this);
    auto* topLayout = new QVBoxLayout(topArea);
    topLayout->setContentsMargins(0, 0, 0, 0);
    auto* actionBar = new QHBoxLayout();
    auto* clear = new QPushButton("Clear Log", topArea);
    clear->setObjectName("secondaryBtn");
    connect(clear, &QPushButton::clicked, logger, &ActivityLogger::clear);
    actionBar->addWidget(clear);
    actionBar->addStretch();
    topLayout->addLayout(actionBar);

    auto* table = new QTableView(topArea);
    table->setModel(logger);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->horizontalHeader()->setStretchLastSection(true);
    table->setColumnWidth(0, 160);
    table->setColumnWidth(1, 80);
    table->setColumnWidth(2, 200);
    table->setColumnWidth(4, 60);
    topLayout->addWidget(table, 1);

    auto* outputPanel = new QWidget(this);
    outputPanel->setObjectName("outputPanel");
    outputPanel->setMinimumHeight(48);
    outputPanel->setMaximumHeight(48);
    auto* outputLayout = new QVBoxLayout(outputPanel);
    outputLayout->setContentsMargins(0, 0, 0, 0);
    outputLayout->setSpacing(0);
    auto* headerWidget = new QWidget(outputPanel);
    headerWidget->setFixedHeight(48);
    auto* header = new QHBoxLayout(headerWidget);
    header->setContentsMargins(12, 4, 12, 4);
    auto* label = new QLabel("Output", headerWidget);
    auto* toggle = new QPushButton("Show", headerWidget);
    toggle->setObjectName("secondaryBtn");
    auto* clearOutput = new QPushButton("Clear", headerWidget);
    clearOutput->setObjectName("secondaryBtn");
    header->addWidget(label);
    header->addStretch();
    header->addWidget(toggle);
    header->addWidget(clearOutput);
    outputLayout->addWidget(headerWidget);
    output = new QPlainTextEdit(outputPanel);
    output->setReadOnly(true);
    output->setLineWrapMode(QPlainTextEdit::NoWrap);
    output->setVisible(false);
    outputLayout->addWidget(output, 1);
    connect(clearOutput, &QPushButton::clicked, output, &QPlainTextEdit::clear);
    connect(toggle, &QPushButton::clicked, this, [this, outputPanel, toggle]() {
        const bool willHide = output->isVisible();
        output->setVisible(!willHide);
        toggle->setText(willHide ? "Show" : "Hide");
        outputPanel->setMaximumHeight(willHide ? 48 : QWIDGETSIZE_MAX);
        outputPanel->setMinimumHeight(willHide ? 48 : 80);
    });

    auto* splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(topArea);
    splitter->addWidget(outputPanel);
    splitter->setStretchFactor(0, 5);
    splitter->setStretchFactor(1, 1);
    splitter->setChildrenCollapsible(false);
    layout->addWidget(splitter, 1);

    connect(table, &QTableView::clicked, this, [this](const QModelIndex& index) {
        output->setPlainText(this->logger->entryAt(index.row()).output);
    });
}
