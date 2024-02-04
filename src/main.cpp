#include "mainwindow.h"
#include <QApplication>
#include "UI/Widgets/VKWidget.h"
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    //VRcz::VKWidget w;
    //w.resize(800, 600);
    //w.show();
    return a.exec();
}
