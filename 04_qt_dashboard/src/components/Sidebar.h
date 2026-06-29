#pragma once

#include <QWidget>

class QButtonGroup;

class Sidebar : public QWidget {
    Q_OBJECT

public:
    explicit Sidebar(QWidget* parent = nullptr);

signals:
    void pageSelected(int index);

private:
    QButtonGroup* buttonGroup = nullptr;
};
