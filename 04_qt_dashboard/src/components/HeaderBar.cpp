#include "HeaderBar.h"

#include <QHBoxLayout>
#include <QLabel>

HeaderBar::HeaderBar(bool rootMode, QWidget* parent)
    : QWidget(parent)
{
    setObjectName("header");
    setFixedHeight(56);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(0);

    auto* titleLabel = new QLabel("Ubuntu System Manager", this);
    titleLabel->setObjectName("headerTitle");

    auto* badgeLabel = new QLabel(rootMode ? QStringLiteral("● Root mode")
                                           : QStringLiteral("⚠ Limited mode"), this);
    badgeLabel->setObjectName(rootMode ? "badgeRoot" : "badgeLimited");

    layout->addWidget(titleLabel);
    layout->addStretch();
    layout->addWidget(badgeLabel);
}
