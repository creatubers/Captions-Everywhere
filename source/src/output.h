#pragma once
#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonObject>
#include <QElapsedTimer>
#include "recognizer.h"

class Output : public QObject {
    Q_OBJECT
public:
    explicit Output(QString root,QString state,QObject *p=nullptr);
    bool listen(quint16 port,QString *error);
    void update(QString text,bool final);
    void clear();
    void setStyle(QJsonObject s);
    void setHealth(QJsonObject delta);
    QJsonObject snapshot() const;
    quint16 port() const { return http.serverPort(); }
    QString writeError;
signals:
    void changed(QString confirmed,QString provisional);
    void error(QString message);
private:
    QString root,state,history;
    Stabilizer stable;
    QJsonObject style,health;
    QTcpServer http;
    QList<QTcpSocket*> peers;
    QElapsedTimer last;
    qint64 sequence=0;
    void publish();
    void acceptHttp();
};
