#include "i18n.h"
#include "window.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QPixmap>
#include <QTextLayout>
#include <QPushButton>
#include <QDialog>
#include <QFormLayout>
#include <QSpinBox>
#include <QColorDialog>
#include <QCheckBox>
#include <QDialogButtonBox>
#include "desktop.h"
#include <QStyleOptionButton>
#include <QPainterPath>
#include <QUrl>
#include <QCloseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QClipboard>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <cmath>

void VisibleCheckBox::paintEvent(QPaintEvent *event) {
    QCheckBox::paintEvent(event);
    QStyleOptionButton option;initStyleOption(&option);
    const QRectF box=style()->subElementRect(QStyle::SE_CheckBoxIndicator,&option,this).adjusted(1,1,-1,-1);
    QPainter painter(this);painter.setRenderHint(QPainter::Antialiasing);
    if(!isEnabled())painter.setOpacity(.5);
    painter.setPen(QPen(QColor(isChecked()?"#8bd5b6":"#a8bdd6"),1.5));
    painter.setBrush(QColor(isChecked()?"#8bd5b6":"#172235"));painter.drawRoundedRect(box,3,3);
    if(isChecked()) {
        QPainterPath tick;tick.moveTo(box.left()+box.width()*.20,box.top()+box.height()*.52);
        tick.lineTo(box.left()+box.width()*.43,box.top()+box.height()*.75);
        tick.lineTo(box.left()+box.width()*.82,box.top()+box.height()*.25);
        painter.setPen(QPen(QColor("#0b101a"),2.2,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));painter.drawPath(tick);
    }
}
void CaptionView::paintEvent(QPaintEvent*) {
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    QColor bg=background; bg.setAlphaF(opacity/100.0); painter.setBrush(bg); painter.setPen(Qt::NoPen); painter.drawRoundedRect(rect(),12,12);
    QFont font("DejaVu Sans"); font.setPixelSize(fontSize); font.setWeight(QFont::Medium);
    QString text=confirmed+(confirmed.isEmpty()||provisional.isEmpty()?"":" ")+provisional;
    if(text.isEmpty()) { font.setPixelSize(20); painter.setFont(font); painter.setPen(QColor("#94a3b8")); painter.drawText(rect().adjusted(20,15,-20,-15),Qt::AlignVCenter|Qt::TextWordWrap,ui("Los subtítulos aparecerán aquí")); return; }
    QTextLayout layout(text,font);
    QTextOption option; option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere); layout.setTextOption(option);
    QTextCharFormat full; full.setForeground(foreground);
    QTextCharFormat partial; QColor muted=foreground; muted.setAlphaF(.85); partial.setForeground(muted);
    QList<QTextLayout::FormatRange> formats;
    formats.append({0,int(text.size()),full});
    if(!provisional.isEmpty()) formats.append({int(text.size()-provisional.size()),int(provisional.size()),partial});
    layout.setFormats(formats);
    QList<QTextLine> laid;
    layout.beginLayout();
    while(true) { auto line=layout.createLine(); if(!line.isValid()) break; line.setLineWidth(width()-40); laid.append(line); }
    layout.endLayout();
    int start=std::max(0,int(laid.size())-lines); qreal y=16;
    for(int i=start;i<laid.size();++i) { laid[i].setPosition(QPointF(20,y)); y+=laid[i].height()*1.15; }
    painter.setClipRect(rect().adjusted(10,10,-10,-10));
    for(int i=start;i<laid.size();++i) laid[i].draw(&painter,QPointF(0,0));
}
Window::Window(QString path,QString storage,Output *o,Audio *a,Recognizer *r):root(std::move(path)),state(std::move(storage)),out(o),audio(a),recognizer(r),settings(state+"/settings.ini",QSettings::IniFormat) {
    setUiLanguage(settings.value("uiLanguageMode","auto").toString());
    selectedInput=settings.value("input","@system").toString();
    setWindowTitle("Captions Everywhere"); setObjectName("CaptionsEverywhere");
    setWindowFlag(Qt::WindowStaysOnTopHint,settings.value("top",true).toBool());
    setMinimumWidth(700); resize(860,370);
    setStyleSheet("QWidget{background:#0b101a;color:#e5edf9;font-family:'DejaVu Sans';font-size:12px} QComboBox,QSpinBox{background:#172235;border:1px solid #29374d;border-radius:6px;padding:6px} QPushButton,QToolButton{background:#172235;border:1px solid #29374d;border-radius:6px;padding:7px 12px} QPushButton:hover,QToolButton:hover{background:#243750} QToolButton:checked{background:#245448;border-color:#3b967d} QLabel#brand{font-size:16px;font-weight:600} QCheckBox{spacing:8px} QCheckBox::indicator{width:18px;height:18px} QDialog{background:#0b101a}");
    auto layout=new QVBoxLayout(this); layout->setContentsMargins(16,14,16,12);layout->setSpacing(12);
    auto head=new QHBoxLayout;
    auto logo=new QLabel;
    logo->setObjectName("appLogo");logo->setFixedSize(40,40);logo->setAccessibleName("Captions Everywhere");
    logo->setPixmap(QPixmap(root+"/resources/logo.png").scaled(36,36,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    head->addWidget(logo);
    auto brand=new QLabel("Captions Everywhere");brand->setObjectName("brand");head->addWidget(brand);head->addStretch();
    prefs=new QPushButton;head->addWidget(prefs);connect(prefs,&QPushButton::clicked,this,&Window::preferences);layout->addLayout(head);
    view=new CaptionView(this);
    view->fontSize=settings.value("fontSize",32).toInt();view->lines=settings.value("lines",3).toInt();view->opacity=settings.value("opacity",85).toInt();
    view->foreground=QColor(settings.value("foreground","#ffffff").toString());view->background=QColor(settings.value("background","#111827").toString());
    view->setMinimumHeight(std::max(110,int(view->fontSize*1.4*view->lines)+32)); layout->addWidget(view,1);
    auto controls=new QHBoxLayout;
    toggle=new QToolButton;toggle->setCheckable(true);toggle->setText("▶  Iniciar");controls->addWidget(toggle);
    inputs=new QComboBox;inputs->setMinimumWidth(200);inputs->addItem("", "@system");inputs->addItem("", "@mic");
    // Restore synchronously, before save(), model warmup or audio enumeration.
    int deviceIndex=inputs->findData(selectedInput);
    if(deviceIndex<0){inputs->addItem(selectedInput,selectedInput);deviceIndex=inputs->count()-1;}
    inputs->setCurrentIndex(deviceIndex);
    inputLabel=new QLabel;controls->addWidget(inputLabel);controls->addWidget(inputs,1);layout->addLayout(controls);
    auto languageRow=new QHBoxLayout;
    languageLabel=new QLabel;languageLabel->setStyleSheet("font-size:14px;font-weight:600;color:#f8fafc");languageRow->addWidget(languageLabel);
    languages=new QComboBox;languages->setMinimumWidth(155);languages->setStyleSheet("font-size:14px;border:1px solid #5b8ac4;padding:8px");
    languages->addItem("", "es");languages->addItem("", "en");languages->addItem("", "auto");
    languages->setCurrentIndex(std::max(0,languages->findData(settings.value("language","es"))));languageRow->addWidget(languages);
    automaticLanguage=new QLabel;automaticLanguage->setStyleSheet("color:#8bd5b6;font-size:14px;padding:8px");languageRow->addWidget(automaticLanguage);
    languageRow->addSpacing(18);modelLabel=new QLabel;languageRow->addWidget(modelLabel);
    models=new QComboBox;
    if(QFileInfo::exists(root+"/models/ggml-base-q5_1.bin"))models->addItem("Whisper · base","base");
    if(QFileInfo::exists(root+"/models/ggml-small-q5_1.bin"))models->addItem("Whisper · small","small");
    if(QFileInfo::exists(root+"/models/parakeet/encoder.int8.onnx"))models->addItem("Parakeet","parakeet");
    const bool upgraded=settings.value("recognitionVersion",1).toInt()<2;
    const auto chosenModel=settings.value("recognitionVersion",1).toInt()<5?QString("parakeet"):settings.value("model","parakeet").toString();
    int chosenIndex=models->findData(chosenModel);if(chosenIndex<0)chosenIndex=models->findData("parakeet");
    models->setCurrentIndex(std::max(0,chosenIndex));
    settings.setValue("recognitionVersion",5);
    const bool autoLanguage=models->currentData()=="parakeet";if(autoLanguage)languages->setCurrentIndex(2);languages->setVisible(!autoLanguage);automaticLanguage->setVisible(autoLanguage);
    languageRow->addWidget(models,1);layout->addLayout(languageRow);
    auto transcriptRow=new QHBoxLayout;
    record=new VisibleCheckBox;transcriptRow->addWidget(record);transcriptRow->addStretch();transcripts=new QPushButton;transcriptRow->addWidget(transcripts);layout->addLayout(transcriptRow);
    connect(record,&QCheckBox::toggled,this,[this](bool on){
        if(on){QString err;if(!out->startTranscript(&err)){record->setChecked(false);QMessageBox::warning(this,"Captions Everywhere",err);}}
        else out->stopTranscript();
    });
    connect(out,&Output::recordingChanged,this,[this](bool on){record->blockSignals(true);record->setChecked(on);record->blockSignals(false);record->setToolTip(on?out->transcriptPath():QString());});
    connect(transcripts,&QPushButton::clicked,this,[this]{
        const auto path=state+"/transcripts";
        if(!QDir().mkpath(path)){QMessageBox::warning(this,"Captions Everywhere",ui("No se puede abrir la carpeta de transcripciones."));return;}
        auto opener=openDesktopFolder(path,this);transcripts->setEnabled(false);
        auto failed=[this,opener]{
            if(opener->property("reported").toBool())return;opener->setProperty("reported",true);
            transcripts->setEnabled(true);QMessageBox::warning(this,"Captions Everywhere",ui("No se puede abrir la carpeta de transcripciones.")+"\n"+state+"/transcripts");
        };
        connect(opener,&QProcess::errorOccurred,this,[failed,opener](QProcess::ProcessError){failed();opener->deleteLater();});
        connect(opener,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,opener,failed](int code,QProcess::ExitStatus result){
            transcripts->setEnabled(true);if(code!=0||result!=QProcess::NormalExit)failed();opener->deleteLater();
        });
        opener->start();
    });
    auto footer=new QHBoxLayout;status=new QLabel;status->setWordWrap(true);status->setStyleSheet("color:#94a3b8");footer->addWidget(status,1);meter=new QLabel;meter->setStyleSheet("color:#7ca99b");footer->addWidget(meter);layout->addLayout(footer);
    url=new QLabel; url->setTextInteractionFlags(Qt::TextSelectableByMouse);url->setStyleSheet("color:#64748b;font-size:10px"); layout->addWidget(url);
    QString err; const int port=settings.value("port",8765).toInt();const bool listening=out->listen(quint16(port),&err);
    retranslate();if(!listening)status->setText(err);
    out->setStyle(style());
    connect(toggle,&QToolButton::toggled,this,&Window::startCapture);
    connect(languages,&QComboBox::currentIndexChanged,this,[this]{ reconfigure(); });
    connect(models,&QComboBox::currentIndexChanged,this,[this]{
        languages->blockSignals(true);
        const bool automatic=models->currentData()=="parakeet";
        languages->setVisible(!automatic);automaticLanguage->setVisible(automatic);
        if(automatic)languages->setCurrentIndex(2);else if(languages->currentData()=="auto")languages->setCurrentIndex(0);
        languages->blockSignals(false);reconfigure();
    });
    connect(inputs,&QComboBox::currentIndexChanged,this,[this]{ selectedInput=inputs->currentData().toString();reconfigure(); });
    connect(audio,&Audio::devices,this,[this](QStringList names,QStringList descriptions) {
        const QString selected=selectedInput;
        inputs->blockSignals(true);
        while(inputs->count()>2) inputs->removeItem(2);
        for(int i=0;i<names.size();++i) inputs->addItem(descriptions.value(i),names[i]);
        int found=inputs->findData(selected);
        // Keep a disconnected microphone selected, rather than silently capturing a different device.
        if(found<0) { inputs->addItem(ui("Desconectado · ")+selected,selected); found=inputs->count()-1; }
        inputs->setCurrentIndex(found);inputs->blockSignals(false);
    });
    auto message=[this](QString s,bool error) { if(enabled||error) {status->setText(s);status->setStyleSheet(error?"color:#fda4af":"color:#94a3b8");} };
    connect(audio,&Audio::status,this,message);
    connect(recognizer,&Recognizer::status,this,[this](QString s,bool error){status->setText(s);status->setStyleSheet(error?"color:#fda4af":"color:#94a3b8");});
    connect(recognizer,&Recognizer::ready,this,[this]{if(enabled)audio->select(true,inputs->currentData().toString());else status->setText(ui("Listo"));});
    connect(out,&Output::error,this,[this](QString s){status->setText(s);});
    connect(out,&Output::changed,this,[this](QString c,QString p){view->confirmed=c;view->provisional=p;view->update();});
    connect(recognizer,&Recognizer::metric,this,[this](qint64 ms,double seconds,int dropped) {
        meter->setText(QString::number(ms)+" ms · "+QString::number(seconds,'f',1)+" s audio");
        if(dropped)status->setText(ui("Audio omitido: el equipo no sigue el ritmo."));
    });
    if(settings.contains("geometry")) restoreGeometry(settings.value("geometry").toByteArray());
    if(upgraded)resize(860,std::max(height(),370));
    out->setHealth({{"running",false},{"model",models->currentData().toString()},{"language",languages->currentData().toString()}});
    recognizer->configure(false,models->currentData().toString(),languages->currentData().toString());save();
    if(auto screen=QGuiApplication::primaryScreen()) if(!screen->availableGeometry().intersects(frameGeometry()))move(screen->availableGeometry().topLeft()+QPoint(40,40));
}
QJsonObject Window::style()const {return {{"fontSize",view->fontSize},{"lines",view->lines},{"opacity",view->opacity},{"foreground",view->foreground.name()},{"background",view->background.name()}};}
void Window::retranslate() {
    prefs->setText(ui("Ajustes"));inputLabel->setText(ui("Entrada"));inputs->setAccessibleName(ui("Entrada"));
    inputs->setItemText(0,ui("Audio del sistema"));inputs->setItemText(1,ui("Micrófono predeterminado"));
    languageLabel->setText(ui("Idioma del audio"));languages->setAccessibleName(ui("Idioma del audio"));
    languages->setItemText(0,ui("Español"));languages->setItemText(1,ui("Inglés"));languages->setItemText(2,ui("Automático"));
    automaticLanguage->setText(ui("Detección automática"));modelLabel->setText(ui("Modelo"));models->setAccessibleName(ui("Modelo"));
    record->setText(ui("Guardar transcripción"));transcripts->setText(ui("Abrir transcripciones"));
    toggle->setText(enabled?ui("Ⅱ  Pausar"):ui("▶  Iniciar"));
    status->setText(enabled?ui("Escuchando"):ui("Pausado"));
    url->setText("OBS · http://127.0.0.1:"+QString::number(out->port())+"/obs");view->update();
}
void Window::selectDevice(QString name) { int index=inputs->findData(name);if(index<0){inputs->addItem(name,name);index=inputs->count()-1;}inputs->setCurrentIndex(index); }
void Window::startCapture(bool on) {
    enabled=on;toggle->blockSignals(true);toggle->setChecked(on);toggle->blockSignals(false);toggle->setText(on?ui("Ⅱ  Pausar"):ui("▶  Iniciar"));
    reconfigure();status->setText(on?ui("Preparando…"):ui("Pausado"));
}
void Window::reconfigure() {
    out->setHealth({{"running",enabled},{"model",models->currentData().toString()},{"language",languages->currentData().toString()},{"input",inputs->currentData().toString()}});
    out->clear();recognizer->configure(enabled,models->currentData().toString(),languages->currentData().toString());
    audio->select(false,inputs->currentData().toString());save();
}
void Window::save() {
    settings.setValue("language",languages->currentData());settings.setValue("model",models->currentData());settings.setValue("input",selectedInput);
    auto s=style();for(auto i=s.begin();i!=s.end();++i) settings.setValue(i.key(),i.value().toVariant());
    settings.setValue("geometry",saveGeometry());settings.sync();
}
void Window::preferences() {
    QDialog dialog(this);dialog.setWindowTitle(ui("Ajustes"));auto form=new QFormLayout(&dialog);
    QComboBox interfaceLanguage;interfaceLanguage.addItem(ui("Sistema (automático)"),"auto");interfaceLanguage.addItem("Español","es");interfaceLanguage.addItem("English","en");
    interfaceLanguage.setCurrentIndex(std::max(0,interfaceLanguage.findData(settings.value("uiLanguageMode","auto"))));form->addRow("Interfaz / Interface",&interfaceLanguage);
    QSpinBox size;size.setRange(16,72);size.setValue(view->fontSize);form->addRow(ui("Tamaño de letra"),&size);
    QSpinBox lines;lines.setRange(1,6);lines.setValue(view->lines);form->addRow(ui("Líneas visibles"),&lines);
    QSpinBox opacity;opacity.setRange(0,100);opacity.setSuffix(" %");opacity.setValue(view->opacity);form->addRow(ui("Opacidad"),&opacity);
    QColor fg=view->foreground,bg=view->background;
    QPushButton foreground(ui("Color del texto")),background(ui("Color del fondo"));form->addRow(&foreground);form->addRow(&background);
    connect(&foreground,&QPushButton::clicked,&dialog,[&]{auto c=QColorDialog::getColor(fg,&dialog);if(c.isValid())fg=c;});
    connect(&background,&QPushButton::clicked,&dialog,[&]{auto c=QColorDialog::getColor(bg,&dialog);if(c.isValid())bg=c;});
    VisibleCheckBox top(ui("Siempre visible"));top.setChecked(windowFlags().testFlag(Qt::WindowStaysOnTopHint));form->addRow(&top);
    auto note=new QLabel(ui("Wayland: consulta LEEME.md si necesitas una regla de ventana."));note->setWordWrap(true);form->addRow(note);
    QSpinBox port;port.setRange(1024,65534);port.setValue(settings.value("port",8765).toInt());form->addRow(ui("Puerto de OBS"),&port);
    QPushButton copy(ui("Copiar URL de OBS")),file(ui("Copiar ruta de texto"));form->addRow(&copy);form->addRow(&file);
    connect(&copy,&QPushButton::clicked,&dialog,[&]{QGuiApplication::clipboard()->setText("http://127.0.0.1:"+QString::number(port.value())+"/obs");});
    connect(&file,&QPushButton::clicked,&dialog,[&]{QGuiApplication::clipboard()->setText(state+"/captions.txt");});
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);buttons.button(QDialogButtonBox::Ok)->setText(ui("Aceptar"));buttons.button(QDialogButtonBox::Cancel)->setText(ui("Cancelar"));form->addRow(&buttons);
    connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    if(port.value()!=settings.value("port",8765).toInt()) {
        QString err;if(!out->listen(quint16(port.value()),&err)){QMessageBox::warning(this,"OBS",err);out->listen(quint16(settings.value("port",8765).toInt()),&err);return;}
        settings.setValue("port",port.value());
    }
    settings.setValue("uiLanguageMode",interfaceLanguage.currentData());settings.remove("uiLanguage");setUiLanguage(interfaceLanguage.currentData().toString());
    view->fontSize=size.value();view->lines=lines.value();view->opacity=opacity.value();view->foreground=fg;view->background=bg;
    view->setMinimumHeight(std::max(110,int(view->fontSize*1.4*view->lines)+32));view->update();
    if(top.isChecked()!=windowFlags().testFlag(Qt::WindowStaysOnTopHint)) { setWindowFlag(Qt::WindowStaysOnTopHint,top.isChecked());show(); }
    settings.setValue("top",top.isChecked());save();out->setStyle(style());
    retranslate();
}
void Window::closeEvent(QCloseEvent *event) {save();startCapture(false);hide();event->accept();}
