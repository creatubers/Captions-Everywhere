#pragma once
#include <QWidget>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSettings>
#include <QJsonObject>
#include "audio.h"
#include "recognizer.h"
#include "output.h"

class VisibleCheckBox : public QCheckBox {
public:
    using QCheckBox::QCheckBox;
protected:
    void paintEvent(QPaintEvent *event) override;
};
class CaptionView : public QWidget {
public:
    QString confirmed,provisional;
    int fontSize=32,lines=3,opacity=85;
    QColor foreground=Qt::white,background=QColor("#111827");
    explicit CaptionView(QWidget *p=nullptr):QWidget(p){setMinimumHeight(110);setMinimumWidth(440);}
    void paintEvent(QPaintEvent*) override;
};
class Window : public QWidget {
    Q_OBJECT
public:
    Window(QString root,QString state,Output *out,Audio *audio,Recognizer *recognizer);
    void startCapture(bool enabled);
    void selectDevice(QString name);
    QJsonObject style() const;
    bool running()const{return enabled;}
private:
    QString root,state;
    Output *out;
    Audio *audio;
    Recognizer *recognizer;
    QSettings settings;
    CaptionView *view;
    QLabel *status,*meter,*url,*automaticLanguage;
    QToolButton *toggle;
    QPushButton *prefs,*transcripts;
    QCheckBox *record;
    QLabel *inputLabel,*languageLabel,*modelLabel;
    QString selectedInput;
    void retranslate();
    QComboBox *inputs,*languages,*models;
    bool enabled=false;
    void save();
    void preferences();
    void reconfigure();
    void closeEvent(QCloseEvent*) override;
};
