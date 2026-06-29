#include "MainWindow.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QFontInfo>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setStyle("Fusion");

    QFont font("Ubuntu", 10);
    if (!QFontInfo(font).exactMatch()) {
        font.setFamily("Noto Sans");
    }
    app.setFont(font);

    QFile qss("resources/style.qss");
    if (qss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
    }

    MainWindow window;
    window.resize(1280, 800);
    window.show();

    return app.exec();
}
