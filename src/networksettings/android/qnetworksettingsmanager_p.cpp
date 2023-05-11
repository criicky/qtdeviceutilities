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
        changes = false;
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
    if(wifiOn){ //if the wifi in on i check if theres a active network
        QJniObject activeNetwork = m_connectivitymanager.callObjectMethod("getActiveNetwork",
                                                                          "()Landroid/net/Network;");

        if(activeNetwork==NULL){ //if the activeNetwork is null and the currentSsid is not empty i need to clear the connection state
            if(!m_currentSsid.isEmpty()){
                if(m_serviceModel->getByName(m_currentSsid)->type() == QNetworkSettingsType::Wifi){
                    m_currentWifiConnection.clear();
                    emit q->currentWifiConnectionChanged();
                }
                else if(m_serviceModel->getByName(m_currentSsid)->type() == QNetworkSettingsType::Wired){
                    m_currentWiredConnection.clear();
                    emit q->currentWiredConnectionChanged();
                }
                clearConnectionState();//clearing the connection because is not active anymore
            }
        }
        else{ //getting all the infos of the network

            QJniObject networkCapabilities = m_connectivitymanager.callObjectMethod("getNetworkCapabilities",
                                                                                    "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;",
                                                                                    activeNetwork.object<jobject>());

            if(networkCapabilities.callMethod<jboolean>("hasTransport","(I)Z",1)){//if the network has transport for wifi than ill check the ssid

                QJniObject wifiInfo = m_wifimanager.callObjectMethod("getConnectionInfo",
                                                                     "()Landroid/net/wifi/WifiInfo;");

                QString ssid = wifiInfo.callObjectMethod("getSSID","()Ljava/lang/String;").toString();

                ssid = formattedSSID(ssid); //clearing the ssid because it has " at the front and at the end

                if(!m_currentSsid.isEmpty() && m_currentSsid != ssid){ //i need to check if the new active connection is different from the previous one
                    clearConnectionState(); //when they are different i need to clear the old one
                }
                m_currentSsid = ssid;

                //the service is already inside the serviceModel because i called updateServices
                QNetworkSettingsService *service = m_serviceModel->getByName(m_currentSsid); //getting the service object to change its priorities

                //the type is wifi because i checked if the connectin has transport for wifi
                type->setType(QNetworkSettingsType::Wifi);
                service->propertyCall("Type",QVariant::fromValue(type));

                wirelessConfig(wifiInfo,networkCapabilities,service); //setting the signalStrength and its securityType

                QJniObject linkProperties = m_connectivitymanager.callObjectMethod("getLinkProperties",
                                                                                   "(Landroid/net/Network;)Landroid/net/LinkProperties;",
                                                                                   activeNetwork.object<jobject>());

                QJniObject proxyInfo = linkProperties.callObjectMethod("getHttpProxy",
                                                                       "()Landroid/net/ProxyInfo;"); //proxy could be null if not set

                savingProxy(proxyInfo,service); //updating proxys infos

                QJniObject domain = linkProperties.callObjectMethod("getDnsServers","()Ljava/util/List;"); //getDnsServers cannot return a null value
                savingDNS(domain,service); //updating dnss properties

                QJniObject linkAddresses = linkProperties.callObjectMethod("getLinkAddresses",
                                                                           "()Ljava/util/List;"); //list of all addresses and other fields (ipv4 or ipv6)

                int size = linkAddresses.callMethod<jint>("size","()I");

                for(int i=0;i<size;i++){
                    QJniObject linkAddress = linkAddresses.callObjectMethod("get","(I)Ljava/lang/Object;",i);
                    QJniObject address = linkAddress.callObjectMethod("getAddress","()Ljava/net/InetAddress;"); //could be ipv4 format or ipv6 so i need to check it

                    jclass inet4address = env.findClass("java/net/Inet4Address");
                    jclass inet6address = env.findClass("java/net/Inet6Address");

                    if(env->IsInstanceOf(address.object<jobject>(),inet4address) && (ipv4->address().isEmpty())){ //checking the class instance

                        QString indirizzo = address.callObjectMethod("getHostAddress",
                                                                     "()Ljava/lang/String;").toString();
                        ipv4->setAddress(indirizzo);

                        int prefixLength = linkAddress.callMethod<jint>("getPrefixLength","()I");

                        ipv4->setMask(QString::number(prefixLength)); //the mask is set using its length

                        QJniObject dhcp = linkProperties.callObjectMethod("getDhcpServerAddress",
                                                                          "()Ljava/net/Inet4Address;");

                        if(dhcp != NULL){ //checking if there are dhcp settings
                            ipv4->setMethod(QNetworkSettingsIPv4::Method::Dhcp);
                        }
                        else{
                            ipv4->setMethod(QNetworkSettingsIPv4::Method::Manual);
                        }

                        gateway = savingGateway(linkProperties,"IPv4");

                        QVariant variant = QVariant::fromValue(ipv4);
                        if(updateProperties(variant,service)){ //checking if those priorities changed during the call
                            service->propertyCall("IPv4",variant);
                            changes = true; //if they changed i set changes true so i now that and can send a signal to the user
                        }
                    }
                    else if(env->IsInstanceOf(address.object<jobject>(),inet6address) && (ipv6->address().isEmpty())){ //checking if its an instance of ipv6

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
                        if(updateProperties(variant,service)){ //same thing as before
                            service->propertyCall("IPv6",variant);
                            changes = true;
                        }
                    }
                }

                if(changes){ //if something changed then ill send a signal to the user
                    setCurrentWifiConnection(service);
                    emit q->currentWifiConnectionChanged();
                    changes = false; //sent the signal i can put changes to false and wait for another update
                }
            }
            else{
                qWarning() << "Network does not have transport for wifi"; //quindi può essere Wired
            }
        }
    }
    else{
        //if the wifi is off i check the last connection known and if its wifi i need to clear that
        if(!m_currentSsid.isEmpty()){
            QNetworkSettingsService *service = m_serviceModel->getByName(m_currentSsid);
            if(service != nullptr){
                if(service->type() == QNetworkSettingsType::Wifi){
                    clearConnectionState();
                    m_currentWifiConnection.clear();
                    emit q->currentWifiConnectionChanged();
                }
            }
        }
    }
}

//function to check services properties
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

//setting wireless Configs
void QNetworkSettingsManagerPrivate::wirelessConfig(QJniObject wifiInfo,QJniObject networkCapabilities,QNetworkSettingsService *service){
    int signalStrength = networkCapabilities.callMethod<jint>("getSignalStrength","()I");
    int securityType = wifiInfo.callMethod<jint>("getCurrentSecurityType","()I");
    bool isOutOfRange = false;
    service->propertyCall("Strength",signalStrength);
    service->propertyCall("Security",securityType);
}

QString QNetworkSettingsManagerPrivate::savingGateway(QJniObject linkProperties,QString key){ //update Gateway
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
            if(route.callMethod<jboolean>("isDefaultRoute","()Z")){ //checking if that route is the default one because there could be more than one gateway
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
    return NULL; //is it possible to not find any gateway?
}

void QNetworkSettingsManagerPrivate::savingProxy(QJniObject proxyInfo,QNetworkSettingsService *service){
    QNetworkSettingsProxy *proxy = new QNetworkSettingsProxy();
    QJniEnvironment env;
    QStringList excluded;
    if(proxyInfo == NULL){ //if the proxy found is null i need to check if there was one before
        if(!service->proxy()->url().isEmpty()){ //if there was one i need to clear it
            service->propertyCall("Proxy",QVariant::fromValue(proxy));
            changes = true;
        }
    }
    else{
        proxy->setUrl(proxyInfo.callObjectMethod("getPacFileUrl","()Landroid/net/Uri;").toString());
        QJniObject exclusionList = proxyInfo.callObjectMethod("getExclusionList",
                                                              "()[Ljava/lang/String;");
        int size = env->GetArrayLength(exclusionList.object<jobjectArray>());
        for(int i=0;i<size;i++){  //need to create the list of excluded hosts
            jstring str = (jstring) env->GetObjectArrayElement(exclusionList.object<jobjectArray>(),i); //need to convert the string because it is in utf format
            const char* utfString = env->GetStringUTFChars(str ,0);
            QString convertedString = QString::fromUtf8(utfString);
            excluded.append(convertedString);
        }
        proxy->setExcludes(excluded);
        if(service->proxy()->url() != proxy->url()){ //if the proxy changed url i need to change it in the service
            changes = true;
            service->proxy()->setUrl(proxy->url());
        }
        QStringList oldList = service->proxy()->excludes()->index(0,0).data().toStringList(); //getting the old list
        if(oldList.isEmpty() && !excluded.isEmpty()){
            changes = true;
            service->propertyCall("Proxy",QVariant::fromValue(proxy));
        }
        else{ //if both arent empty check if they are the same
            foreach (const QString &str, excluded) {
                if(!oldList.contains(str)){
                    changes = true;
                    service->propertyCall("Proxy",QVariant::fromValue(proxy));
                    break;
                }
            }
            foreach (const QString &str, oldList) { //its possible that some excluded host is not anymore excluded
                if(!excluded.contains(str)){
                    changes = true;
                    service->propertyCall("Proxy",QVariant::fromValue(proxy));
                    break;
                }
            }
        }
    }
}

void QNetworkSettingsManagerPrivate::savingDNS(QJniObject domain,QNetworkSettingsService *service){ //function that updates dnses
    int totDNS = domain.callMethod<jint>("size","()I");
    QStringList newAddresses;
    QStringList newNames;
    for(int y = 0; y < totDNS; y++){ //creating the list of dnss names and addresses
        QJniObject dnsIndirizzo = domain.callObjectMethod("get","(I)Ljava/lang/Object;",y);
        newAddresses.append(dnsIndirizzo.callObjectMethod("getHostAddress","()Ljava/lang/String;").toString());
        newNames.append(dnsIndirizzo.callObjectMethod("getHostName","()Ljava/lang/String;").toString());
    }
    QStringList oldAddresses = service->domains()->index(0,0).data().toStringList();
    QStringList oldNames = service->nameservers()->index(0,0).data().toStringList();
    if(oldAddresses.isEmpty() && !newAddresses.isEmpty() && !newNames.isEmpty() && oldNames.isEmpty()){ //need to update both lists
        changes = true;
        service->propertyCall("Domains",newAddresses);
        service->propertyCall("Nameservers",newNames);
    }
    else{
        foreach(const QString &item,newAddresses){  //need to check if theres a new domain address
            if(!oldAddresses.contains(item)){
                changes = true;
                service->propertyCall("Domains",newAddresses);
                break;
            }
        }
        foreach(const QString &str,newNames){ //need to check if theres a new domain name
            if(!oldNames.contains(str)){
                changes = true;
                service->propertyCall("Nameservers",newNames);
                break;
            }
        }
        foreach(const QString &item,oldAddresses){  //need to check if theres not a domain address anymore
            if(!newAddresses.contains(item)){
                changes = true;
                service->propertyCall("Domains",newAddresses);
                break;
            }
        }
        foreach(const QString &str,oldNames){ //need to check if theres not a domain name anymore
            if(!newNames.contains(str)){
                changes = true;
                service->propertyCall("Nameservers",newNames);
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

void QNetworkSettingsManagerPrivate::updateServices() //create a list of known ssids
{
    bool found;
    QJniObject scanResult;
    QString wifiSsid;
    Q_Q(QNetworkSettingsManager);

    QJniObject scanResults = m_wifimanager.callObjectMethod("getScanResults","()Ljava/util/List;");

    if(!m_wifimanager.callMethod<jboolean>("isWifiEnabled","()Z")){
        QList<QNetworkSettingsService*> services = m_serviceModel->getModel();
        for(int i=0;i<services.size();i++){
            QNetworkSettingsService *s = services.at(i);
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
        if(m_serviceModel->rowCount()!=0){ //if scanResults is empty and there are services inside the serviceModel i need to clear them
            for(int i=0;i<m_serviceModel->rowCount();i++){
                m_serviceModel->remove(i);
                emit q->servicesChanged();
            }
        }
    }
    else{
        int size = scanResults.callMethod<jint>("size","()I");
        if(size != 0){
            for(int i=0;i<size;i++){ //everytime a ssid is found i need to check if its already in the list
                scanResult = scanResults.callObjectMethod("get","(I)Ljava/lang/Object;",i);
                wifiSsid = scanResult.getObjectField<jstring>("SSID").toString();
                if(!checkExistence(wifiSsid)){ //if it is not found i need to send a signal of servicesChanged
                    QNetworkSettingsService *service = new QNetworkSettingsService(wifiSsid, this);
                    service->propertyCall("Name",wifiSsid);
                    m_serviceModel->append(service);
                    emit q->servicesChanged();
                }
            }
            //removing services that are not in the scanResults anymore
            QList<QNetworkSettingsService*> listaServices = m_serviceModel->getModel();
            foreach (QNetworkSettingsService *service, listaServices) {
                found = false;
                for(int i=0;i<size;i++){
                    scanResult = scanResults.callObjectMethod("get","(I)Ljava/lang/Object;",i);
                    wifiSsid = scanResult.getObjectField<jstring>("SSID").toString();
                    if(wifiSsid == service->name()){
                        found = true;
                    }
                }
                if(found == false){
                    m_serviceModel->removeService(service->id());
                    emit q->servicesChanged();
                }
            }
        }
    }
}
bool QNetworkSettingsManagerPrivate::checkExistence(QString ssid){ //function to check if the ssid is already in the serviceModel
    QList<QNetworkSettingsService*> services = m_serviceModel->getModel(); //list of every service found in the serviceModel
    int size = services.size();
    for(int i=0;i<size;i++){
        QNetworkSettingsService *s = services.at(i);
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

    if(m_context != nullptr){ //here im sending a check permission to see if every permission i need is granted
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

void QNetworkSettingsManagerPrivate::clearConnectionState() //clearing the old conneciton
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
