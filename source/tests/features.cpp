// Runs in seconds without starting capture or loading any speech model.
#include "window.h"
#include "i18n.h"
#include "desktop.h"
#include <QApplication>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QDialog>
#include <QDialogButtonBox>
#include <QPushButton>
#include <stdexcept>
#include <cstdio>
static void check(bool ok,const char *what){if(!ok)throw std::runtime_error(what);}
static QByteArray read(QString path){QFile f(path);check(f.open(QIODevice::ReadOnly),"read file");return f.readAll();}
static QComboBox *input(Window &w){for(auto c:w.findChildren<QComboBox*>())if(c->findData("@system")>=0)return c;throw std::runtime_error("input selector");}
int main(int argc,char **argv){
    QApplication app(argc,argv);
    try {
        const auto savedLocale=qgetenv("CAPTIONS_EVERYWHERE_SYSTEM_LOCALE");
        for(const auto &locale:{"es_ES.UTF-8","es_MX.UTF-8","es-AR","es","es_ES:en_US"}) {
            qputenv("CAPTIONS_EVERYWHERE_SYSTEM_LOCALE",locale);setUiLanguage("auto");check(currentUiLanguage()=="es","Spanish desktop language");
        }
        for(const auto &locale:{"en_US.UTF-8","fr_FR.UTF-8","de_DE.UTF-8","pt_BR.UTF-8","C.UTF-8","eo","en_US:es_ES",""}) {
            qputenv("CAPTIONS_EVERYWHERE_SYSTEM_LOCALE",locale);setUiLanguage("auto");check(currentUiLanguage()=="en","non-Spanish desktop uses English");
        }
        qputenv("CAPTIONS_EVERYWHERE_SYSTEM_LOCALE","es_ES.UTF-8");setUiLanguage("en");check(currentUiLanguage()=="en","manual English overrides automatic");
        qputenv("CAPTIONS_EVERYWHERE_SYSTEM_LOCALE","fr_FR.UTF-8");setUiLanguage("es");check(currentUiLanguage()=="es","manual Spanish overrides automatic");
        if(savedLocale.isNull())qunsetenv("CAPTIONS_EVERYWHERE_SYSTEM_LOCALE");else qputenv("CAPTIONS_EVERYWHERE_SYSTEM_LOCALE",savedLocale);
        const auto oldPlugins=qgetenv("QT_PLUGIN_PATH");qputenv("QT_PLUGIN_PATH","/invalid/bundle/plugins");
        const auto host=desktopEnvironment();check(!host.contains("QT_PLUGIN_PATH")&&!host.contains("QT_QPA_PLATFORM"),"desktop environment excludes bundle Qt settings");
        check(host.value("DBUS_SESSION_BUS_ADDRESS")==qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"),"desktop session retained");
        if(oldPlugins.isNull())qunsetenv("QT_PLUGIN_PATH");else qputenv("QT_PLUGIN_PATH",oldPlugins);
        if(argc==3 && QString(argv[1])=="--open-transcripts") {
            const auto path=QString::fromLocal8Bit(argv[2]);check(QDir().mkpath(path),"transcript directory");
            auto opener=openDesktopFolder(path,&app);opener->start();
            check(opener->waitForFinished(10000),"host opener finished");
            check(opener->exitStatus()==QProcess::NormalExit && opener->exitCode()==0,"host opener succeeded");
            check(!opener->readAllStandardError().contains("Segmentation fault"),"no desktop crash");
            puts("PASS: host transcript folder opened with clean environment");return 0;
        }
        QTemporaryDir tmp;check(tmp.isValid(),"temporary directory");
        const QString root=tmp.path(),state=root+"/state";
        QDir().mkpath(state);QDir().mkpath(root+"/models/parakeet");
        QFile encoder(root+"/models/parakeet/encoder.int8.onnx");check(encoder.open(QIODevice::WriteOnly),"placeholder");encoder.close();
        QTcpServer reserve;check(reserve.listen(QHostAddress::LocalHost,0),"reserve port");const int port=reserve.serverPort();reserve.close();
        {QSettings s(state+"/settings.ini",QSettings::IniFormat);s.setValue("input","@mic");s.setValue("port",port);s.setValue("uiLanguageMode","es");}
        {
            Output out(root,state);Audio audio;Recognizer recognizer(root);Window w(root,state,&out,&audio,&recognizer);
            check(input(w)->currentData()=="@mic","restore mic before enumeration");
            audio.devices({"usb_mic"},{"USB microphone"});check(input(w)->currentData()=="@mic","enumeration keeps mic");
            bool accepted=false;
            QTimer::singleShot(0,[&]{
                auto dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());if(!dialog)return;
                for(auto c:dialog->findChildren<QComboBox*>())if(c->findText("English")>=0)c->setCurrentIndex(c->findData("en"));
                auto buttons=dialog->findChild<QDialogButtonBox*>();if(buttons){accepted=true;buttons->button(QDialogButtonBox::Ok)->click();}
            });
            for(auto b:w.findChildren<QPushButton*>())if(b->text()=="Ajustes"){b->click();break;}
            check(accepted,"language dialog accepted");check(ui("Ajustes")=="Settings","English applied immediately");
            check(input(w)->itemText(0)=="System audio","main window translated");
            check(w.findChild<QCheckBox*>()->text()=="Save transcript","save control translated");
            w.selectDevice("usb_mic");
            bool openLabel=false;for(auto b:w.findChildren<QPushButton*>())if(b->text()=="Open transcripts")openLabel=true;
            check(openLabel,"explicit open transcripts label");
            if(argc==3 && QString(argv[1])=="--screenshots") {
                w.show();app.processEvents();auto checkbox=w.findChild<QCheckBox*>();
                check(w.grab().save(QString(argv[2])+"-off.png"),"unchecked screenshot");
                checkbox->blockSignals(true);checkbox->setChecked(true);
                check(w.grab().save(QString(argv[2])+"-on.png"),"checked screenshot");
                checkbox->setChecked(false);checkbox->blockSignals(false);
            }
        }
        {
            Output out(root,state);Audio audio;Recognizer recognizer(root);Window w(root,state,&out,&audio,&recognizer);
            check(input(w)->currentData()=="usb_mic","specific mic survives restart");check(ui("Listo")=="Ready","English survives restart");
            audio.devices({},{});check(input(w)->currentData()=="usb_mic","disconnected device retained");
            audio.devices({"usb_mic"},{"USB microphone"});check(input(w)->currentText()=="USB microphone","reconnected device restored");
            bool onlyInstalledModel=false;
            for(auto c:w.findChildren<QComboBox*>())if(c->findData("parakeet")>=0)onlyInstalledModel=c->count()==1;
            check(onlyInstalledModel,"lists only installed model");
        }
        Output out(root,state);QString error;
        check(!QFile::exists(state+"/transcripts"),"no transcript by default");
        check(out.startTranscript(&error),"start transcript");const auto path=out.transcriptPath();
        out.update("Esto no funciona",false);out.update("Esto sí funciona",false);out.update("Esto sí funciona",true);
        out.update("Hello world",true);out.clear();out.update("Última frase",false);out.stopTranscript();
        check(read(path)==QString("Esto sí funciona\nHello world\nÚltima frase\n").toUtf8(),"corrections, both languages, last provisional saved once");
        out.update("Must not save",true);out.clear();check(!read(path).contains("Must not"),"saving stops");
        check(out.startTranscript(&error),"second session");check(out.transcriptPath()!=path,"unique session file");
        const auto second=out.transcriptPath();out.update("Before pause",false);out.clear();out.clear();out.update("After pause",true);out.stopTranscript();
        check(read(second)=="Before pause\nAfter pause\n","pause does not duplicate or lose text");
        const QString invalid=root+"/not-a-directory";QFile blocker(invalid);check(blocker.open(QIODevice::WriteOnly),"blocker");blocker.close();
        Output denied(root,invalid);check(!denied.startTranscript(&error)&&!error.isEmpty(),"save failure diagnosed");
        puts("PASS: language switch/restart, audio restore/reconnect, installed model menu, corrected transcript, pause/stop, save errors");return 0;
    }catch(const std::exception &e){fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
