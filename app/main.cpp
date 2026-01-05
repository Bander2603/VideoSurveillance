#include <QApplication>
#include "ui/MainWindow.h"
#include "core/DotEnv.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    DotEnv::loadFile(".env");

    MainWindow w;
    w.show();

    return app.exec();
}