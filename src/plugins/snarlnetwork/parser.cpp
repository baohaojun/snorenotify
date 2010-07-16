/****************************************************************************************
 * Copyright (c) 2010 Patrick von Reth <patrick.vonreth@gmail.com>                      *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#include "parser.h"
#include "snarlnetwork.h"

#include "core/snoreserver.h"
#include "core/notification.h"
#include "core/utils.h"


#include <QDir>
#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QEventLoop>
#include <QNetworkReply>
#include <QObject>
#include <QTcpSocket>

Parser::Parser(SnarlNetworkFrontend *snarl):snarl(snarl)
{
    setParent(snarl);
    getSnpType.insert("type",TYPE);
    getSnpType.insert("app",APP);
    getSnpType.insert("version",VERSION);
    getSnpType.insert("action",ACTION);
    getSnpType.insert("register",REGISTER);
    getSnpType.insert("add_class",ADD_CLASS);
    getSnpType.insert("notification",NOTIFICATION);
    getSnpType.insert("unregister",UNREGISTER);
    getSnpType.insert("class",CLASS);
    getSnpType.insert("title",TITLE);
    getSnpType.insert("text",TEXT);
    getSnpType.insert("icon",ICON);
    getSnpType.insert("timeout",TIMEOUT);
}


SnarlNotification Parser::parse(QString &msg,QTcpSocket* client){
    msg=msg.trimmed();

    SnarlNotification sNotification;
    sNotification.httpClient=false;
    sNotification.vailid=true;   
    sNotification.clientSocket=client;


    snpTypes action(ERROR);  
    if(msg.startsWith("GET ")){
        msg=msg.mid(msg.indexOf("/")+1);
        msg=msg.mid(0,msg.indexOf(" "));
        QByteArray dat(QByteArray::fromBase64(msg.toLatin1().data()));
        msg=QString(dat);
        qDebug()<<"Notification from a browser"<<msg;
        sNotification.httpClient=true;
    }

    QString app;
    QString title;
    QString text;
    QString sntpIcon;
    QString icon;
    QString alert;
    int timeout=10;

    QString key;
    QString value;
    QStringList splitted=msg.split("#?");
    foreach(QString s,splitted){
        key=s.mid(0,s.indexOf("=")).toLower();
        value=s.mid(s.indexOf("=")+1);
        switch(getSnpType.value(key)){
        case APP:
            app=value;
            break;
        case ACTION:
            action=getSnpType.value(value);
            sNotification.action=value;
            break;
        case  TITLE:
            title=value;
            break;
        case TEXT:
            text=value;
            break;
        case ICON:
            sntpIcon=value;
            icon=downloadIcon(value);
            break;
        case CLASS:
            alert=value;
        case TIMEOUT:
            timeout=value.toInt();
            break;
        default:
            break;
        }
    }

    sNotification.notification=QSharedPointer<Notification>(new Notification(snarl,title,text,icon,timeout));
    sNotification.notification->setIsNotification(false);
    sNotification.notification->insertHint("SnarlIcon",sntpIcon);


    switch(action){
    case NOTIFICATION:
        if(snarl->snore()->applicationListAlertIsActive(sNotification.notification->application(),sNotification.notification->alert()))
            break;
        sNotification.notification->setIsNotification(true);
        return sNotification;
        break;    
    case ADD_CLASS:
        if(sNotification.notification->alert().isEmpty()){
            qDebug()<<"Error registering alert with empty name";
            break;
        }
        if(title.isEmpty())
            title = alert;
        snarl->snore()->addAlert(sNotification.notification->application(),alert,title);
        break;
    case REGISTER:
        qDebug()<<snarl->snore()->aplicationList().keys();
        if(!snarl->snore()->aplicationList().contains(sNotification.notification->application())&&!sNotification.notification->application().isEmpty()){
            snarl->snore()->addApplication(QSharedPointer<Application>(new Application(sNotification.notification->application())));
        }
        else
            qDebug()<<sNotification.notification->application()<<"already registred";
        break;
    case UNREGISTER:
        snarl->snore()->removeApplication(sNotification.notification->application());
        break;
    case ERROR:
    default:
        sNotification.vailid=false;
        break;
    }
    qDebug()<<Utils::notificationToSNTPString(sNotification.notification);
    sNotification.notification->insertHint("SNaction",sNotification.action);
    return sNotification;
}

QString Parser::downloadIcon(const QString &address){
    if(address=="")
        return "";
    if(address.startsWith("file://"))
        return QString(address.mid(7));
    QByteArray arr=address.toUtf8();
    QUrl url=QUrl::fromEncoded(arr);

    QCryptographicHash hash(QCryptographicHash::Md5);
    hash.addData(arr);
    QString filename=QDir::temp().path()+"/SnoreNotify/"+hash.result().toHex()+address.mid(address.lastIndexOf(".")-1);
    QFile file(filename);
    if(file.exists())
        return filename;

    QByteArray reply=download(url);

    file.open(QIODevice::WriteOnly);
    file.write(reply);

    return filename;

}

QByteArray Parser::download(const QUrl &address){
    QNetworkAccessManager manager;
    QEventLoop loop;
    QNetworkRequest request(address);
    request.setRawHeader("User-Agent", "SnoreNotify");
    QNetworkReply *reply=manager.get(request);
    QObject::connect(reply, SIGNAL(finished()), &loop, SLOT(quit()));
    loop.exec();
    return reply->readAll();
}

#include "parser.moc"
