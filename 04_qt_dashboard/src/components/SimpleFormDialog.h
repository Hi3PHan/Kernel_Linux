#pragma once

#include <QDialog>

class QLineEdit;

class SimpleFormDialog : public QDialog {
    Q_OBJECT

public:
    SimpleFormDialog(const QString& title,
                     const QList<QPair<QString, QString>>& fields,
                     bool isDangerous = false,
                     QWidget* parent = nullptr);

    QStringList values() const;

private:
    QList<QLineEdit*> edits;
};
