#include "i18n.h"
#include "window.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLockFile>
#include <QFontDatabase>
#include <QIcon>
#include <QMessageBox>
#include <QElapsedTimer>
#include <QDateTime>
#include <QSysInfo>
#include <QCryptographicHash>
#include <QtEndian>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <memory>
#include <csignal>
#include "whisper.h"
#include "ggml-backend.h"

static volatile std::sig_atomic_t shutdownRequested=0;
// Bundled Qt has no host platform-theme plugin. Read cursor preferences before
// Wayland creates its cursor theme, while respecting explicitly set variables.
static void configureCursorTheme() {
    const QString config=qEnvironmentVariable("XDG_CONFIG_HOME",QDir::homePath()+"/.config");
    QString theme,size;
    if(qEnvironmentVariable("XDG_CURRENT_DESKTOP").contains("KDE",Qt::CaseInsensitive)) {
        QSettings kde(config+"/kcminputrc",QSettings::IniFormat);
        theme=kde.value("Mouse/cursorTheme").toString();size=kde.value("Mouse/cursorSize").toString();
    }
    for(const auto &file:{QString("/gtk-4.0/settings.ini"),QString("/gtk-3.0/settings.ini")}) {
        QSettings gtk(config+file,QSettings::IniFormat);
        if(theme.isEmpty())theme=gtk.value("Settings/gtk-cursor-theme-name").toString();
        if(size.isEmpty())size=gtk.value("Settings/gtk-cursor-theme-size").toString();
    }
    if(qEnvironmentVariableIsEmpty("XCURSOR_THEME")&&!theme.isEmpty())qputenv("XCURSOR_THEME",theme.toUtf8());
    bool ok=false;const int pixels=size.toInt(&ok);
    if(qEnvironmentVariableIsEmpty("XCURSOR_SIZE")&&ok&&pixels>0&&pixels<=256)qputenv("XCURSOR_SIZE",size.toUtf8());
    if(qEnvironmentVariableIsEmpty("XCURSOR_PATH")) {
        QStringList paths{qEnvironmentVariable("XDG_DATA_HOME",QDir::homePath()+"/.local/share")+"/icons",QDir::homePath()+"/.icons"};
        for(const auto &path:qEnvironmentVariable("XDG_DATA_DIRS","/usr/local/share:/usr/share").split(':',Qt::SkipEmptyParts))paths<<path+"/icons";
        paths<<"/usr/share/pixmaps";qputenv("XCURSOR_PATH",paths.join(':').toUtf8());
    }
}
static QByteArray loadWav(QString file,QString *error) {
    QFile f(file); if(!f.open(QIODevice::ReadOnly)){*error=f.errorString();return {};}
    auto b=f.readAll(); if(b.size()<44 || b.left(4)!="RIFF" || b.mid(8,4)!="WAVE"){*error="Se necesita WAV PCM de 16 bits, mono, 16000 Hz.";return {};}
    quint16 channels=0,bits=0,format=0;quint32 rate=0;QByteArray samples;
    for(qint64 p=12;p+8<=b.size();) {
        auto size=qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(b.constData()+p+4));
        if(p+8+size>b.size())break;
        if(b.mid(p,4)=="fmt " && size>=16){const uchar *h=reinterpret_cast<const uchar*>(b.constData()+p+8);format=qFromLittleEndian<quint16>(h);channels=qFromLittleEndian<quint16>(h+2);rate=qFromLittleEndian<quint32>(h+4);bits=qFromLittleEndian<quint16>(h+14);}
        if(b.mid(p,4)=="data")samples=b.mid(p+8,size);
        p+=8+size+(size&1);
    }
    if(format!=1||channels!=1||rate!=16000||bits!=16||samples.isEmpty()){*error="Se necesita WAV PCM de 16 bits, mono, 16000 Hz.";return {};}
    QByteArray pcm(samples.size()*2,'\0');
    for(int i=0;i<samples.size()/2;++i){auto s=qFromLittleEndian<qint16>(reinterpret_cast<const uchar*>(samples.constData()+2*i));float value=float(s)/32768.f;std::memcpy(pcm.data()+4*i,&value,4);}
    return pcm;
}
static void jsonLine(QJsonObject obj) {auto line=QJsonDocument(obj).toJson(QJsonDocument::Compact);fwrite(line.constData(),1,line.size(),stdout);fputc('\n',stdout);fflush(stdout);}
int main(int argc,char **argv) {
    configureCursorTheme();
    QApplication app(argc,argv);app.setApplicationName("CaptionsEverywhere");app.setApplicationDisplayName("Captions Everywhere");app.setDesktopFileName("captions_everywhere");app.setApplicationVersion("1.4.2");
    QCommandLineParser parser;parser.setApplicationDescription("Subtítulos locales para Linux y OBS");parser.addHelpOption();parser.addVersionOption();
    parser.addOptions({{"diagnostics","Escribir eventos y transcripciones por stdout (opcional)"},{"root","Carpeta portable (pruebas)","path"},{"headless","Servicio sin ventana"},{"start","Iniciar captura inmediatamente"},{"self-test","Comprobar salida y estabilización sin capturar audio"},{"loop-test-wav","Repetir el WAV de prueba"},{"device","Nombre PulseAudio de la entrada (opcional)","name"},{"test-wav","Prueba de audio WAV PCM16 mono 16kHz en tiempo real","file"},{"language","Idioma de prueba: es/en","lang","es"},{"model","Modelo de prueba: parakeet/base/small","model","base"},{"quit-after","Cerrar después de N segundos","seconds"},{"screenshot","Guardar imagen de la ventana para comprobar el diseño","file"}});
    parser.process(app);
    const bool diagnostics=parser.isSet("diagnostics") || parser.isSet("test-wav");
    std::signal(SIGTERM,[](int){shutdownRequested=1;});std::signal(SIGINT,[](int){shutdownRequested=1;});
    QTimer shutdownTimer;shutdownTimer.setInterval(100);
    QObject::connect(&shutdownTimer,&QTimer::timeout,&app,[&]{if(shutdownRequested)app.quit();});shutdownTimer.start();
    const QString root=QDir::cleanPath(parser.isSet("root")?QFileInfo(parser.value("root")).absoluteFilePath():qEnvironmentVariable("LC_PORTABLE_ROOT",QCoreApplication::applicationDirPath()+"/.."));
    const QString state=QDir::cleanPath(qEnvironmentVariable("CAPTIONS_EVERYWHERE_STATE_DIR",root+"/state"));
    QDir().mkpath(state);
    // A lock copied from another host, boot or folder must not block a portable copy.
    const QByteArray instanceKey=QCryptographicHash::hash(QSysInfo::machineUniqueId()+QSysInfo::bootUniqueId()+state.toUtf8(),QCryptographicHash::Sha256).toHex().left(16);
    QLockFile lock(state+"/app-"+QString::fromLatin1(instanceKey)+".lock");lock.setStaleLockTime(0);
    if(!lock.tryLock(100)){fprintf(stderr,"Captions Everywhere ya está abierto o state/ no permite escritura.\n");return 2;}
    QSaveFile probe(state+"/.write-test");if(!probe.open(QIODevice::WriteOnly)||probe.write("ok")!=2||!probe.commit()){fprintf(stderr,"La carpeta necesita permiso de escritura.\n");return 2;}QFile::remove(state+"/.write-test");
    app.setWindowIcon(QIcon(root+"/resources/logo.png"));
    QFontDatabase::addApplicationFont(root+"/resources/DejaVuSans.ttf");
    whisper_log_set([](enum ggml_log_level level,const char *text,void*){if(level>=GGML_LOG_LEVEL_WARN)fputs(text,stderr);},nullptr);
    ggml_backend_load_all_from_path((root+"/lib").toUtf8().constData());
    Output output(root,state);Audio audio;Recognizer recognizer(root);
    Window window(root,state,&output,&audio,&recognizer);
    auto operationalStatus=[&](QString subsystem,QString message,bool error) {
        output.setHealth({{"status",message},{"subsystem",subsystem},{"error",error},{"updated",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});
        if(!error)return;
        output.setHealth({{"lastError",message},{"lastErrorAt",QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}});
        const auto path=state+"/diagnostics.log";
        QFile file(path);
        if(file.size()>256*1024 && file.open(QIODevice::ReadOnly)) {file.seek(file.size()-128*1024);auto tail=file.readAll();file.close();QSaveFile rotated(path);if(rotated.open(QIODevice::WriteOnly)){rotated.write(tail);rotated.commit();}}
        if(file.open(QIODevice::WriteOnly|QIODevice::Append)){file.write(QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toUtf8()+" "+subsystem.toUtf8()+" "+message.toUtf8()+"\n");}
    };
    QObject::connect(&recognizer,&Recognizer::status,&app,[&](QString s,bool e){operationalStatus("recognition",s,e);});
    QObject::connect(&audio,&Audio::status,&app,[&](QString s,bool e){operationalStatus("audio",s,e);});
    QObject::connect(&output,&Output::error,&app,[&](QString s){operationalStatus("output",s,true);});
    QObject::connect(&recognizer,&Recognizer::metric,&app,[&](qint64 ms,double,int dropped){output.setHealth({{"inferenceMs",ms},{"droppedSamples",dropped}});});

    QObject::connect(&audio,&Audio::samples,&recognizer,&Recognizer::push,Qt::DirectConnection);
    QObject::connect(&recognizer,&Recognizer::caption,&output,&Output::update);
    QObject::connect(&recognizer,&Recognizer::status,&app,[&](QString text,bool error){if(diagnostics)jsonLine({{"event","status"},{"text",text},{"error",error}});});
    QObject::connect(&audio,&Audio::status,&app,[&](QString text,bool error){if(diagnostics)jsonLine({{"event","audio"},{"text",text},{"error",error}});});
    QElapsedTimer timeline;timeline.start();
    QObject::connect(&recognizer,&Recognizer::caption,&app,[&](QString text,bool final){if(diagnostics)jsonLine({{"event","caption"},{"text",text},{"final",final},{"elapsedMs",timeline.elapsed()}});});
    QObject::connect(&recognizer,&Recognizer::metric,&app,[&](qint64 ms,double seconds,int dropped){if(diagnostics)jsonLine({{"event","metric"},{"inferenceMs",ms},{"audioSeconds",seconds},{"droppedSamples",dropped},{"elapsedMs",timeline.elapsed()}});});
    if(parser.isSet("self-test")) {
        Stabilizer s;s.update("hola mundo de prueba",false);s.update("hola mundo de pruebas",false);
        if(s.confirmed!="hola mundo"||s.provisional!="de pruebas")return 3;
        s.update("hola mundo de pruebas",true);if(!s.provisional.isEmpty())return 3;
        output.update("El sistema no corrige palabras",false);
        output.update("El sistema sí corrige las palabras con contexto",false);
        auto corrected=output.snapshot()["text"].toString();
        if(corrected.contains("no corrige") || !corrected.contains("sí corrige"))return 3;
        output.clear();output.update("Hola mundo",true);output.update("Hello world",false);
        auto snap=output.snapshot();QFile file(state+"/captions.txt");file.open(QIODevice::ReadOnly);
        if(file.readAll()!=snap["text"].toString().toUtf8() || !output.port())return 3;
        output.clear();jsonLine({{"event","self-test"},{"ok",true},{"port",output.port()},{"cursorTheme",qEnvironmentVariable("XCURSOR_THEME")},{"cursorSize",qEnvironmentVariable("XCURSOR_SIZE")},{"interfaceLanguage",currentUiLanguage()}});return 0;
    }
    recognizer.start();
    if(!parser.isSet("headless"))window.show();
    if(parser.isSet("test-wav")) {
        QString err;auto pcm=loadWav(parser.value("test-wav"),&err);if(pcm.isEmpty()){fprintf(stderr,"%s\n",err.toUtf8().constData());return 4;}
        // Six seconds of silence allow the last utterance to finalize without storing any recording.
        pcm+=QByteArray(16000*4*6,'\0');
        QString m=parser.value("model"),l=parser.value("language");
        if((m!="base"&&m!="small"&&m!="parakeet")||(l!="es"&&l!="en")){fprintf(stderr,"Modelo o idioma no válido.\n");return 4;}
        recognizer.configure(true,m,l);
        auto payload=std::make_shared<QByteArray>(pcm);auto pos=std::make_shared<int>(0);
        auto timer=new QTimer(&app);timer->setTimerType(Qt::PreciseTimer);timer->setInterval(100);
        QObject::connect(timer,&QTimer::timeout,&app,[&,payload,pos,timer]{if(*pos>=payload->size()){if(parser.isSet("loop-test-wav")){*pos=0;jsonLine({{"event","loop"},{"elapsedMs",timeline.elapsed()}});}else{timer->stop();return;}}recognizer.push(payload->mid(*pos,6400));*pos+=6400;});QObject::connect(&recognizer,&Recognizer::ready,&app,[&,timer]{if(!timer->isActive()){timeline.restart();jsonLine({{"event","ready"}});timer->start();}});
    } else {audio.start();if(parser.isSet("device"))window.selectDevice(parser.value("device"));if(parser.isSet("start"))window.startCapture(true);}
    if(parser.isSet("quit-after")){bool ok=false;double seconds=parser.value("quit-after").toDouble(&ok);if(ok&&seconds>0)QTimer::singleShot(int(seconds*1000),&app,&QCoreApplication::quit);}
    if(parser.isSet("screenshot"))QTimer::singleShot(1200,&app,[&]{window.grab().save(parser.value("screenshot"));});
    QObject::connect(&app,&QCoreApplication::aboutToQuit,&app,[&]{audio.select(false,"@system");recognizer.configure(false,"base","es");output.clear();output.stopTranscript();audio.requestInterruption();recognizer.requestInterruption();});
    int rc=app.exec();audio.requestInterruption();audio.wait();recognizer.requestInterruption();recognizer.wait();return rc;
}
