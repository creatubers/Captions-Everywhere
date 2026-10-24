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
#include <QDesktopServices>
#include <QUrl>
#include <QCloseEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QClipboard>
#include <QMessageBox>
#include <cmath>

void CaptionView::paintEvent(QPaintEvent*) {
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    QColor bg=background; bg.setAlphaF(opacity/100.0); painter.setBrush(bg); painter.setPen(Qt::NoPen); painter.drawRoundedRect(rect(),12,12);
    QFont font("DejaVu Sans"); font.setPixelSize(fontSize); font.setWeight(QFont::Medium);
    QString text=confirmed+(confirmed.isEmpty()||provisional.isEmpty()?"":" ")+provisional;
    if(text.isEmpty()) { font.setPixelSize(20); painter.setFont(font); painter.setPen(QColor("#94a3b8")); painter.drawText(rect().adjusted(20,15,-20,-15),Qt::AlignVCenter|Qt::TextWordWrap,"Los subtítulos aparecerán aquí"); return; }
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
    setWindowTitle("Captions Everywhere"); setObjectName("CaptionsEverywhere");
    setWindowFlag(Qt::WindowStaysOnTopHint,settings.value("top",true).toBool());
    setMinimumWidth(700); resize(860,370);
    setStyleSheet("QWidget{background:#0b101a;color:#e5edf9;font-family:'DejaVu Sans';font-size:12px} QComboBox,QSpinBox{background:#172235;border:1px solid #29374d;border-radius:6px;padding:6px} QPushButton,QToolButton{background:#172235;border:1px solid #29374d;border-radius:6px;padding:7px 12px} QPushButton:hover,QToolButton:hover{background:#243750} QToolButton:checked{background:#245448;border-color:#3b967d} QLabel#brand{font-size:16px;font-weight:600} QCheckBox{spacing:8px} QDialog{background:#0b101a}");
    auto layout=new QVBoxLayout(this); layout->setContentsMargins(16,14,16,12);layout->setSpacing(12);
    auto head=new QHBoxLayout;
    auto logo=new QLabel;
    logo->setObjectName("appLogo");logo->setFixedSize(40,40);logo->setAccessibleName("Logo de Captions Everywhere");
    logo->setPixmap(QPixmap(root+"/resources/logo.png").scaled(36,36,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    head->addWidget(logo);
    auto brand=new QLabel("Captions Everywhere");brand->setObjectName("brand");head->addWidget(brand);head->addStretch();
    auto local=new QLabel("LOCAL · SIN CONEXIÓN"); local->setStyleSheet("color:#7ca99b;font-size:10px;letter-spacing:1px");head->addWidget(local);
    auto prefs=new QPushButton("Ajustes");head->addWidget(prefs); connect(prefs,&QPushButton::clicked,this,&Window::preferences);layout->addLayout(head);
    view=new CaptionView(this);
    view->fontSize=settings.value("fontSize",32).toInt();view->lines=settings.value("lines",3).toInt();view->opacity=settings.value("opacity",85).toInt();
    view->foreground=QColor(settings.value("foreground","#ffffff").toString());view->background=QColor(settings.value("background","#111827").toString());
    view->setMinimumHeight(std::max(110,int(view->fontSize*1.4*view->lines)+32)); layout->addWidget(view,1);
    auto controls=new QHBoxLayout;
    toggle=new QToolButton;toggle->setCheckable(true);toggle->setText("▶  Iniciar");controls->addWidget(toggle);
    inputs=new QComboBox;inputs->setAccessibleName("Entrada de audio");inputs->setMinimumWidth(200);inputs->addItem("Audio del sistema","@system");inputs->addItem("Micrófono predeterminado","@mic");controls->addWidget(new QLabel("Entrada de audio"));controls->addWidget(inputs,1);
    layout->addLayout(controls);
    auto languageRow=new QHBoxLayout;
    auto languageLabel=new QLabel("Idioma del audio");languageLabel->setStyleSheet("font-size:14px;font-weight:600;color:#f8fafc");languageRow->addWidget(languageLabel);
    languages=new QComboBox;languages->setAccessibleName("Idioma del audio");languages->setMinimumWidth(155);languages->setStyleSheet("font-size:14px;border:1px solid #5b8ac4;padding:8px");languages->addItem("Español","es");languages->addItem("Inglés / English","en");languages->addItem("Automático (ES / EN)","auto");languages->setCurrentIndex(settings.value("language","es")=="en"?1:0);languageRow->addWidget(languages);
    automaticLanguage=new QLabel("Detección automática");automaticLanguage->setStyleSheet("color:#8bd5b6;font-size:14px;font-weight:600;padding:8px");languageRow->addWidget(automaticLanguage);languageRow->addSpacing(18);languageRow->addWidget(new QLabel("Reconocimiento"));
    models=new QComboBox;models->setAccessibleName("Calidad del reconocimiento");models->addItem("Rápido · base","base");models->addItem("Whisper · mayor precisión (más lento)","small");models->addItem("Precisión en directo · Parakeet","parakeet");
    const bool upgraded=settings.value("recognitionVersion",1).toInt()<2;
    const auto chosenModel=settings.value("model","parakeet").toString();models->setCurrentIndex(settings.value("recognitionVersion",1).toInt()<5 ? 2 : (chosenModel=="parakeet"?2:(chosenModel=="small"?1:0)));
    settings.setValue("recognitionVersion",5);
    const bool autoLanguage=models->currentData()=="parakeet";if(autoLanguage)languages->setCurrentIndex(2);languages->setVisible(!autoLanguage);automaticLanguage->setVisible(autoLanguage);
    languageRow->addWidget(models,1);layout->addLayout(languageRow);
    auto languageHint=new QLabel("Incluye español e inglés. Para fijar el idioma manualmente, selecciona un modelo Whisper.");
    languageHint->setWordWrap(true);languageHint->setStyleSheet("color:#a8bdd6;font-size:11px");layout->addWidget(languageHint);
    auto footer=new QHBoxLayout;status=new QLabel("Pausado · elige una entrada y pulsa Iniciar");status->setWordWrap(true);status->setStyleSheet("color:#94a3b8");footer->addWidget(status,1);meter=new QLabel;meter->setStyleSheet("color:#7ca99b");footer->addWidget(meter);layout->addLayout(footer);
    url=new QLabel; url->setTextInteractionFlags(Qt::TextSelectableByMouse);url->setStyleSheet("color:#64748b;font-size:10px"); layout->addWidget(url);
    QString err; const int port=settings.value("port",8765).toInt(); if(!out->listen(quint16(port),&err))status->setText(err);
    url->setText("OBS · http://127.0.0.1:"+QString::number(port)+"/obs  ·  Archivo de texto: copiar ruta en Ajustes");
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
    connect(inputs,&QComboBox::currentIndexChanged,this,[this]{ reconfigure(); });
    connect(audio,&Audio::devices,this,[this](QStringList names,QStringList descriptions) {
        QString selected=inputs->currentData().toString();
        if(inputs->count()==2 && selected=="@system") selected=settings.value("input","@system").toString();
        inputs->blockSignals(true);
        while(inputs->count()>2) inputs->removeItem(2);
        for(int i=0;i<names.size();++i) inputs->addItem(descriptions.value(i),names[i]);
        int found=inputs->findData(selected);
        // Keep a disconnected microphone selected, rather than silently capturing a different device.
        if(found<0) { inputs->addItem("Desconectado · "+selected,selected); found=inputs->count()-1; }
        inputs->setCurrentIndex(found);inputs->blockSignals(false);
    });
    auto message=[this](QString s,bool error) { if(enabled||error) {status->setText(s);status->setStyleSheet(error?"color:#fda4af":"color:#94a3b8");} };
    connect(audio,&Audio::status,this,message);
    connect(recognizer,&Recognizer::status,this,[this](QString s,bool error){status->setText(s);status->setStyleSheet(error?"color:#fda4af":"color:#94a3b8");});
    connect(recognizer,&Recognizer::ready,this,[this]{if(enabled)audio->select(true,inputs->currentData().toString());else status->setText("Modelo listo · selecciona la entrada y pulsa Iniciar");});
    connect(out,&Output::error,this,[this](QString s){status->setText(s);});
    connect(out,&Output::changed,this,[this](QString c,QString p){view->confirmed=c;view->provisional=p;view->update();});
    connect(recognizer,&Recognizer::metric,this,[this](qint64 ms,double seconds,int dropped) {
        meter->setText(QString::number(ms)+" ms · "+QString::number(seconds,'f',1)+" s audio");
        if(dropped)status->setText("El equipo va lento: se omitió audio antiguo para mantener el directo.");
    });
    if(settings.contains("geometry")) restoreGeometry(settings.value("geometry").toByteArray());
    if(upgraded)resize(860,std::max(height(),370));
    out->setHealth({{"running",false},{"model",models->currentData().toString()},{"language",languages->currentData().toString()}});
    recognizer->configure(false,models->currentData().toString(),languages->currentData().toString());save();
    if(auto screen=QGuiApplication::primaryScreen()) if(!screen->availableGeometry().intersects(frameGeometry()))move(screen->availableGeometry().topLeft()+QPoint(40,40));
}
QJsonObject Window::style()const {return {{"fontSize",view->fontSize},{"lines",view->lines},{"opacity",view->opacity},{"foreground",view->foreground.name()},{"background",view->background.name()}};}
void Window::selectDevice(QString name) { int index=inputs->findData(name);if(index<0){inputs->addItem(name,name);index=inputs->count()-1;}inputs->setCurrentIndex(index); }
void Window::startCapture(bool on) {
    enabled=on;toggle->blockSignals(true);toggle->setChecked(on);toggle->blockSignals(false);toggle->setText(on?"Ⅱ  Pausar":"▶  Iniciar");
    reconfigure();status->setText(on?"Preparando reconocimiento…":"Pausado");
}
void Window::reconfigure() {
    out->setHealth({{"running",enabled},{"model",models->currentData().toString()},{"language",languages->currentData().toString()},{"input",inputs->currentData().toString()}});
    out->clear();recognizer->configure(enabled,models->currentData().toString(),languages->currentData().toString());
    audio->select(false,inputs->currentData().toString());save();
}
void Window::save() {
    settings.setValue("language",languages->currentData());settings.setValue("model",models->currentData());settings.setValue("input",inputs->currentData());
    auto s=style();for(auto i=s.begin();i!=s.end();++i) settings.setValue(i.key(),i.value().toVariant());
    settings.setValue("geometry",saveGeometry());settings.sync();
}
void Window::preferences() {
    QDialog dialog(this);dialog.setWindowTitle("Ajustes · Captions Everywhere");auto form=new QFormLayout(&dialog);
    QSpinBox size;size.setRange(16,72);size.setValue(view->fontSize);form->addRow("Tamaño de letra",&size);
    QSpinBox lines;lines.setRange(1,6);lines.setValue(view->lines);form->addRow("Líneas visibles",&lines);
    QSpinBox opacity;opacity.setRange(0,100);opacity.setSuffix(" %");opacity.setValue(view->opacity);form->addRow("Opacidad del fondo",&opacity);
    QColor fg=view->foreground,bg=view->background;
    QPushButton foreground("Elegir color de texto"),background("Elegir color de fondo");form->addRow(&foreground);form->addRow(&background);
    connect(&foreground,&QPushButton::clicked,&dialog,[&]{auto c=QColorDialog::getColor(fg,&dialog);if(c.isValid())fg=c;});
    connect(&background,&QPushButton::clicked,&dialog,[&]{auto c=QColorDialog::getColor(bg,&dialog);if(c.isValid())bg=c;});
    QCheckBox top("Mantener encima de otras ventanas");top.setChecked(windowFlags().testFlag(Qt::WindowStaysOnTopHint));form->addRow(&top);
    auto note=new QLabel("Wayland puede requerir una regla del escritorio.\nConsulta LEEME.md para KDE y GNOME.");note->setWordWrap(true);form->addRow(note);
    QSpinBox port;port.setRange(1024,65534);port.setValue(settings.value("port",8765).toInt());form->addRow("Puerto de OBS",&port);
    QPushButton copy("Copiar URL para OBS"),file("Copiar ruta de captions.txt");form->addRow(&copy);form->addRow(&file);
    connect(&copy,&QPushButton::clicked,&dialog,[&]{QGuiApplication::clipboard()->setText("http://127.0.0.1:"+QString::number(port.value())+"/obs");});
    connect(&file,&QPushButton::clicked,&dialog,[&]{QGuiApplication::clipboard()->setText(state+"/captions.txt");});
    QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);form->addRow(&buttons);
    connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return;
    if(port.value()!=settings.value("port",8765).toInt()) {
        QString err;if(!out->listen(quint16(port.value()),&err)){QMessageBox::warning(this,"OBS",err);out->listen(quint16(settings.value("port",8765).toInt()),&err);return;}
        settings.setValue("port",port.value());
    }
    view->fontSize=size.value();view->lines=lines.value();view->opacity=opacity.value();view->foreground=fg;view->background=bg;
    view->setMinimumHeight(std::max(110,int(view->fontSize*1.4*view->lines)+32));view->update();
    if(top.isChecked()!=windowFlags().testFlag(Qt::WindowStaysOnTopHint)) { setWindowFlag(Qt::WindowStaysOnTopHint,top.isChecked());show(); }
    settings.setValue("top",top.isChecked());save();out->setStyle(style());
    url->setText("OBS · http://127.0.0.1:"+QString::number(port.value())+"/obs  ·  Archivo de texto: copiar ruta en Ajustes");
}
void Window::closeEvent(QCloseEvent *event) {save();startCapture(false);hide();event->accept();}
