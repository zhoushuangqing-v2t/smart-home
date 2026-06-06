#ifndef DATASQL_H
#define DATASQL_H
#include <QWidget>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QtSql/QSqlError>
#include <QSqlQueryModel>
namespace Ui {
class DataSQL;
}

class DataSQL : public QWidget
{
    Q_OBJECT

public:
    explicit DataSQL(QWidget *parent = nullptr);
    ~DataSQL();
    void recvDb(QSqlDatabase recvdb);
    void tableCreate(QSqlDatabase db);


private slots:
    void on_closeButton_clicked();
signals:
    void backMain();

private:
    Ui::DataSQL *ui;
//    QSqlDatabase db;  // 定义db
};

#endif // DATASQL_H
