#include "SimpleFormDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

SimpleFormDialog::SimpleFormDialog(const QString& title,
                                   const QList<QPair<QString, QString>>& fields,
                                   bool isDangerous,
                                   QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    if (isDangerous) {
        auto* warning = new QLabel("Thao tác này có side effect hoặc xóa/thay đổi trạng thái hệ thống.", this);
        warning->setObjectName("badgeLimited");
        warning->setWordWrap(true);
        layout->addWidget(warning);
    }

    for (const auto& field : fields) {
        auto* label = new QLabel(field.first, this);
        auto* edit = new QLineEdit(this);
        edit->setPlaceholderText(field.second);
        edit->setText(field.second);
        edits.append(edit);
        layout->addWidget(label);
        layout->addWidget(edit);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    if (auto* ok = buttons->button(QDialogButtonBox::Ok)) {
        ok->setObjectName(isDangerous ? "dangerBtn" : "primaryBtn");
    }
    if (auto* cancel = buttons->button(QDialogButtonBox::Cancel)) {
        cancel->setObjectName("secondaryBtn");
    }

    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

QStringList SimpleFormDialog::values() const
{
    QStringList result;
    for (auto* edit : edits) {
        result << edit->text();
    }
    return result;
}
