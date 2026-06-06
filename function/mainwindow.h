#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QSqlQueryModel>
#include <QTcpSocket>
#include "datasql.h"
#include <QThread>
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

// 创建一个用于运行定时器和函数的线程类
class TimerThread : public QThread {
    Q_OBJECT

public:
    TimerThread(int interval, QObject *receiver, const char *member, QObject *parent = nullptr)
        : QThread(parent), m_interval(interval), m_receiver(receiver), m_member(member) {}

    void run() override {
        QTimer timer;
        connect(&timer, SIGNAL(timeout()), m_receiver, m_member);
        timer.setInterval(m_interval);
        timer.start();
        exec();  // 进入事件循环，保持线程运行
    }

private:
    int m_interval;
    QObject *m_receiver;
    const char *m_member;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    void open1();
    void open2();
    void open3();


    void back();
    void mstateChanged(QAbstractSocket::SocketState state);

private slots:
    void on_researchData_clicked();
    void function1();
    void function2();
    void function3();
signals:
    void sendDb(QSqlDatabase db);
private:
    Ui::MainWindow *ui;
    int dht11Fd;
    int sg90Fd;
    int sr501Fd;
    int sr04Fd;
    int alarmFd;
    int irdaFd;
    int motorFd;
    unsigned char alarmStatus = 0;
    unsigned char motorStatus = 1;
    QTimer *dht11Timer;  // 定时器对象
    QTimer *sr501Timer;  // 定时器对象
    QTimer *irdaTimer;  // 定时器对象
    QSqlDatabase db;    // 数据库对象
    QSqlQuery query;
    DataSQL *datasql;    // 显示表格的界面
    QTcpSocket *socket;  // 客户端
};
#endif // MAINWINDOW_H
