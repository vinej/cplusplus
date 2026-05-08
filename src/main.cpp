#include "MainWindow.h"

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("qt_finance");
    QApplication::setOrganizationName("learnai");

    MainWindow w;
    w.show();
    return app.exec();
}
