#pragma once

#include <QWidget>

class HeaderBar : public QWidget {
    Q_OBJECT

public:
    explicit HeaderBar(bool rootMode, QWidget* parent = nullptr);
};
