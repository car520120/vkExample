#include "mainwindow.h"
#include "UI/Widgets/VKWidget.h"

#include <QTimer>
void MainWindow::closeEvent(QCloseEvent* event)
{
    timer->stop();
}
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent)
{
    auto vk = new VRcz::VKWidget(this);
    setCentralWidget(vk);
    resize(800, 600);
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, [vk]() {
        vk->onUpdateRender();
    });
    timer->start(1);
}

MainWindow::~MainWindow()
{
}
