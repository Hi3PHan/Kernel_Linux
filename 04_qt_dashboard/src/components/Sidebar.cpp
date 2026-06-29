#include "Sidebar.h"

#include <QButtonGroup>
#include <QFrame>
#include <QPushButton>
#include <QVBoxLayout>

Sidebar::Sidebar(QWidget* parent)
    : QWidget(parent)
    , buttonGroup(new QButtonGroup(this))
{
    setObjectName("sidebar");
    setMinimumWidth(180);
    setMaximumWidth(320);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(0);

    const QList<QPair<int, QString>> mainPages = {
        {0, "Shell & Automation"},
        {1, "Process & Network"},
        {2, "Packet Capture"},
        {3, "System Monitor"},
    };

    for (const auto& page : mainPages) {
        auto* button = new QPushButton(page.second, this);
        button->setCheckable(true);
        buttonGroup->addButton(button, page.first);
        layout->addWidget(button);
    }

    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    layout->addSpacing(8);
    layout->addWidget(separator);
    layout->addSpacing(8);

    const QList<QPair<int, QString>> secondaryPages = {
        {4, "Activity Log"},
    };

    for (const auto& page : secondaryPages) {
        auto* button = new QPushButton(page.second, this);
        button->setCheckable(true);
        buttonGroup->addButton(button, page.first);
        layout->addWidget(button);
    }

    layout->addStretch();
    buttonGroup->button(0)->setChecked(true);

    connect(buttonGroup, &QButtonGroup::idClicked, this, &Sidebar::pageSelected);
}
