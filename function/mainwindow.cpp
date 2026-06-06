#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "fcntl.h"
#include "unistd.h"
#include "errno.h"
#include "cstring"
#include <QDebug>
#include <QDateTime>
#include <QDialog>
#include <QTableView>
#include <QJsonObject>
#include <QJsonDocument>



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    socket = new QTcpSocket(this);
    socket->connectToHost("192.168.5.10", 8133);
    connect(socket, &QTcpSocket::stateChanged, this, &MainWindow::mstateChanged);
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("temhumData.db");
    if (!db.open()) {
        qDebug() << "Error: Unable to open sqlite!" << db.lastError();
    }else{
        qDebug() << "open sqlite";
    }
//    // function1
//    QTimer *dht11Timer = new QTimer(this);
//    open1();
//    if(dht11Fd >= 0){

//        dht11Timer->setInterval(5000);
//        connect(dht11Timer, &QTimer::timeout, this, &MainWindow::function1);
//        dht11Timer->start();
//    }
////    // function2
//    QTimer *sr501Timer = new QTimer(this);
//    open2();
//    if(sr501Fd >= 0){
//        sr501Timer->setInterval(1000);   // 1s读一次
//        connect(sr501Timer, &QTimer::timeout, this, &MainWindow::function2);
//        sr501Timer->start();
//    }
////    // function3
//    QTimer *irdaTimer = new QTimer(this);
//    open3();
//    ui->motorButton->setText("窗帘关闭");
//    ui->motorButton->setStyleSheet("background-color: green; color: white; border-radius: 10px; padding: 10px;");
//    if(irdaFd >= 0){
//        irdaTimer->setInterval(2000);   // 1s读一次
//        ui->textBrowser->append("function3");
//        connect(irdaTimer, &QTimer::timeout, this, &MainWindow::function3);
//        irdaTimer->start();
//    }

    open1(); //温度控制dht11 sg90
    if(dht11Fd >= 0){
        TimerThread *dht11Thread = new TimerThread(5000, this, SLOT(function1()));
        dht11Thread->start();
    }

    open2();  //异常入侵
    if(sr501Fd >= 0){
        TimerThread *sr501Thread = new TimerThread(1000, this, SLOT(function2()));
        sr501Thread->start();
    }

    open3();
    ui->motorButton->setText("窗帘关闭");
    ui->motorButton->setStyleSheet("background-color: green; color: white; border-radius: 10px; padding: 10px;");
    if(irdaFd >= 0){
        TimerThread *irdaThread = new TimerThread(2000, this, SLOT(function3()));
        irdaThread->start();
    }
}

MainWindow::~MainWindow()
{
    if (dht11Fd >= 0) {
        ::close(dht11Fd);  // 关闭设备文件
        ui->textBrowser->append("Device closed.");
    }
    if (sg90Fd >= 0) {
        ::close(sg90Fd);  // 关闭设备文件
        ui->textBrowser->append("Device closed.");
    }
    if (sr501Fd >= 0) {
        ::close(sr501Fd);  // 关闭设备文件
        ui->textBrowser->append("Device closed.");
    }
    if (sr04Fd >= 0) {
        ::close(sr04Fd);  // 关闭设备文件
        ui->textBrowser->append("Device closed.");
    }
    if (alarmFd >= 0) {
        ::close(alarmFd);  // 关闭设备文件
        ui->textBrowser->append("Device closed.");
    }
    if (irdaFd >= 0) {
        ::close(irdaFd);  // 关闭设备文件
        ui->textBrowser->append("Device closed.");
    }
    if (motorFd >= 0) {
        ::close(motorFd);  // 关闭设备文件
        ui->textBrowser->append("Device closed.");
    }
    // 断开连接
    socket->disconnectFromHost();
    delete ui;
}

void MainWindow::open1()
{
    // open1
    dht11Fd = ::open("/dev/querydht11", O_RDWR);
    if (dht11Fd < 0) {
        ui->textBrowser->append("Failed to open device");
    } else {
        ui->textBrowser->append("Device opened successfully.");
    }

    sg90Fd = ::open("/dev/sg90", O_RDWR);
    if (sg90Fd < 0) {
        ui->textBrowser->append("Failed to open device");
    } else {
        ui->textBrowser->append("Device opened successfully.");
    }
}

void MainWindow::open2()
{
    // open2
    sr501Fd = ::open("/dev/mysr501", O_RDWR);
    if (sr501Fd < 0) {
        ui->textBrowser->append("Failed to open device");
    } else {
        ui->textBrowser->append("Device opened successfully.");
    }

    sr04Fd = ::open("/dev/sr04", O_RDWR);
    if (sr04Fd < 0) {
        ui->textBrowser->append("Failed to open device");
    } else {
        ui->textBrowser->append("Device opened successfully.");
    }

    alarmFd = ::open("/dev/alarm", O_RDWR);
    if (alarmFd < 0) {
        ui->textBrowser->append("Failed to open device");
    } else {
        ui->textBrowser->append("Device opened successfully.");
    }
}

void MainWindow::open3()
{
    // open3
    irdaFd = ::open("/dev/myirda", O_RDWR);
    if (irdaFd < 0) {
        ui->textBrowser->append("Failed to open device");
    } else {
        ui->textBrowser->append("Device opened successfully.");
    }

    motorFd = ::open("/dev/motor", O_RDWR);
    if (motorFd < 0) {
        ui->textBrowser->append("Failed to open device");
    } else {
        ui->textBrowser->append("Device opened successfully.");
    }
}

void MainWindow::function1()
{
    unsigned char readBuf[4];
    char sg90Buf[1];
    ui->textBrowser->append("function1");
    if (::read(dht11Fd, readBuf, 4) == 4){
        ui->textBrowser->append(QString("get humidity: %1 %2").arg(readBuf[0]).arg(readBuf[1]));
        ui->textBrowser->append(QString("get temprature: %1 %2").arg(readBuf[2]).arg(readBuf[3]));
        // 将 char 转换为字符串
        QString intPart1 = QString::number(static_cast<int>(readBuf[0]));
        QString decimalPart1 = QString::number(static_cast<int>(readBuf[1]));
        QString intPart2 = QString::number(static_cast<int>(readBuf[2]));
        QString decimalPart2 = QString::number(static_cast<int>(readBuf[3]));
        // 拼接成一个完整的数字字符串
        QString humidity = intPart1 + "." + decimalPart1;
        QString temperature = intPart2 + "." + decimalPart2;
        // 将字符串设置到 QLineEdit 中
        ui->humEdit->setText(humidity);
        ui->temEdit->setText(temperature);
        // 将字符串转为float
        double humNum = humidity.toDouble();
        double temNum = temperature.toDouble();
        // 将数据发送给服务器
        // 发送消息
        if(socket->state() == QAbstractSocket::ConnectedState){   // 判断是否连接到服务器，然后再给服务器发送数据
            QJsonObject jsonObj;
            jsonObj["humidity"] = humNum;
            jsonObj["temperature"] = temNum;
            jsonObj["alarm"] = -1;
            QJsonDocument jsonDoc(jsonObj);
            QByteArray jsonData = jsonDoc.toJson();
            socket->write(jsonData);   // 发送数据
        }else{
            qDebug() << "与服务器端断开连接";
        }
        // 将温湿度以及当前时间写入数据库中 数据库知识
        QSqlQuery query(db);
        query.prepare("INSERT INTO humiture (Date, humidity, temperature) "
                      "VALUES (:Date, :humidity, :temperature)");
        query.bindValue(":Date", QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
        query.bindValue(":humidity", humNum);
        query.bindValue(":temperature", temNum);
        if (!query.exec()) {
            qDebug() << "Failed to insert data:" << query.lastError();
        }else{
            qDebug() << "Success insert data";
        }
        // 判断温湿度是否超出阈值
        if(humNum >= 50 && temNum >= 28){
            sg90Buf[0] = 1;
            ::write(sg90Fd, sg90Buf, 1);
        }else{
            sg90Buf[0] = 0;
            ::write(sg90Fd, sg90Buf, 1);
        }
    }
    else{
        ui->textBrowser->append("get dht11: -1");
    }
}

void MainWindow::function2()
{
    unsigned char sr501Status;
    int sr04Data;
    if (::read(sr501Fd, &sr501Status, 1) == 1){
        if(sr501Status == 1){
            if (::read(sr04Fd, &sr04Data, 4) == 4){
                int dis = sr04Data * 17 / 1000000;
                ui->textBrowser->append(QString("read sr04: %1 cm").arg(dis));
                // 满足条件，LED亮、报警
                if(dis <= 20){
                    // LED灯亮
                    if(alarmStatus != 1){
                        alarmStatus = 1;
                        ::write(alarmFd, &alarmStatus, 1);
                    }
                    // 将报警信息传递给服务器
                    if(socket->state() == QAbstractSocket::ConnectedState){   // 判断是否连接到服务器，然后再给服务器发送数据
                        QJsonObject jsonObj;
                        jsonObj["humidity"] = -1;
                        jsonObj["temperature"] = -1;
                        jsonObj["alarm"] = 1;
                        QJsonDocument jsonDoc(jsonObj);
                        QByteArray jsonData = jsonDoc.toJson();
                        socket->write(jsonData);   // 发送数据
                    }else{
                        qDebug() << "与服务器端断开连接";
                    }
                }else{
                    if(alarmStatus != 0){
                        alarmStatus = 0;
                        ::write(alarmFd, &alarmStatus, 1);
                    }
                }
            }
        }else{
            if(alarmStatus != 0){
                alarmStatus = 0;
                ::write(alarmFd, &alarmStatus, 1);
            }
        }
    }
}

void MainWindow::function3()
{
    unsigned char irdaStatus[2];
    int res;
    res = ::read(irdaFd, irdaStatus, 2);
    if (res == 2){
        if(irdaStatus[1] == 16){   // 窗帘关闭
            ui->textBrowser->append(QString("read sr501: %1 %2").arg(irdaStatus[0]).arg(irdaStatus[1]));
            if(motorStatus != 1){
                motorStatus = 1;
                ui->textBrowser->append("窗户关闭");
                ::write(motorFd, &motorStatus, 1);
                ui->motorButton->setText("窗帘关闭");
                ui->motorButton->setStyleSheet("background-color: green; color: white; border-radius: 10px; padding: 10px;");
            }else{
                ui->textBrowser->append("窗户已关闭");
            }

        }else if(irdaStatus[1] == 90){
            if(motorStatus != 2){
                motorStatus = 2;
                ui->textBrowser->append(QString("read sr501: %1 %2").arg(irdaStatus[0]).arg(irdaStatus[1]));
                ui->textBrowser->append("窗户打开");
                ::write(motorFd, &motorStatus, 2);   // 这里之后要改为2
                ui->motorButton->setText("窗帘打开");
                ui->motorButton->setStyleSheet("background-color: red; color: white; border-radius: 10px; padding: 10px;");
            }else{
                ui->textBrowser->append("窗户已打开");
            }
        }
     }else{

        ui->textBrowser->append("read err");
    }
}

void MainWindow::back()
{
    this->show();
    datasql->hide();
}

void MainWindow::mstateChanged(QAbstractSocket::SocketState state)
{
    switch (state) {
        // 断开状态
        case QAbstractSocket::UnconnectedState:
            qDebug() << "断开连接";
            break;
        // 已经连接
        case QAbstractSocket::ConnectedState:
            qDebug() << "连接上服务器";
            break;
        default:
            break;
    }
}

void MainWindow::on_researchData_clicked()
{
    datasql = new DataSQL;
    connect(datasql, &DataSQL::backMain, this, &MainWindow::back);
    connect(this, &MainWindow::sendDb, datasql, &DataSQL::recvDb);  // 给DataSQL传db
    this->hide();
    datasql->show();
    emit this->sendDb(db);
}

