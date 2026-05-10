#include "MainWindow.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName("qt_finance");
    QApplication::setOrganizationName("learnai");

    // Qt 6.10.x regression: Windows native style hits a GDI Format_Mono assert
    // on any widget repaint. Fusion avoids the broken code path entirely.
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    MainWindow w;
    w.show();
    return app.exec();
}
