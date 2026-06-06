#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    server = new QTcpServer(this);
    server->listen(QHostAddress("192.168.5.10"), 8133);
    this->setGeometry(700, 350, 1400, 800);
    this->setWindowTitle("监控系统");
    connect(server, &QTcpServer::newConnection, this, &MainWindow::mNewConnection);  // 如果有新的客户端连接会触发信号槽
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::mNewConnection()
{
    QTcpSocket *tmpTcpSocket = server->nextPendingConnection();  // 接收到客户端的连接
    ui->textBrowser->append("客户端建立连接");
    connect(tmpTcpSocket, &QTcpSocket::readyRead, this, &MainWindow::mRecvMessage);   // 接收客户端的数据
    connect(tmpTcpSocket, &QTcpSocket::stateChanged, this, &MainWindow::mStateChanged); // 状态改变
}

void MainWindow::mRecvMessage()
{
    QTcpSocket *tmpTcpSocket = (QTcpSocket *)sender();  // 返回指向客户端的指针
    QByteArray jsonData = tmpTcpSocket->readAll();
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    QJsonObject jsonObj = jsonDoc.object();
    double humidity = jsonObj["humidity"].toDouble();
    double temperature = jsonObj["temperature"].toDouble();
    int alarm = jsonObj["alarm"].toInt();
    qDebug() << "Received humidity:" << humidity;
    qDebug() << "Received temperature:" << temperature;
    qDebug() << "Received alarm:" << alarm;

    ui->textBrowser->append("接收到客户端数据");
    if(humidity != -1 && temperature != -1){
        QString hum = QString::number(humidity);
        QString tem = QString::number(temperature);
        ui->lineEdit1->setText(hum);
        ui->lineEdit2->setText(tem);
    }else if(alarm == 1){
        QMessageBox::critical(this, "警告", "有人入侵，请注意！！！");
    }

}

void MainWindow::mStateChanged(QAbstractSocket::SocketState state)
{
    QTcpSocket *tmpTcpSocket = (QTcpSocket *)sender();  // 返回指向客户端的指针
    switch (state) {
    // 断开状态
    case QAbstractSocket::UnconnectedState:
        tmpTcpSocket->deleteLater();   // 延时一段时间释放客户端socket
        ui->textBrowser->append("客户端断开连接");
        break;
    // 已经连接
    case QAbstractSocket::ConnectedState:
        break;
    default:
        break;
    }
}
