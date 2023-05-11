/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Device Utilities module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/
#include "qnetworksettingsservice.h"
#include "qnetworksettingsmanager_p.h"
#include "qnetworksettingsinterface_p.h"
#include "qnetworksettingsservicemodel.h"
#include "qnetworksettingsuseragent.h"
#include <QJniObject>
#include <QObject>
#include <QCoreApplication>
#include <QtCore/private/qandroidextras_p.h>
#include "qnetworksettingsservice_p.h"

QT_BEGIN_NAMESPACE

QNetworkSettingsManagerPrivate::QNetworkSettingsManagerPrivate(QNetworkSettingsManager *parent)
    :QObject(parent)
    ,q_ptr(parent)
    , m_interfaceModel(nullptr)
    , m_serviceFilter(nullptr)
    , m_context()
    , m_agent(nullptr)
    , m_currentWifiConnection(nullptr)
    , m_currentWiredConnection(nullptr)
    , m_initialized(false)
{

    m_serviceModel = new QNetworkSettingsServiceModel(this);
    m_serviceFilter = new QNetworkSettingsServiceFilter(this);
    m_serviceFilter->setSourceModel(m_serviceModel);

    if(!initialize()){
        qWarning("Failed to initialize the context");
    }
    else{

        m_wifimanager = m_context.callObjectMethod("getSystemService",
                                                   "(Ljava/lang/String;)Ljava/lang/Object;",
                                                   QJniObject::fromString("wifi").object<jstring>());
        m_connectivitymanager = m_context.callObjectMethod("getSystemService",
                                                           "(Ljava/lang/String;)Ljava/lang/Object;",
                                                           QJniObject::fromString("connectivity").object<jstring>());

        bool wifiOn = m_wifimanager.callMethod<jboolean>("isWifiEnabled","()Z");

        /*if(wifiOn){
            qWarning() << "Wifi attivo";
        }
        else{
            qWarning() << "Wifi spento"; //richiesta di attivare il wifi (aggiungere) aggiornamento: wifienabled deprecato dal livello 29
        }*/

        foreach (QNetworkSettingsService *s, m_serviceModel->getModel()) {
            //qWarning() << "servizio: " << s->id();
        }

        //-----------------------------------------------------
        /*
        QJniObject aNet = m_connectivitymanager.callObjectMethod("getActiveNetwork","()Landroid/net/Network;");
        if(aNet!=NULL) qDebug() << "almeno esisto";
        QJniObject capabilities = m_connectivitymanager.callObjectMethod("getNetworkCapabilities",
                                                                         "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;",
                                                                         aNet.object<jobject>());
        QJniObject transport = capabilities.callObjectMethod("getTransportInfo","()Landroid/net/TransportInfo;");
        if(transport!=NULL) qDebug() << "esiste il transport";
        qDebug() << transport.callObjectMethod("getSSID","()Ljava/lang/String;").toString();

        QJniEnvironment env;
        jclass wifiinfo = env.findClass("android/net/wifi/WifiInfo");
        if(env->IsInstanceOf(transport.object<jobject>(),wifiinfo)){
            qWarning() << "Sono un wifiinfo";
        }

        QJniObject wi = m_wifimanager.callObjectMethod("getConnectionInfo",
                                                       "()Landroid/net/wifi/WifiInfo;");

        QString ssid = wi.callObjectMethod("getSSID","()Ljava/lang/String;").toString();
        qDebug() << "Questo invece? " << ssid;
        */
        //-----------------------------------------------------
        modifiche = false;
        startTimer(2000);
    }
}
void QNetworkSettingsManagerPrivate::timerEvent(QTimerEvent *event)
{
    QJniEnvironment env;
    QNetworkSettingsIPv4 *ipv4 = new QNetworkSettingsIPv4();
    QNetworkSettingsIPv6 *ipv6 = new QNetworkSettingsIPv6();
    QNetworkSettingsType *type = new QNetworkSettingsType();
    QNetworkSettingsWireless *wireless = new QNetworkSettingsWireless();
    QString gateway;
    Q_Q(QNetworkSettingsManager);

    updateServices();

    bool wifiOn = m_wifimanager.callMethod<jboolean>("isWifiEnabled","()Z");
    if(wifiOn){
        QJniObject activeNetwork = m_connectivitymanager.callObjectMethod("getActiveNetwork",
                                                                          "()Landroid/net/Network;");

        if(activeNetwork==NULL){ //togliere gli indirizzi dalla lista, fare uno scan e poi se non è più presente nello scan allora bisogna rimuovere l entry dalla lista
            if(!m_currentSsid.isEmpty()){
                if(m_serviceModel->getByName(m_currentSsid)->type() == QNetworkSettingsType::Wifi){
                    m_currentWifiConnection.clear();
                    emit q->currentWifiConnectionChanged();
                }
                else if(m_serviceModel->getByName(m_currentSsid)->type() == QNetworkSettingsType::Wired){
                    m_currentWiredConnection.clear();
                    emit q->currentWiredConnectionChanged();
                }
                clearConnectionState();//invece di fare una ricerca lo tolgo direttamente dalla lista poi faccio uno scan e se sarà ancora presente verrà reinserito
            }
        }
        else{ //salvo l ssid in modo da rendere più facile e veloce l aggiornamento

            QJniObject networkCapabilities = m_connectivitymanager.callObjectMethod("getNetworkCapabilities",
                                                                                    "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;",
                                                                                    activeNetwork.object<jobject>());

            if(networkCapabilities.callMethod<jboolean>("hasTransport","(I)Z",1)){//if the network has transport for wifi than ill check the ssid

                QJniObject wifiInfo = m_wifimanager.callObjectMethod("getConnectionInfo",
                                                                     "()Landroid/net/wifi/WifiInfo;");

                QString ssid = wifiInfo.callObjectMethod("getSSID","()Ljava/lang/String;").toString();

                ssid = formattedSSID(ssid);

                if(!m_currentSsid.isEmpty() && m_currentSsid != ssid){ //se è vuoto allora non c'è bisogno di andare a cancellare gli ssid
                    clearConnectionState(); //perchè anche se poi pulisce l ssid lo salvo dopo
                }
                m_currentSsid = ssid;

                //aggiornare gli indirizzi dopo che ho trovato una connessione
                QNetworkSettingsService *service = m_serviceModel->getByName(m_currentSsid); //mi ritorna l item di tipo service con quell ssid

                //essendo in questo ramo sono sicuro di avere una connessione di tipo wifi
                type->setType(QNetworkSettingsType::Wifi);
                service->propertyCall("Type",QVariant::fromValue(type));

                /*if(service != nullptr){
                        qWarning() << "Not empty object";
                    }
                    else if(service == nullptr){
                        qWarning() << "Empty object";
                    }
                    else{
                        qWarning() << "CLOWNFIESTA";
                    }*/

                wirelessConfig(wifiInfo,networkCapabilities,service);

                QJniObject linkProperties = m_connectivitymanager.callObjectMethod("getLinkProperties",
                                                                                   "(Landroid/net/Network;)Landroid/net/LinkProperties;",
                                                                                   activeNetwork.object<jobject>());

                QJniObject proxyInfo = linkProperties.callObjectMethod("getHttpProxy",
                                                                       "()Landroid/net/ProxyInfo;");

                //if(proxyInfo != NULL){
                salvoProxy(proxyInfo,service);
                //}
                //else{ //se al secondo controllo non trovo più un proxy devo fare un clear di tutte le impostazioni che avevo salvato
                //    qDebug() << "Proxy non impostato";
                //}

                QJniObject domain = linkProperties.callObjectMethod("getDnsServers","()Ljava/util/List;"); //getDnsServers cannot return a null value
                salvoDNS(domain,service);

                QJniObject linkAddresses = linkProperties.callObjectMethod("getLinkAddresses",
                                                                           "()Ljava/util/List;"); //lista degli address

                int size = linkAddresses.callMethod<jint>("size","()I");

                for(int i=0;i<size;i++){
                    QJniObject linkAddress = linkAddresses.callObjectMethod("get","(I)Ljava/lang/Object;",i);
                    QJniObject address = linkAddress.callObjectMethod("getAddress","()Ljava/net/InetAddress;");

                    jclass inet4address = env.findClass("java/net/Inet4Address");
                    jclass inet6address = env.findClass("java/net/Inet6Address");

                    if(env->IsInstanceOf(address.object<jobject>(),inet4address) && (ipv4->address().isEmpty())){ //controllo che l ip sia un istanza di ipv4

                        QString indirizzo = address.callObjectMethod("getHostAddress",
                                                                     "()Ljava/lang/String;").toString();
                        ipv4->setAddress(indirizzo);

                        int prefixLength = linkAddress.callMethod<jint>("getPrefixLength","()I");

                        ipv4->setMask(QString::number(prefixLength)); //setto la maschera

                        QJniObject dhcp = linkProperties.callObjectMethod("getDhcpServerAddress",
                                                                          "()Ljava/net/Inet4Address;");
                        if(dhcp != NULL){
                            ipv4->setMethod(QNetworkSettingsIPv4::Method::Dhcp);
                        }
                        else{//devo fare qualcosa nel caso in cui non ci siano sk
                            ipv4->setMethod(QNetworkSettingsIPv4::Method::Manual);
                        }

                        gateway = salvoGateway(linkProperties,"IPv4");

                        QVariant variant = QVariant::fromValue(ipv4);
                        if(updateProperties(variant,service)){
                            service->propertyCall("IPv4",variant); //chiamata per la modifica delle piority
                            modifiche = true;
                        }
                    }
                    else if(env->IsInstanceOf(address.object<jobject>(),inet6address) && (ipv6->address().isEmpty())){ //controllo che l ip sia un istanza di ipv6

                        ipv6->setAddress(address.callObjectMethod("getHostAddress",
                                                                  "()Ljava/lang/String;").toString());

                        gateway = salvoGateway(linkProperties,"IPv6");
                        ipv6->setGateway(gateway);

                        if(!address.toString().isEmpty()){
                            ipv6->setMethod(QNetworkSettingsIPv6::Method::Auto);
                        }
                        else{
                            ipv6->setMethod(QNetworkSettingsIPv6::Method::Manual);
                        }
                        ipv6->setPrefixLength(linkAddress.callMethod<jint>("getPrefixLength","()I"));

                        QVariant variant = QVariant::fromValue(ipv6);
                        if(updateProperties(variant,service)){
                            service->propertyCall("IPv6",variant); //salvo le proprietà nel servizio
                            modifiche = true;
                        }
                    }
                }

                if(modifiche){
                    setCurrentWifiConnection(service);
                    /*foreach (QNetworkSettingsService *s, m_serviceModel->getModel()) {
                        qWarning() << "servizio dopo aggiornamento: "
                                   << "proxy: " << s->proxy()->url().toString()
                                   << s->id() << " ipv4: " << s->ipv4()->address()
                                   << " mask: " << s->ipv4()->mask()
                                   << " dns count: " << s->domains()->rowCount()
                                   << " count colonne: " << s->domains()->columnCount()
                                   << " ipv6: " << s->ipv6()->address()
                                   << " gateway: " << s->ipv6()->gateway();
                        for(int u=0;u<s->domains()->rowCount();u++){
                            for(int o=0;o<s->domains()->columnCount();o++){
                                QModelIndex nonSo = s->domains()->index(u,o);
                                QStringList lis = nonSo.data().toStringList();
                                for(const QString &item : lis){
                                    qDebug() << item;
                                }
                            }
                        }
                    }*/
                    emit q->currentWifiConnectionChanged();
                    modifiche = false;
                }
            }
            else{
                qWarning() << "Network does not have transport for wifi"; //quindi può essere Wired
            }
        }
    }
    else{
        //qWarning() << "Wifi spento"; //richiesta di attivare il wifi (aggiungere) aggiornamento: wifienabled deprecato dal livello 29
        //se il wifi è spento controllo di che tipo fosse la connessione salvata
        if(!m_currentSsid.isEmpty()){
            QNetworkSettingsService *service = m_serviceModel->getByName(m_currentSsid);
            if(service != nullptr){
                if(service->type() == QNetworkSettingsType::Wifi){ //wifi spento e
                    clearConnectionState();
                }
            }
        }
    }
    /*QNetworkSettingsService *ser = m_serviceModel->getByName(m_currentSsid);
    QNetworkSettingsIPv4 *ip = ser->ipv4();
    if(ip != nullptr){
        qDebug() << "ip trovato" << ip->address();
    }
    else{
        qWarning() << "ip non trovato";
    }*/
}

bool QNetworkSettingsManagerPrivate::updateProperties(QVariant mod,QNetworkSettingsService *service){
    if(mod.canConvert<QNetworkSettingsIPv4 *>()){
        if(service->ipv4()->address() != mod.value<QNetworkSettingsIPv4 *>()->address()){
            return true;
        }
        if(service->ipv4()->mask() != QNetworkSettingsServicePrivate::ensureMask(mod.value<QNetworkSettingsIPv4 *>()->mask().toInt())){
            return true;
        }
        if(service->ipv4()->method() != mod.value<QNetworkSettingsIPv4 *>()->method()){
            return true;
        }
        if(service->ipv4()->gateway() != mod.value<QNetworkSettingsIPv4 *>()->gateway()){
            return true;
        }
    }
    if(mod.canConvert<QNetworkSettingsIPv6 *>()){
        if(service->ipv6()->address() != mod.value<QNetworkSettingsIPv6 *>()->address()){
            return true;
        }
        if(service->ipv6()->prefixLength() != mod.value<QNetworkSettingsIPv6 *>()->prefixLength()){
            return true;
        }
        if(service->ipv6()->method() != mod.value<QNetworkSettingsIPv6 *>()->method()){
            return true;
        }
        if(service->ipv6()->gateway() != mod.value<QNetworkSettingsIPv6 *>()->gateway()){
            return true;
        }
    }
    return false;
}

void QNetworkSettingsManagerPrivate::wirelessConfig(QJniObject wifiInfo,QJniObject networkCapabilities,QNetworkSettingsService *service){
    int signalStrength = networkCapabilities.callMethod<jint>("getSignalStrength","()I");
    int securityType = wifiInfo.callMethod<jint>("getCurrentSecurityType","()I");
    bool isOutOfRange = false;
    service->propertyCall("Strength",signalStrength);
    service->propertyCall("Security",securityType);
}

QString QNetworkSettingsManagerPrivate::salvoGateway(QJniObject linkProperties,QString key){
    QString gateway;
    QJniEnvironment env;
    QJniObject routes = linkProperties.callObjectMethod("getRoutes","()Ljava/util/List;");
    jclass inet4address = env.findClass("java/net/Inet4Address");
    jclass inet6address = env.findClass("java/net/Inet6Address");

    if(routes!=NULL){
        int size = routes.callMethod<jint>("size","()I");
        for(int i=0;i<size;i++){
            QJniObject route = routes.callObjectMethod("get","(I)Ljava/lang/Object;",i);
            gateway = route.callObjectMethod("getGateway","()Ljava/net/InetAddress;")
                          .callObjectMethod("getHostAddress","()Ljava/lang/String;").toString();
            if(route.callMethod<jboolean>("isDefaultRoute","()Z")){
                if(env->IsInstanceOf(route.callObjectMethod("getGateway",
                                                             "()Ljava/net/InetAddress;").object<jobject>(),
                                      inet4address) && (key=="IPv4")){
                    return gateway;
                }
                else if(env->IsInstanceOf(route.callObjectMethod("getGateway",
                                                                  "()Ljava/net/InetAddress;").object<jobject>(),
                                           inet6address) && (key=="IPv6")){
                    return gateway;
                }
            }
        }
    }
    return NULL; //è possibile non riuscire a trovare neanche un gateway?
}

void QNetworkSettingsManagerPrivate::salvoProxy(QJniObject proxyInfo,QNetworkSettingsService *service){
    QNetworkSettingsProxy *proxy = new QNetworkSettingsProxy();
    QJniEnvironment env;
    QStringList lista;
    //qDebug() << "PAC Url " << proxyInfo.callObjectMethod("getPacFileUrl","()Landroid/net/Uri;").toString();
    if(proxyInfo == NULL){
        //qDebug() << "valore in service: " << service->proxy()->url().toString() << "cos: " << service->proxy()->url().isEmpty();
        if(!service->proxy()->url().isEmpty()){
            service->propertyCall("Proxy",QVariant::fromValue(proxy));
            modifiche = true;
        }
    }
    else{
        proxy->setUrl(proxyInfo.callObjectMethod("getPacFileUrl","()Landroid/net/Uri;").toString());
        QJniObject exclusionList = proxyInfo.callObjectMethod("getExclusionList",
                                                              "()[Ljava/lang/String;");
        int size = env->GetArrayLength(exclusionList.object<jobjectArray>());
        for(int i=0;i<size;i++){
            jstring str = (jstring) env->GetObjectArrayElement(exclusionList.object<jobjectArray>(),i);
            const char* utfString = env->GetStringUTFChars(str ,0);
            QString convertedString = QString::fromUtf8(utfString);
            lista.append(convertedString);
        }
        proxy->setExcludes(lista);
        if(service->proxy()->url() != proxy->url()){
            modifiche = true;
            service->proxy()->setUrl(proxy->url());
        }
        QStringList oldList = service->proxy()->excludes()->index(0,0).data().toStringList();
        if(oldList.isEmpty() && !lista.isEmpty()){
            modifiche = true;
            service->propertyCall("Proxy",QVariant::fromValue(proxy));
        }
        else{
            foreach (const QString &str, lista) {
                if(!oldList.contains(str)){
                    modifiche = true;
                    qDebug() << "Aggiorno proxy";
                    service->propertyCall("Proxy",QVariant::fromValue(proxy));
                    break;
                }
            }
        }
    }
}

void QNetworkSettingsManagerPrivate::salvoDNS(QJniObject domain,QNetworkSettingsService *service){
    int totDNS = domain.callMethod<jint>("size","()I");
    QStringList listaIndirizzi;
    QStringList listaNomi;
    for(int y = 0; y < totDNS; y++){
        QJniObject dnsIndirizzo = domain.callObjectMethod("get","(I)Ljava/lang/Object;",y);
        //qDebug() << "dns: " << dnsIndirizzo.callObjectMethod("getHostAddress","()Ljava/lang/String;").toString();
        listaIndirizzi.append(dnsIndirizzo.callObjectMethod("getHostAddress","()Ljava/lang/String;").toString());
        listaNomi.append(dnsIndirizzo.callObjectMethod("getHostName","()Ljava/lang/String;").toString());
    }
    QStringList listaOldIndirizzi = service->domains()->index(0,0).data().toStringList();
    QStringList listaOldNomi = service->nameservers()->index(0,0).data().toStringList();
    if(listaOldIndirizzi.isEmpty() && !listaIndirizzi.isEmpty() && !listaNomi.isEmpty() && listaOldNomi.isEmpty()){
        modifiche = true;
        service->propertyCall("Domains",listaIndirizzi);
        service->propertyCall("Nameservers",listaNomi);
    }
    else{
        foreach(const QString &item,listaIndirizzi){
            if(!listaOldIndirizzi.contains(item)){
                modifiche = true;
                qDebug() << "Modifica dei DNS";
                service->propertyCall("Domains",listaIndirizzi);
                break;
            }
        }
        foreach(const QString &str,listaNomi){
            if(!listaOldNomi.contains(str)){
                modifiche = true;
                service->propertyCall("Nameservers",listaNomi);
                break;
            }
        }
    }
}

QString QNetworkSettingsManagerPrivate::formattedSSID(QString ssid){

    if (ssid.front() == '"' && ssid.back() == '"') {
        ssid = ssid.mid(1,ssid.length() - 2);
    }
    return ssid;
}

void QNetworkSettingsManagerPrivate::updateServices() //per riempire la lista con gli ssid conosciuti
{
    bool trovato;
    QJniObject scanResult;
    QString wifiSsid;
    Q_Q(QNetworkSettingsManager);

    QJniObject scanResults = m_wifimanager.callObjectMethod("getScanResults","()Ljava/util/List;");

    if(!m_wifimanager.callMethod<jboolean>("isWifiEnabled","()Z")){
        QList<QNetworkSettingsService*> listaServices = m_serviceModel->getModel();
        for(int i=0;i<listaServices.size();i++){
            QNetworkSettingsService *s = listaServices.at(i);
            if(s->type() == QNetworkSettingsType::Wifi){
                m_serviceModel->removeService(s->id());
                if(s->name() == m_currentSsid){
                    clearConnectionState();
                    m_currentWifiConnection.clear();
                    emit q->currentWifiConnectionChanged();
                }
                emit q->servicesChanged();
            }
        }
    }
    else if(scanResults == NULL){
        if(m_serviceModel->rowCount()!=0){ //se il rowCount è diverso da zero vuol dire che bisogna cancellare i servizi
            for(int i=0;i<m_serviceModel->rowCount();i++){
                m_serviceModel->remove(i);
                emit q->servicesChanged();
            }
        }
    }
    else{
        int size = scanResults.callMethod<jint>("size","()I");
        if(size != 0){
            for(int i=0;i<size;i++){ //ogni volta che prelevo un ssid devo controllare che esista nella lista
                scanResult = scanResults.callObjectMethod("get","(I)Ljava/lang/Object;",i);
                wifiSsid = scanResult.getObjectField<jstring>("SSID").toString();
                if(!checkExistence(wifiSsid)){ //se non lo trovo all' interno posso mandare il segnale della modifica dei services
                    QNetworkSettingsService *service = new QNetworkSettingsService(wifiSsid, this);
                    service->propertyCall("Name",wifiSsid);
                    m_serviceModel->append(service);
                    emit q->servicesChanged();
                }
            }
            //parte in cui rimuovo i servizi che non sono più presenti nella scanResult
            QList<QNetworkSettingsService*> listaServices = m_serviceModel->getModel();
            foreach (QNetworkSettingsService *service, listaServices) {
                trovato = false;
                for(int i=0;i<size;i++){
                    scanResult = scanResults.callObjectMethod("get","(I)Ljava/lang/Object;",i);
                    wifiSsid = scanResult.getObjectField<jstring>("SSID").toString();
                    if(wifiSsid == service->name()){
                        trovato = true;
                    }
                }
                if(trovato == false){
                    m_serviceModel->removeService(service->id());
                    emit q->servicesChanged();
                }
            }
        }
    }
}
bool QNetworkSettingsManagerPrivate::checkExistence(QString ssid){
    QList<QNetworkSettingsService*> listaServices = m_serviceModel->getModel();
    int size = listaServices.size();
    for(int i=0;i<size;i++){
        QNetworkSettingsService *s = listaServices.at(i);
        if(s->id()==ssid){
            return true;
        }
    }
    return false;
}
bool QNetworkSettingsManagerPrivate::initialize()
{
    if(m_initialized && (m_context != NULL))
        return m_initialized;

    QJniObject activity = QNativeInterface::QAndroidApplication::context();
    m_context = activity.callObjectMethod("getApplicationContext","()Landroid/content/Context;");

    if(m_context != nullptr){
        QtAndroidPrivate::PermissionResult locPermission = QtAndroidPrivate::Undetermined;
        QtAndroidPrivate::checkPermission(QtAndroidPrivate::PreciseLocation)
            .then([&locPermission](QtAndroidPrivate::PermissionResult result){
                if(result != QtAndroidPrivate::PermissionResult::Authorized){
                    result = QtAndroidPrivate::requestPermission(QtAndroidPrivate::PreciseLocation).result();
                }
                locPermission = result;
            });
        if(locPermission == QtAndroidPrivate::Authorized){
            m_initialized = true;
        }
        else{
            m_initialized = false;
        }
    }
    else{
        m_initialized = false;
    }
    return m_initialized;
}

void QNetworkSettingsManagerPrivate::requestInput(const QString& service, const QString& type)
{
    Q_UNUSED(service);
    Q_UNUSED(type);
    emit m_agent->showUserCredentialsInput();
}

void QNetworkSettingsManagerPrivate::connectBySsid(const QString &name){
    m_unnamedServicesForSsidConnection = m_unnamedServices;
    tryNextConnection();
    m_currentSsid = name;
}

void QNetworkSettingsManagerPrivate::clearConnectionState() //devo aggiungere una chiamata che mi permetta di togliere gli indirizzi della connessione che sto pulendo
{
    Q_Q(QNetworkSettingsManager);
    m_currentSsid.clear();
}

void QNetworkSettingsManagerPrivate::tryNextConnection()
{
    Q_Q(QNetworkSettingsManager);
    QNetworkSettingsService *service = nullptr;

    if(!m_currentSsid.isEmpty()){
        service = m_serviceModel->getByName(m_currentSsid); //getting the current ssid to establish a connection
        m_currentSsid.clear();  //clearing the current ssid
    }
    if (!service) {
        if (!m_unnamedServicesForSsidConnection.isEmpty()) {
            service = m_unnamedServicesForSsidConnection.take(m_unnamedServicesForSsidConnection.firstKey());
        } else {
            q->clearConnectionState();
        }
    }

    if (service) {
        service->doConnectService();
    }
}

void QNetworkSettingsManagerPrivate::onTechnologyAdded()
{

    Q_Q(QNetworkSettingsManager);

    QJniObject connectivityManager = m_context.callObjectMethod("getSystemService",
                                                        "(Ljava/lang/String;)Ljava/lang/Object;",
                                                        QJniObject::fromString("connectivity").object<jstring>());

    QJniObject network = connectivityManager.callObjectMethod("getActiveNetwork","()Landroid/net/Network;");

    QJniObject networkCap = connectivityManager.callObjectMethod("getNetworkCapabilities",
                                                        "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;",
                                                        network.object<jobject>());

    jboolean hasTransport = networkCap.callMethod<jboolean>("hasTransport","(I)Z",1);
    if(hasTransport){
        qDebug() << "WIFI";
        //m_interfaceModel.append();
        emit q->servicesChanged();
    }
    else{
        hasTransport = networkCap.callMethod<jboolean>("hasTransport","(I)Z",3);
        if(hasTransport){
            qDebug() << "WIRED";

        }
        else{
            qDebug() << "ELSE";
        }
    }

    emit q->interfacesChanged();

}

void QNetworkSettingsManagerPrivate::setCurrentWifiConnection(QNetworkSettingsService *connection)
{
    m_currentWifiConnection = connection;
}

QNetworkSettingsService *QNetworkSettingsManagerPrivate::currentWifiConnection() const
{
    return m_currentWifiConnection.data();
}

void QNetworkSettingsManagerPrivate::setCurrentWiredConnection(QNetworkSettingsService *connection)
{
    m_currentWiredConnection = connection;
}

QNetworkSettingsService *QNetworkSettingsManagerPrivate::currentWiredConnection() const
{
    return m_currentWiredConnection.data();
}

void QNetworkSettingsManagerPrivate::onServicesChanged()
{
    Q_Q(QNetworkSettingsManager);


}

void QNetworkSettingsManagerPrivate::handleNewService(const QString &servicePath)
{
}

void QNetworkSettingsManagerPrivate::setUserAgent(QNetworkSettingsUserAgent *agent)
{
    m_agent = agent;
}

void QNetworkSettingsManagerPrivate::serviceReady()
{

}

QT_END_NAMESPACE
