#pragma once
#include <QThread>
#include <QMutex>
#include <QStringList>
#include <pulse/pulseaudio.h>
#include <atomic>

class Audio : public QThread {
    Q_OBJECT
public:
    explicit Audio(QObject *p=nullptr):QThread(p){}
    ~Audio() override { requestInterruption(); wait(); }
    void select(bool enabled, const QString &device);
signals:
    void samples(QByteArray pcm);
    void devices(QStringList names, QStringList descriptions);
    void status(QString text, bool error);
protected:
    void run() override;
private:
    QMutex mutex;
    bool wanted=false;
    QString chosen="@system";
    pa_mainloop *loop=nullptr;
    pa_context *ctx=nullptr;
    pa_stream *stream=nullptr;
    QString target, active, defaultSink, defaultSource;
    QStringList names, descriptions;
    bool serverReady=false, enumerate=false;
    static void serverInfo(pa_context*, const pa_server_info*, void*);
    static void sinkInfo(pa_context*, const pa_sink_info*, int, void*);
    static void sourceInfo(pa_context*, const pa_source_info*, int, void*);
    static void readAudio(pa_stream*, size_t, void*);
    void closeStream();
};
