#include "i18n.h"
#include "recognizer.h"
#include "whisper.h"
#include "c-api.h"
#include <QMutexLocker>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <thread>

void Stabilizer::update(const QString &text,bool final) {
    const QString clean=text.simplified();
    if(final) { confirmed=clean; provisional.clear(); previous=clean; return; }
    auto a=previous.split(' ',Qt::SkipEmptyParts), b=clean.split(' ',Qt::SkipEmptyParts);
    int common=0;
    while(common<a.size() && common<b.size() && a[common]==b[common]) ++common;
    // A word remains provisional until another decoding agrees; keep the last two words revisable.
    common=std::min(common,std::max(0,int(b.size())-2));
    confirmed=b.mid(0,common).join(' '); provisional=b.mid(common).join(' '); previous=clean;
}
Recognizer::~Recognizer() { requestInterruption(); wake.wakeAll(); wait(); }
void Recognizer::configure(bool on,QString m,QString l) {
    QMutexLocker lock(&mutex); enabled=on; prime=true; model=std::move(m); language=std::move(l); ++generation; pending.clear(); wake.wakeAll();
}
void Recognizer::push(const QByteArray &pcm) {
    QMutexLocker lock(&mutex);
    if(!enabled) return;
    const size_t n=pcm.size()/sizeof(float), old=pending.size();
    pending.resize(old+n); std::memcpy(pending.data()+old,pcm.constData(),n*sizeof(float));
    // Only latest audio: never create an unbounded inference queue.
    if(pending.size()>16000*8) { size_t loss=pending.size()-16000*8; dropped+=int(loss); pending.erase(pending.begin(),pending.begin()+loss); }
    wake.wakeOne();
}
void Recognizer::run() {
    whisper_context *ctx=nullptr;
    whisper_vad_context *vad=nullptr;
    QString loaded, contextText, lastHypothesis;
    const SherpaOnnxOfflineRecognizer *offline=nullptr;
    unsigned current=~0u;
    std::vector<float> utterance, preRoll, vadHistory;
    int vadTick=0; bool vadVoice=false;
    bool speaking=false;
    int silence=0, sinceDecode=0;
    QElapsedTimer inputIdle; inputIdle.start();
    const int rate=16000, block=512;
    while(!isInterruptionRequested()) {
        std::vector<float> input;
        QString m,lang; unsigned gen; int lost;bool isPrime=false;
        {
            QMutexLocker lock(&mutex);
            if(!prime && (!enabled || pending.size()<block)) {
                if(enabled && speaking && inputIdle.elapsed()>160) {
                    // Some monitor devices stop delivering frames when playback ends.
                    // Account for that wall-clock silence so the last sentence finalizes.
                    pending.insert(pending.end(),2560,0.f);
                } else {wake.wait(&mutex,80);continue;}
            }
            isPrime=prime;prime=false;
            gen=generation; m=model; lang=language; lost=dropped; dropped=0;
            input.swap(pending);inputIdle.restart();
        }
        if(gen!=current || lost>0) { utterance.clear(); preRoll.clear(); vadHistory.clear(); vadTick=0; vadVoice=false; speaking=false; silence=0; sinceDecode=0; current=gen; contextText.clear(); lastHypothesis.clear(); }
        if(lost>0) {emit caption(QString(),true);emit status(ui("Se omitió audio antiguo para mantener el directo."),true);}
        if(loaded!=m || (!ctx && m!="parakeet") || !vad) {
            if(ctx) { whisper_free(ctx); ctx=nullptr; }
            if(vad) { whisper_vad_free(vad); vad=nullptr; }
            const auto path=root+"/models/ggml-"+m+"-q5_1.bin";
            const auto vpath=root+"/models/ggml-silero-v5.1.2.bin";
            if((m!="parakeet" && !QFileInfo::exists(path)) || !QFileInfo::exists(vpath)) {
                emit status(ui("Falta un modelo en models/. Vuelve a copiar la carpeta completa."),true);
                QMutexLocker lock(&mutex); enabled=false; continue;
            }
            emit status(ui("Cargando modelo ")+m+"…",false);
            auto cp=whisper_context_default_params(); cp.use_gpu=false;
            if(m=="parakeet") {
                if(!offline) {
                    const QByteArray dir=(root+"/models/parakeet/").toUtf8();
                    const QByteArray encoder=dir+"encoder.int8.onnx",decoder=dir+"decoder.int8.onnx",joiner=dir+"joiner.int8.onnx",tokens=dir+"tokens.txt";
                    if(!QFileInfo::exists(QString::fromUtf8(encoder)) || !QFileInfo::exists(QString::fromUtf8(decoder)) || !QFileInfo::exists(QString::fromUtf8(joiner)) || !QFileInfo::exists(QString::fromUtf8(tokens))) {
                        emit status(ui("Falta un archivo en models/parakeet/. Copia la carpeta completa."),true);QMutexLocker lock(&mutex);enabled=false;continue;
                    }
                    SherpaOnnxOfflineRecognizerConfig config{};
                    config.feat_config.sample_rate=16000;config.feat_config.feature_dim=80;
                    config.model_config.transducer.encoder=encoder.constData();config.model_config.transducer.decoder=decoder.constData();config.model_config.transducer.joiner=joiner.constData();
                    config.model_config.tokens=tokens.constData();config.model_config.provider="cpu";config.model_config.num_threads=3;config.model_config.model_type="nemo_transducer";
                    config.decoding_method="greedy_search";
                    offline=SherpaOnnxCreateOfflineRecognizer(&config);
                }
            } else ctx=whisper_init_from_file_with_params(path.toUtf8().constData(),cp);
            auto vp=whisper_vad_default_context_params(); vp.use_gpu=false; vp.n_threads=1;
            vad=whisper_vad_init_from_file_with_params(vpath.toUtf8().constData(),vp);
            if((m=="parakeet"?!offline:!ctx) || !vad) { emit status(ui("No se pudo cargar el modelo. Comprueba los archivos y la memoria disponible."),true); QMutexLocker lock(&mutex); enabled=false; continue; }
            loaded=m; emit status(ui("Listo"),false);
        }
        if(isPrime) {bool valid;{QMutexLocker lock(&mutex);valid=generation==gen;}if(valid)emit ready();}
        if(input.empty())continue;
        // Preserve pre-roll; run VAD over the most recent second so its recurrent state has context.
        size_t pos=0;
        while(pos+block<=input.size() && !isInterruptionRequested()) {
            { QMutexLocker lock(&mutex); if(!enabled || generation!=gen) break; }
            const float *frame=input.data()+pos;
            double rms=0; for(int i=0;i<block;++i) rms+=frame[i]*frame[i]; rms=std::sqrt(rms/block);
            vadHistory.insert(vadHistory.end(),frame,frame+block);
            if(vadHistory.size()>size_t(rate)) vadHistory.erase(vadHistory.begin(),vadHistory.end()-rate);
            if(++vadTick>=4) {
                vadTick=0; vadVoice=false;
                if(whisper_vad_detect_speech(vad,vadHistory.data(),int(vadHistory.size()))) {
                    const int count=whisper_vad_n_probs(vad); auto probs=whisper_vad_probs(vad);
                    for(int i=std::max(0,count-4);i<count;++i) if(probs[i]>=0.5f) vadVoice=true;
                }
            }
            bool voice=vadVoice && rms>0.0005;
            if(!speaking) {
                preRoll.insert(preRoll.end(),frame,frame+block);
                if(preRoll.size()>size_t(rate)) preRoll.erase(preRoll.begin(),preRoll.end()-rate);
                if(voice) { speaking=true; utterance=preRoll; preRoll.clear(); silence=0; sinceDecode=0; }
            } else { utterance.insert(utterance.end(),frame,frame+block); }
            pos+=block;
            if(!speaking) continue;
            silence=voice ? 0 : silence+block;
            sinceDecode+=block;
            // A brief pause requests a better hypothesis but does not freeze those words.
            // Keep the acoustic context through pauses, allowing a later phrase to revise them.
            const bool end=silence>=int(rate*.9) && silence-block<int(rate*.9);
            const bool longPause=silence>=rate*6;
            const bool cut=utterance.size()>=size_t(rate*20) || longPause;
            // Coalesce every batch received during the previous decode into one new hypothesis.
            // Decoding each old second separately would accumulate latency on a busy CPU.
            const bool tail=pos+block>input.size();
            const bool decode=(tail && silence<rate && sinceDecode>=int(rate*.65) && utterance.size()>=size_t(rate*.8)) || end || cut;
            if(!decode) continue;
            sinceDecode=0;
            QString result=(longPause ? lastHypothesis : QString());
            qint64 elapsed=0;
            if(!longPause && utterance.size()>=size_t(rate*.4)) {
                QElapsedTimer processing;processing.start();
                if(m=="parakeet") {
                    auto recording=SherpaOnnxCreateOfflineStream(offline);
                    SherpaOnnxAcceptWaveformOffline(recording,16000,utterance.data(),int(utterance.size()));
                    SherpaOnnxDecodeOfflineStream(offline,recording);
                    auto text=SherpaOnnxGetOfflineStreamResult(recording);
                    result=QString::fromUtf8(text->text).simplified();
                    SherpaOnnxDestroyOfflineRecognizerResult(text);SherpaOnnxDestroyOfflineStream(recording);
                    elapsed=processing.elapsed();
                } else {
                auto p=whisper_full_default_params((end||cut) ? WHISPER_SAMPLING_BEAM_SEARCH : WHISPER_SAMPLING_GREEDY);
                p.beam_search.beam_size=3;
                p.n_threads=std::min(6,std::max(1,int(std::thread::hardware_concurrency())/2));
                p.language=(lang=="en" ? "en" : "es"); p.translate=false;
                // Discard stale partial hypotheses in whisper's internal cache. Supply only
                // finalized previous text explicitly; the current 20-second audio is re-decoded.
                p.no_context=true; p.no_timestamps=true; p.single_segment=true;
                const QByteArray prompt=contextText.toUtf8();
                p.initial_prompt=prompt.isEmpty()?nullptr:prompt.constData();
                p.n_max_text_ctx=224;
                p.print_progress=false; p.print_realtime=false; p.print_timestamps=false; p.print_special=false;
                p.suppress_blank=true; p.suppress_nst=true; p.temperature=0; p.temperature_inc=0;
                p.max_tokens=256; p.audio_ctx=0; // full trained audio context; no experimental truncation
                // Abort a long decode on pause, language/model change or shutdown.
                struct Abort { Recognizer *self; unsigned gen; } abort{this,gen};
                p.abort_callback=[](void *u) { auto a=static_cast<Abort*>(u); QMutexLocker l(&a->self->mutex); return a->self->isInterruptionRequested() || !a->self->enabled || a->self->generation!=a->gen; };
                p.abort_callback_user_data=&abort;
                QElapsedTimer timer; timer.start();
                const int rc=whisper_full(ctx,p,utterance.data(),int(utterance.size()));
                elapsed=timer.elapsed();
                if(rc==0) {
                    const int n=whisper_full_n_segments(ctx);
                    for(int i=0;i<n;++i) if(whisper_full_get_segment_no_speech_prob(ctx,i)<0.65f) result+=QString::fromUtf8(whisper_full_get_segment_text(ctx,i));
                    result=result.simplified();
                } else { QMutexLocker l(&mutex); if(enabled && generation==gen) emit status(ui("Error durante el reconocimiento; se reintentará con el siguiente fragmento."),true); }
            }
                }
            bool valid;
            { QMutexLocker lock(&mutex); valid=enabled && generation==gen; }
            if(valid) {
                if(!result.isEmpty()) {emit caption(result,cut);lastHypothesis=result;}
                if(!cut && !result.isEmpty()) emit status(ui("Escuchando"),false);
                emit metric(elapsed,double(utterance.size())/rate,lost); lost=0;
            }
            if(cut) {
                // Next utterance carries only audio overlap; output de-duplicates matching boundary words.
                preRoll.clear();
                if(!longPause) preRoll.assign(utterance.end()-rate*3/4,utterance.end());
                if(!result.isEmpty()) {
                    contextText=(contextText+" "+result).simplified();
                    auto words=contextText.split(' ',Qt::SkipEmptyParts);
                    contextText=words.mid(std::max(0,int(words.size())-100)).join(' ');
                }
                lastHypothesis.clear();
                utterance.clear(); speaking=false; silence=0;
            }
        }
        if(pos<input.size()) { QMutexLocker lock(&mutex); if(generation==gen) pending.insert(pending.begin(),input.begin()+pos,input.end()); }
    }
    if(offline)SherpaOnnxDestroyOfflineRecognizer(offline);
    if(ctx) whisper_free(ctx);
    if(vad) whisper_vad_free(vad);
}
