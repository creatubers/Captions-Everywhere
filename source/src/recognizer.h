#pragma once
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <atomic>
#include <vector>

class Stabilizer {
public:
    QString confirmed, provisional, previous;
    void update(const QString &text,bool final);
    void clear() { confirmed.clear(); provisional.clear(); previous.clear(); }
};
class Recognizer : public QThread {
    Q_OBJECT
public:
    explicit Recognizer(QString root,QObject *p=nullptr):QThread(p),root(std::move(root)){}
    ~Recognizer() override;
    void configure(bool enabled,QString model,QString language);
    void push(const QByteArray &pcm);
signals:
    void ready();
    void caption(QString text,bool final);
    void status(QString text,bool error);
    void metric(qint64 inferenceMs,double audioSeconds,int droppedSamples);
protected:
    void run() override;
private:
    QString root,model="base",language="es";
    QMutex mutex;
    QWaitCondition wake;
    std::vector<float> pending;
    bool enabled=false,prime=false;
    unsigned generation=0;
    int dropped=0;
};
