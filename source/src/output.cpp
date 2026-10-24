#include "output.h"
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QJsonDocument>
#include <QTimer>
#include <QTcpSocket>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <memory>
#include <algorithm>

static QByteArray frame(const QByteArray &payload) {
    QByteArray out; out.append(char(0x81));
    if(payload.size()<126) out.append(char(payload.size()));
    else { out.append(char(126)); out.append(char((payload.size()>>8)&255)); out.append(char(payload.size()&255)); }
    return out+payload;
}
Output::Output(QString path,QString storage,QObject *p):QObject(p),root(std::move(path)),state(std::move(storage)) {
    last.start();
    connect(&http,&QTcpServer::newConnection,this,&Output::acceptHttp);
    auto timer=new QTimer(this); timer->start(1000);
    connect(timer,&QTimer::timeout,this,[this] { if(last.elapsed()>8000 && (!history.isEmpty() || !stable.previous.isEmpty())) clear(); });
}
bool Output::listen(quint16 port,QString *err) {
    for(auto peer:peers) peer->disconnectFromHost();
    http.close();
    if(port<1024 || port>65534) { *err="El puerto debe estar entre 1024 y 65534."; return false; }
    if(!http.listen(QHostAddress::LocalHost,port)) { *err="Puerto "+QString::number(port)+" ocupado. Elige otro en Ajustes."; return false; }
    return true;
}
QJsonObject Output::snapshot() const {
    QString c=(history+" "+stable.confirmed).simplified();
    QStringList wrapped; QString line;
    const auto words=(c+" "+stable.provisional).simplified().split(' ',Qt::SkipEmptyParts);
    for(const auto &word:words) {
        if(!line.isEmpty() && line.size()+word.size()+1>52) {wrapped<<line;line.clear();}
        if(!line.isEmpty())line+=' ';line+=word;
    }
    if(!line.isEmpty())wrapped<<line;
    const int lines=std::clamp(style.value("lines").toInt(3),1,6);
    const QString fileText=wrapped.mid(std::max(0,int(wrapped.size())-lines)).join('\n');
    return {{"confirmed",c},{"provisional",stable.provisional},{"text",fileText},{"style",style},{"sequence",sequence}};
}
void Output::update(QString text,bool final) {
    if(!final || text.simplified()!=stable.previous)last.restart();
    // Remove overlap at the boundary of consecutive finalized chunks only.
    auto h=history.split(' ',Qt::SkipEmptyParts), n=text.simplified().split(' ',Qt::SkipEmptyParts);
    int overlap=0;
    for(int k=std::min(6,std::min(int(h.size()),int(n.size())));k>=2;--k) {
        if(h.mid(h.size()-k).join(' ').compare(n.mid(0,k).join(' '),Qt::CaseInsensitive)==0) { overlap=k; break; }
    }
    text=n.mid(overlap).join(' ');
    stable.update(text,final);
    if(final) { history=(history+" "+stable.confirmed).simplified(); if(history.size()>500) history=history.right(500).section(' ',1); stable.clear(); }
    publish();
}
void Output::clear() { history.clear(); stable.clear(); publish(); }
void Output::setHealth(QJsonObject delta) {for(auto it=delta.begin();it!=delta.end();++it)health.insert(it.key(),it.value());}
void Output::setStyle(QJsonObject s) { style=std::move(s); publish(); }
void Output::publish() {
    ++sequence; auto data=snapshot();
    QSaveFile file(state+"/captions.txt");
    if(!file.open(QIODevice::WriteOnly) || file.write(data["text"].toString().toUtf8())<0 || !file.commit()) {
        if(writeError.isEmpty()) { writeError="No se puede escribir el archivo de subtítulos en la carpeta de datos. Comprueba sus permisos."; emit error(writeError); }
    } else writeError.clear();
    const auto json=QString::fromUtf8(QJsonDocument(data).toJson(QJsonDocument::Compact));
    for(auto peer:peers) if(peer->bytesToWrite()<64*1024) peer->write(frame(json.toUtf8()));
    emit changed(data["confirmed"].toString(),data["provisional"].toString());
}
void Output::acceptHttp() {
    while(http.hasPendingConnections()) {
        auto socket=http.nextPendingConnection();
        socket->setParent(this);
        auto buffer=std::make_shared<QByteArray>();
        connect(socket,&QTcpSocket::readyRead,this,[this,socket,buffer] {
            if(socket->property("websocket").toBool()) {
                QByteArray incoming=socket->readAll();
                if(!incoming.isEmpty() && (uchar(incoming[0])&15)==8) { socket->write(QByteArray::fromHex("8800")); socket->disconnectFromHost(); }
                return;
            }
            *buffer+=socket->readAll();
            if(buffer->size()>8192) { socket->disconnectFromHost(); return; }
            if(!buffer->contains("\r\n\r\n")) return;
            const auto first=buffer->split('\n').value(0).trimmed().split(' ');
            const auto path=first.value(1).split('?').value(0);
            if(first.value(0)=="GET" && path=="/ws") {
                QByteArray key,origin;
                for(auto line:buffer->split('\n')) {
                    if(line.toLower().startsWith("sec-websocket-key:")) key=line.mid(18).trimmed();
                    if(line.toLower().startsWith("origin:")) origin=line.mid(7).trimmed();
                }
                const QByteArray allowed="http://127.0.0.1:"+QByteArray::number(http.serverPort());
                if(key.isEmpty() || peers.size()>=16 || (!origin.isEmpty() && origin!=allowed && origin!="null")) {socket->disconnectFromHost();return;}
                auto accept=QCryptographicHash::hash(key+"258EAFA5-E914-47DA-95CA-C5AB0DC85B11",QCryptographicHash::Sha1).toBase64();
                socket->write("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: "+accept+"\r\n\r\n");
                socket->setProperty("websocket",true);peers<<socket;
                socket->write(frame(QJsonDocument(snapshot()).toJson(QJsonDocument::Compact)));
                connect(socket,&QTcpSocket::disconnected,this,[this,socket]{peers.removeAll(socket);});
                return;
            }
            QByteArray body,type="text/html; charset=utf-8",status="200 OK";
            if(first.value(0)!="GET") { status="405 Method Not Allowed"; body="GET only"; }
            else if(path=="/obs" || path=="/" ) {
                QFile f(root+"/resources/obs.html");
                if(f.open(QIODevice::ReadOnly)) { body=f.readAll(); body.replace("__WS_PORT__",QByteArray::number(http.serverPort())); }
                else { status="500 Internal Server Error"; body="Falta resources/obs.html"; }
            } else if(path=="/font.ttf" || path=="/logo.png") {
                QFile f(root+(path=="/logo.png"?"/resources/logo.png":"/resources/DejaVuSans.ttf"));
                if(f.open(QIODevice::ReadOnly)){body=f.readAll();type=path=="/logo.png"?"image/png":"font/ttf";}else{status="404 Not Found";body="Font missing";}
            } else if(path=="/health") {body=QJsonDocument(health).toJson(QJsonDocument::Compact);type="application/json; charset=utf-8";} else if(path=="/state") { body=QJsonDocument(snapshot()).toJson(QJsonDocument::Compact); type="application/json; charset=utf-8"; }
            else { status="404 Not Found"; body="Not found"; }
            socket->write("HTTP/1.1 "+status+"\r\nContent-Type: "+type+"\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);
            socket->disconnectFromHost();
        });
        connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
        QTimer::singleShot(3000,socket,[socket] { if(!socket->property("websocket").toBool()) socket->disconnectFromHost(); });
    }
}
