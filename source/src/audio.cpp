#include "i18n.h"
#include "audio.h"
#include <QMutexLocker>
#include <QElapsedTimer>

void Audio::select(bool enabled,const QString &device) {
    QMutexLocker lock(&mutex); wanted=enabled; chosen=device;
}
void Audio::serverInfo(pa_context*,const pa_server_info *i,void *u) {
    auto a=static_cast<Audio*>(u);
    if(i) { a->defaultSink=QString::fromUtf8(i->default_sink_name); a->defaultSource=QString::fromUtf8(i->default_source_name); }
}
void Audio::sinkInfo(pa_context*,const pa_sink_info *i,int eol,void *u) {
    auto a=static_cast<Audio*>(u);
    if(!eol && i && QString::fromUtf8(i->name)==a->defaultSink)
        a->target=QString::fromUtf8(i->monitor_source_name);
}
void Audio::sourceInfo(pa_context*,const pa_source_info *i,int eol,void *u) {
    auto a=static_cast<Audio*>(u);
    if(eol) { emit a->devices(a->names,a->descriptions); return; }
    if(i && i->monitor_of_sink==PA_INVALID_INDEX) {
        a->names << QString::fromUtf8(i->name);
        a->descriptions << QString::fromUtf8(i->description ? i->description : i->name);
    }
}
void Audio::readAudio(pa_stream *s,size_t,void *u) {
    auto a=static_cast<Audio*>(u);
    const void *data=nullptr; size_t bytes=0;
    if(pa_stream_peek(s,&data,&bytes)<0) return;
    if(bytes) {
        QByteArray pcm=data ? QByteArray(static_cast<const char*>(data),int(bytes)) : QByteArray(int(bytes),'\0');
        emit a->samples(pcm);
        pa_stream_drop(s);
    }
}
void Audio::closeStream() {
    if(stream) { pa_stream_set_read_callback(stream,nullptr,nullptr); pa_stream_disconnect(stream); pa_stream_unref(stream); stream=nullptr; }
    active.clear();
}
void Audio::run() {
    QElapsedTimer refresh; refresh.start();
    while(!isInterruptionRequested()) {
        loop=pa_mainloop_new();
        ctx=pa_context_new(pa_mainloop_get_api(loop),"Captions Everywhere");
        if(pa_context_connect(ctx,nullptr,PA_CONTEXT_NOFLAGS,nullptr)<0) {
            emit status(ui("No se puede conectar al audio. Comprueba PipeWire/PulseAudio y los permisos de tu sesión."),true);
        } else {
            serverReady=false; target.clear(); defaultSink.clear(); refresh.invalidate();
            while(!isInterruptionRequested()) {
                int result=0;
                if(pa_mainloop_iterate(loop,0,&result)<0) break;
                auto state=pa_context_get_state(ctx);
                if(!PA_CONTEXT_IS_GOOD(state)) break;
                if(state!=PA_CONTEXT_READY) { msleep(10); continue; }
                if(!refresh.isValid() || refresh.elapsed()>1500) {
                    auto op=pa_context_get_server_info(ctx,serverInfo,this); if(op) pa_operation_unref(op);
                    names.clear(); descriptions.clear();
                    op=pa_context_get_source_info_list(ctx,sourceInfo,this); if(op) pa_operation_unref(op);
                    if(!defaultSink.isEmpty()) { op=pa_context_get_sink_info_by_name(ctx,defaultSink.toUtf8().constData(),sinkInfo,this); if(op) pa_operation_unref(op); }
                    refresh.start();
                }
                bool enabled; QString device;
                { QMutexLocker lock(&mutex); enabled=wanted; device=chosen; }
                QString next=device=="@system" ? target : (device=="@mic" ? defaultSource : device);
                if(!enabled || next!=active || (stream && !PA_STREAM_IS_GOOD(pa_stream_get_state(stream)))) closeStream();
                if(enabled && !next.isEmpty() && !stream) {
                    pa_sample_spec spec{PA_SAMPLE_FLOAT32LE,16000,1};
                    stream=pa_stream_new(ctx,"Captions Everywhere",&spec,nullptr);
                    if(!stream) { emit status(ui("No se puede crear la captura de audio."),true); msleep(500); continue; }
                    pa_stream_set_read_callback(stream,readAudio,this);
                    pa_buffer_attr attr; attr.maxlength=uint32_t(-1); attr.tlength=uint32_t(-1); attr.prebuf=uint32_t(-1); attr.minreq=uint32_t(-1); attr.fragsize=3200;
                    if(pa_stream_connect_record(stream,next.toUtf8().constData(),&attr,pa_stream_flags_t(PA_STREAM_ADJUST_LATENCY|PA_STREAM_AUTO_TIMING_UPDATE))<0) {
                        emit status(ui("Entrada de audio no disponible: ")+next,true); closeStream(); msleep(500);
                    } else { active=next; serverReady=false; }
                }
                if(stream && pa_stream_get_state(stream)==PA_STREAM_READY && !serverReady) { serverReady=true; emit status(ui("Audio conectado"),false); }
                if(stream && pa_stream_get_state(stream)==PA_STREAM_FAILED) { emit status(ui("Se perdió la entrada de audio; reconectando…"),true); closeStream(); msleep(500); }
                msleep(10);
            }
        }
        closeStream();
        pa_context_disconnect(ctx); pa_context_unref(ctx); ctx=nullptr;
        pa_mainloop_free(loop); loop=nullptr;
        if(!isInterruptionRequested()) { emit status(ui("Servicio de audio desconectado; reintentando…"),true); for(int i=0;i<20&&!isInterruptionRequested();++i) msleep(100); }
    }
}
