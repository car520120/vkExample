#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
QT_BEGIN_NAMESPACE
class QTimer;
QT_END_NAMESPACE
class MainWindow : public QMainWindow
{
    Q_OBJECT
private:
    QTimer* timer;
public:
    void closeEvent(QCloseEvent* event) override;
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
};

#endif // MAINWINDOW_H
