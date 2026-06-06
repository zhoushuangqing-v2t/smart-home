#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpServer>
#include <QTcpSocket>
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    QTcpServer *server;   // 创建服务器
    void mNewConnection();  // 建立客户端的连接
    void mRecvMessage();    // 收到客户端的数据
    void mStateChanged(QAbstractSocket::SocketState socketState);    // 状态改变
};
#endif // MAINWINDOW_H
