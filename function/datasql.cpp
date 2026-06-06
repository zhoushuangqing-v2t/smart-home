#include "datasql.h"
#include "ui_datasql.h"
#include <QStringList>
#include <QDebug>
#include <QThread>
DataSQL::DataSQL(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DataSQL)
{
    ui->setupUi(this);
}

DataSQL::~DataSQL()
{
    delete ui;
}

void DataSQL::recvDb(QSqlDatabase recvdb)
{
    tableCreate(recvdb);
}

void DataSQL::tableCreate(QSqlDatabase db)
{
    if (!db.open()) {
        qDebug() << "data:Error: Unable to open sqlite!" << db.lastError();
    }else{
        qDebug() << "data:open sqlite";
    }
    // 数据读取
    QSqlQuery query(db);
    query.exec("SELECT * FROM humiture LIMIT 18");
    int row = 0;

    ui->tableWidget->setColumnCount(4);
    ui->tableWidget->setRowCount(18);
    // 设置列标题
    QStringList headers;
    headers << "ID" << "Date" << "Humidity" << "Temperature";
    ui->tableWidget->setHorizontalHeaderLabels(headers);
    while (query.next()) {
        // 获取每一列的值
        QString id = query.value(0).toString();
        QString date = query.value(1).toString();
        QString humidity = query.value(2).toString();
        QString temperature = query.value(3).toString();
        // 将值插入到表格中
        ui->tableWidget->setItem(row, 0, new QTableWidgetItem(id));
        ui->tableWidget->setItem(row, 1, new QTableWidgetItem(date));
        ui->tableWidget->setItem(row, 2, new QTableWidgetItem(humidity));
        ui->tableWidget->setItem(row, 3, new QTableWidgetItem(temperature));
        // 移动到下一行
        row++;
    }
    // 自动调整列宽度
    ui->tableWidget->resizeColumnsToContents();
    // 设置水平头视图策略为自适应模式
    ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void DataSQL::on_closeButton_clicked()
{
    emit this->backMain();
}

