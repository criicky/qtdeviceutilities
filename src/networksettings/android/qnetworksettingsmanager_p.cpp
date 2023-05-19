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
#include "androidServiceDB.h"

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
        m_telephonymanager = m_context.callObjectMethod("getSystemService","(Ljava/lang/String;)Ljava/lang/Object;",
                                                    QJniObject::fromString("phone").object<jstring>());
        m_changes = false;
        m_androidService = AndroidServiceDB::getInstance();
        m_androidInterface = AndroidInterfaceDB::getInstance();

        startActivity();

        //startTimer(2000);
    }
}

void QNetworkSettingsManagerPrivate::available(JNIEnv *env,jobject thiz,jobject network)
{
    qDebug() << "clownfiesta";
    /*QString ssid;
    QJniObject nw = (QJniObject) network;
    QJniObject activeNetwork = m_connectivitymanager.callObjectMethod("getActiveNetwork","()Landroid/net/Network;");
    //qWarning() << "available active network: " << activeNetwork.toString() << " network: " << nw.toString();
    QJniObject networkCapabilities = m_connectivitymanager.callObjectMethod("getNetworkCapabilities",
                                                                          "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;",
                                                                          nw.object<jobject>());
    ssid = getSSID(networkCapabilities);
    qDebug() << "available getSSID: " << ssid;
    if(!checkExistence(ssid))
    {
        QNetworkSettingsService *service = new QNetworkSettingsService(ssid);
        m_serviceModel->append(service);
    }*/
}

void QNetworkSettingsManagerPrivate::startActivity()
{
    QJniEnvironment env;
    JNINativeMethod method[] = {{"available","(Landroid/net/Network;)V",
                                 reinterpret_cast<void *>(&available)}};

    env.registerNativeMethods("receiver/brdReceiver",method,1);

    QJniObject intent = QJniObject::callStaticObjectMethod("receiver/brdReceiver","start",
                                                           "(Landroid/content/Context;)Landroid/content/Intent;",
                                                           m_context.object<jobject>());

    m_context.callMethod<void>("startActivity","(Landroid/content/Intent;)V",intent.object<jobject>());
}

QString QNetworkSettingsManagerPrivate::getSSID(QJniObject networkCapabilities)
{
    QString ssid;
    if(networkCapabilities.callMethod<jboolean>("hasTransport","(I)Z",0)) //cellular data
    {
        QJniObject packageManager = m_context.callObjectMethod("getPackageManager","()Landroid/content/pm/PackageManager;");

        bool hasFeature = packageManager.callMethod<jboolean>("hasSystemFeature","(Ljava/lang/String;)Z",
                                                              QJniObject::fromString("android.hardware.telephony.gsm").object<jstring>());

        ssid = m_telephonymanager.callObjectMethod("getSimCarrierIdName","()Ljava/lang/CharSequence;").toString();
    }
    else if(networkCapabilities.callMethod<jboolean>("hasTransport","(I)Z",1)) //wifi
    {
        QJniObject wifiInfo = m_wifimanager.callObjectMethod("getConnectionInfo",
                                                           "()Landroid/net/wifi/WifiInfo;");
        ssid = wifiInfo.callObjectMethod("getSSID","()Ljava/lang/String;").toString();
    }
    return ssid;
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

    onTechnologyAdded();
    updateServices();

    bool wifiOn = m_wifimanager.callMethod<jboolean>("isWifiEnabled","()Z");
    //if(wifiOn)
    //{ //if the wifi in on i check if theres a active network
        QJniObject activeNetwork = m_connectivitymanager.callObjectMethod("getActiveNetwork",
                                                                          "()Landroid/net/Network;");

        if(activeNetwork==NULL)
        { //if the activeNetwork is null and the currentSsid is not empty i need to clear the connection state and interface
            qDebug() << "am i here";
            if(!m_currentSsid.isEmpty())
            {
                clearConnectionState();//clearing the connection because is not active anymore
            }
        }
        else
        { //getting all the infos of the network
            QJniObject networkCapabilities = m_connectivitymanager.callObjectMethod("getNetworkCapabilities",
                                                                                    "(Landroid/net/Network;)Landroid/net/NetworkCapabilities;",
                                                                                    activeNetwork.object<jobject>());

            if(networkCapabilities.callMethod<jboolean>("hasTransport","(I)Z",1))
            {//if the network has transport for wifi than ill check the ssid
                QJniObject wifiInfo = m_wifimanager.callObjectMethod("getConnectionInfo",
                                                                     "()Landroid/net/wifi/WifiInfo;");

                QString ssid = wifiInfo.callObjectMethod("getSSID","()Ljava/lang/String;").toString();

                ssid = formattedSSID(ssid); //clearing the ssid because it has " at the front and at the end

                if(!m_currentSsid.isEmpty() && m_currentSsid != ssid)
                { //i need to check if the new active connection is different from the previous one
                    clearConnectionState(); //when they are different i need to clear the old one
                }
                m_currentSsid = ssid;

                m_androidService->setProperty("ID",ssid);
                m_androidService->setProperty("Name",ssid);

                //the type is wifi because i checked if the connectin has transport for wifi
                type->setType(QNetworkSettingsType::Wifi);
                m_androidService->setProperty("Type",QVariant::fromValue(type));

                int signalStrength = networkCapabilities.callMethod<jint>("getSignalStrength","()I");
                int securityType = wifiInfo.callMethod<jint>("getCurrentSecurityType","()I");

                m_androidService->setProperty("Strength",signalStrength);
                m_androidService->setProperty("Security",securityType);

                QJniObject linkProperties = m_connectivitymanager.callObjectMethod("getLinkProperties",
                                                                                   "(Landroid/net/Network;)Landroid/net/LinkProperties;",
                                                                                   activeNetwork.object<jobject>());

                //this section is used to change the state of the interface that is being used from ready to online
                QString newInterface = linkProperties.callObjectMethod("getInterfaceName",
                                                                       "()Ljava/lang/String;").toString();
                QList<QNetworkSettingsInterface*> interfaces = m_interfaceModel.getModel();
                foreach (QNetworkSettingsInterface *interface, interfaces)
                {
                    if(interface->name() == newInterface && (interface->state() != QNetworkSettingsState::Online))
                    {
                        m_androidInterface->updateProperty("Name",newInterface);
                        QNetworkSettingsState *state = new QNetworkSettingsState();
                        state->setState(QNetworkSettingsState::Online);
                        m_androidInterface->updateProperty("Connected",QVariant::fromValue(state));
                        break;
                    }
                }
                //end of the section

                QJniObject proxyInfo = linkProperties.callObjectMethod("getHttpProxy",
                                                                       "()Landroid/net/ProxyInfo;"); //proxy could be null if not set

                QNetworkSettingsProxy *proxy = new QNetworkSettingsProxy();
                if(proxyInfo != NULL)
                {
                    QStringList excluded;
                    proxy->setUrl(proxyInfo.callObjectMethod("getPacFileUrl","()Landroid/net/Uri;").toString());
                    QJniObject exclusionList = proxyInfo.callObjectMethod("getExclusionList",
                                                                          "()[Ljava/lang/String;");
                    int size = env->GetArrayLength(exclusionList.object<jobjectArray>());
                    for(int i=0;i<size;i++)
                    {  //need to create the list of excluded hosts
                        jstring str = (jstring) env->GetObjectArrayElement(exclusionList.object<jobjectArray>(),i); //need to convert the string because it is in utf format
                        const char* utfString = env->GetStringUTFChars(str ,0);
                        QString convertedString = QString::fromUtf8(utfString);
                        excluded.append(convertedString);
                    }
                    proxy->setExcludes(excluded);
                }
                m_androidService->setProperty("Proxy",QVariant::fromValue(proxy));

                QJniObject domain = linkProperties.callObjectMethod("getDnsServers","()Ljava/util/List;"); //getDnsServers cannot return a null value
                int totDNS = domain.callMethod<jint>("size","()I");
                QStringList newAddresses;
                QStringList newNames;
                for(int y = 0; y < totDNS; y++)
                { //creating the list of dnss names and addresses
                    QJniObject dnsIndirizzo = domain.callObjectMethod("get","(I)Ljava/lang/Object;",y);
                    newAddresses.append(dnsIndirizzo.callObjectMethod("getHostAddress","()Ljava/lang/String;").toString());
                    newNames.append(dnsIndirizzo.callObjectMethod("getHostName","()Ljava/lang/String;").toString());
                }
                m_androidService->setProperty("Domains",newAddresses);
                m_androidService->setProperty("Nameservers",newNames);

                QJniObject linkAddresses = linkProperties.callObjectMethod("getLinkAddresses",
                                                                           "()Ljava/util/List;"); //list of all addresses and other fields (ipv4 or ipv6)

                int size = linkAddresses.callMethod<jint>("size","()I");

                for(int i=0;i<size;i++)
                {
                    QJniObject linkAddress = linkAddresses.callObjectMethod("get","(I)Ljava/lang/Object;",i);
                    QJniObject address = linkAddress.callObjectMethod("getAddress","()Ljava/net/InetAddress;"); //could be ipv4 format or ipv6 so i need to check it

                    jclass inet4address = env.findClass("java/net/Inet4Address");
                    jclass inet6address = env.findClass("java/net/Inet6Address");

                    if(env->IsInstanceOf(address.object<jobject>(),inet4address) && (ipv4->address().isEmpty()))
                    { //checking the class instance

                        QString indirizzo = address.callObjectMethod("getHostAddress",
                                                                     "()Ljava/lang/String;").toString();
                        ipv4->setAddress(indirizzo);

                        int prefixLength = linkAddress.callMethod<jint>("getPrefixLength","()I");

                        ipv4->setMask(QString::number(prefixLength)); //the mask is set using its length

                        QJniObject dhcp = linkProperties.callObjectMethod("getDhcpServerAddress",
                                                                          "()Ljava/net/Inet4Address;");

                        if(dhcp != NULL)
                        { //checking if there are dhcp settings
                            ipv4->setMethod(QNetworkSettingsIPv4::Method::Dhcp);
                        }
                        else
                        {
                            ipv4->setMethod(QNetworkSettingsIPv4::Method::Manual);
                        }

                        gateway = savingGateway(linkProperties,"IPv4");
                        ipv4->setGateway(gateway);

                        QVariant variant = QVariant::fromValue(ipv4);
                        m_androidService->setProperty("IPv4",variant);
                    }
                    else if(env->IsInstanceOf(address.object<jobject>(),inet6address) && (ipv6->address().isEmpty()))
                    { //checking if its an instance of ipv6

                        ipv6->setAddress(address.callObjectMethod("getHostAddress",
                                                                  "()Ljava/lang/String;").toString());

                        gateway = savingGateway(linkProperties,"IPv6");
                        ipv6->setGateway(gateway);

                        if(!address.toString().isEmpty())
                        {
                            ipv6->setMethod(QNetworkSettingsIPv6::Method::Auto);
                        }
                        else
                        {
                            ipv6->setMethod(QNetworkSettingsIPv6::Method::Manual);
                        }
                        ipv6->setPrefixLength(linkAddress.callMethod<jint>("getPrefixLength","()I"));

                        QVariant variant = QVariant::fromValue(ipv6);
                        m_androidService->setProperty("IPv6",variant);
                    }
                }
            }
            else if(networkCapabilities.callMethod<jboolean>("hasTransport","(I)Z",3))
            { //need to check if hasTransport for wired or something else
                qWarning() << "Network does not have transport for wifi"; //quindi può essere Wired
                type->setType(QNetworkSettingsType::Wired);
                m_androidService->setProperty("Type",QVariant::fromValue(type));
            }
            else if(networkCapabilities.callMethod<jboolean>("hasTransport","(I)Z",0))
            {
                qDebug() << "cellular data";
                QJniObject transportInfo = networkCapabilities.callObjectMethod("getTransportInfo",
                                                                                "()Landroid/net/TransportInfo;");
                if(transportInfo == NULL)
                {
                    qWarning() << "no transport info";
                }
                QJniObject wifiInfo = m_wifimanager.callObjectMethod("getConnectionInfo",
                                                                     "()Landroid/net/wifi/WifiInfo;");
                if(wifiInfo == NULL)
                {
                    qWarning() << "no connection info";
                }
                else
                {
                    QJniObject telephonyManager = m_context.callObjectMethod("getSystemService",
                                                                            "(Ljava/lang/String;)Ljava/lang/Object;",
                                                                            QJniObject::fromString("phone").object<jstring>());
                    QJniObject packageManager = m_context.callObjectMethod("getPackageManager",
                                                                           "()Landroid/content/pm/PackageManager;");

                    bool feature = packageManager.callMethod<jboolean>("hasSystemFeature",
                                                                       "(Ljava/lang/String;)Z",
                                                                       QJniObject::fromString("android.hardware.telephony.gsm").object<jstring>());

                    if(!feature)
                    {
                        qDebug() << "dio cane";
                    }
                    else
                    {
                        qDebug() << "esiste la feature";
                    }
                    if(telephonyManager != NULL)
                    {
                        int size = telephonyManager.callMethod<jint>("getActiveModemCount","()I");
                        qDebug() << size;
                        //qWarning() << "imei: " << telephonyManager.callObjectMethod("getImei","()Ljava/lang/String;").toString();
                    }
                    //QJniObject imei = telephonyManager.callObjectMethod("getImei","(I)Ljava/lang/String;",0).toString();
                    /*QJniObject linkProperties = m_connectivitymanager.callObjectMethod("getLinkProperties",
                                                                                       "(Landroid/net/Network;)Landroid/net/LinkProperties;",
                                                                                       activeNetwork.object<jobject>());
                    QString newInterface = linkProperties.callObjectMethod("getInterfaceName",
                                                                           "()Ljava/lang/String;").toString();
                    qWarning() << newInterface;*/
                }
            }
            QNetworkSettingsService *oldService = m_serviceModel->getByName(m_currentSsid);
            if(oldService != nullptr && m_androidService->changes) //it there was no change in the call then i dont need to send a signal to the user
            {
                m_androidService->changes = false; //resetting changes for the next call
                if(oldService->type() == QNetworkSettingsType::Wifi) //need to check the type just to send the right signal
                {
                    setCurrentWifiConnection(oldService);
                    emit q->currentWifiConnectionChanged();
                }
                else
                {
                    setCurrentWiredConnection(oldService);
                    emit q->currentWiredConnectionChanged();
                }
            }
        }
    //}
    /*else
    {
        //if the wifi is off i check the last connection known and if its wifi i need to clear that
        if(!m_currentSsid.isEmpty())
        {
            clearConnectionState();
        }
    }*/
}

QString QNetworkSettingsManagerPrivate::savingGateway(QJniObject linkProperties,QString key){ //update Gateway
    QString gateway;
    QJniEnvironment env;
    QJniObject routes = linkProperties.callObjectMethod("getRoutes","()Ljava/util/List;");
    jclass inet4address = env.findClass("java/net/Inet4Address");
    jclass inet6address = env.findClass("java/net/Inet6Address");

    if(routes!=NULL)
    {
        int size = routes.callMethod<jint>("size","()I");
        for(int i=0;i<size;i++)
        {
            QJniObject route = routes.callObjectMethod("get","(I)Ljava/lang/Object;",i);
            gateway = route.callObjectMethod("getGateway","()Ljava/net/InetAddress;")
                          .callObjectMethod("getHostAddress","()Ljava/lang/String;").toString();
            if(route.callMethod<jboolean>("isDefaultRoute","()Z"))
            { //checking if that route is the default one because there could be more than one gateway
                if(env->IsInstanceOf(route.callObjectMethod("getGateway",
                                                             "()Ljava/net/InetAddress;").object<jobject>(),
                                      inet4address) && (key=="IPv4"))
                {
                    return gateway;
                }
                else if(env->IsInstanceOf(route.callObjectMethod("getGateway",
                                                                  "()Ljava/net/InetAddress;").object<jobject>(),
                                           inet6address) && (key=="IPv6"))
                {
                    return gateway;
                }
            }
        }
    }
    return NULL; //is it possible to not find any gateway?
}

QString QNetworkSettingsManagerPrivate::formattedSSID(QString ssid){

    if (ssid.front() == '"' && ssid.back() == '"')
    {
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

    if(!m_wifimanager.callMethod<jboolean>("isWifiEnabled","()Z"))
    {
        QList<QNetworkSettingsService*> services = m_serviceModel->getModel();
        for(int i=0;i<services.size();i++)
        {
            QNetworkSettingsService *s = services.at(i);
            if(s->type() == QNetworkSettingsType::Wifi)
            {
                if(s->id() == m_currentSsid)
                {
                    clearConnectionState();
                }
                m_serviceModel->removeService(s->id());
                emit q->servicesChanged();
            }
        }
    }
    else if(scanResults == NULL)
    {
        qDebug() << "ScanResult nullo";
        if(m_serviceModel->rowCount()!=0)
        { //if scanResults is empty and there are services inside the serviceModel i need to clear them
            for(int i=0;i<m_serviceModel->rowCount();i++)
            {
                m_serviceModel->remove(i);
                emit q->servicesChanged();
            }
        }
    }
    else{
        int size = scanResults.callMethod<jint>("size","()I");
        if(size != 0)
        {
            for(int i=0;i<size;i++)
            { //everytime a ssid is found i need to check if its already in the list
                scanResult = scanResults.callObjectMethod("get","(I)Ljava/lang/Object;",i);
                wifiSsid = scanResult.getObjectField<jstring>("SSID").toString();
                if(!checkExistence(wifiSsid))
                { //if it is not found i need to send a signal of servicesChanged
                    QNetworkSettingsService *service = new QNetworkSettingsService(wifiSsid, this);
                    m_serviceModel->append(service);
                    emit q->servicesChanged();
                }
            }
            //removing services that are not in the scanResults anymore
            QList<QNetworkSettingsService*> listaServices = m_serviceModel->getModel();
            foreach (QNetworkSettingsService *service, listaServices)
            {
                found = false;
                for(int i=0;i<size;i++)
                {
                    scanResult = scanResults.callObjectMethod("get","(I)Ljava/lang/Object;",i);
                    wifiSsid = scanResult.getObjectField<jstring>("SSID").toString();
                    if(wifiSsid == service->id())
                    {
                        found = true;
                    }
                }
                if(found == false)
                {
                    m_serviceModel->removeService(service->id());
                    emit q->servicesChanged();
                }
            }
        }
    }
}
bool QNetworkSettingsManagerPrivate::checkExistence(QString ssid)
{ //function to check if the ssid is already in the serviceModel
    QList<QNetworkSettingsService*> services = m_serviceModel->getModel(); //list of every service found in the serviceModel
    int size = services.size();
    for(int i=0;i<size;i++)
    {
        QNetworkSettingsService *s = services.at(i);
        if(s->id()==ssid)
        {
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

    if(m_context != nullptr)
    { //here im sending a check permission to see if every permission i need is granted
        QtAndroidPrivate::PermissionResult locPermission = QtAndroidPrivate::Undetermined;
        QtAndroidPrivate::checkPermission(QtAndroidPrivate::PreciseLocation)
            .then([&locPermission](QtAndroidPrivate::PermissionResult result)
                  {
                if(result != QtAndroidPrivate::PermissionResult::Authorized)
                {
                    result = QtAndroidPrivate::requestPermission(QtAndroidPrivate::PreciseLocation).result();
                }
                locPermission = result;
            });
        if(locPermission == QtAndroidPrivate::Authorized)
        {
            m_initialized = true;
        }
        else
        {
            m_initialized = false;
        }
    }
    else
    {
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

void QNetworkSettingsManagerPrivate::connectBySsid(const QString &name)
{
    m_unnamedServicesForSsidConnection = m_unnamedServices;
    tryNextConnection();
    m_currentSsid = name;
}

void QNetworkSettingsManagerPrivate::clearConnectionState() //clearing the old conneciton
{
    Q_Q(QNetworkSettingsManager);
    if(m_serviceModel->getByName(m_currentSsid)->type() == QNetworkSettingsType::Wifi){
        m_currentWifiConnection.clear();
        emit q->currentWifiConnectionChanged();
    }
    else if(m_serviceModel->getByName(m_currentSsid)->type() == QNetworkSettingsType::Wired){
        m_currentWiredConnection.clear();
        emit q->currentWiredConnectionChanged();
    }
    m_serviceModel->removeService(m_currentSsid);
    m_currentSsid.clear();
}

void QNetworkSettingsManagerPrivate::tryNextConnection()
{
    Q_Q(QNetworkSettingsManager);
    QNetworkSettingsService *service = nullptr;

    if(!m_currentSsid.isEmpty())
    {
        service = m_serviceModel->getByName(m_currentSsid); //getting the current ssid to establish a connection
        m_currentSsid.clear();  //clearing the current ssid
    }
    if (!service)
    {
        if (!m_unnamedServicesForSsidConnection.isEmpty())
        {
            service = m_unnamedServicesForSsidConnection.take(m_unnamedServicesForSsidConnection.firstKey());
        }
        else
        {
            q->clearConnectionState();
        }
    }

    if (service)
    {
        service->doConnectService();
    }
}

void QNetworkSettingsManagerPrivate::onTechnologyAdded() //this method gets called when the timerevent start so is always updated
{
    Q_Q(QNetworkSettingsManager);
    QJniEnvironment env;

    jclass networkInterface = env->FindClass("java/net/NetworkInterface");

    QJniObject interfaces = QJniObject::callStaticObjectMethod(networkInterface,"getNetworkInterfaces",
                                                               "()Ljava/util/Enumeration;");
    QStringList newInterfaces;
    while(interfaces.callMethod<jboolean>("hasMoreElements","()Z")) //to check if there are interfaces
    { //when it returns true there is another interface to check
        QJniObject interface = interfaces.callObjectMethod("nextElement","()Ljava/lang/Object;");

        QString name = interface.callObjectMethod("getName","()Ljava/lang/String;").toString();

        if(!interfaceExistence(name))
        { //if the name does not already exist in the model then i need to add a new interface
            AndroidSettingsInterface *newInterface = new AndroidSettingsInterface(name);
            m_interfaceModel.append(newInterface);
            m_androidInterface->interfaceChanged = true;
        }
        newInterfaces.append(name);
        m_androidInterface->updateProperty("Name",name);
        QNetworkSettingsType *type = new QNetworkSettingsType();
        type->setType(checkInterfaceType(m_androidInterface->interfaceName));
        m_androidInterface->updateProperty("Type",QVariant::fromValue(type)); //when updating the singleton class the interface is getting updated aswell
        m_androidInterface->updateProperty("Powered",true);
    }
    checkInterface(newInterfaces); //method to check if an old interface is not available anymore
    if(m_androidInterface->interfaceChanged == true)
    {
        m_androidInterface->interfaceChanged = false;
        emit q->interfacesChanged();
    }
}

bool QNetworkSettingsManagerPrivate::interfaceExistence(QString name)
{
    foreach (QNetworkSettingsInterface *oldIntertface, m_interfaceModel.getModel())
    {
        if(oldIntertface->name() == name)
        {
            return true; //returns true if the interface already exists
        }
    }
    return false; //return false if the interface does not exists
}

QNetworkSettingsType::Type QNetworkSettingsManagerPrivate::checkInterfaceType(QString name)
{ //android has multiple types but the global structure only has 4
    if(name.contains("wlan"))
    {
        return QNetworkSettingsType::Wifi;
    }
    else if(name.contains("eth") || name.contains("enp")) //for android eth is the mobile data connection
    {
        return QNetworkSettingsType::Wired;
    }
    else if(name.contains("rmnet"))
    {
        return QNetworkSettingsType::Bluetooth;
    }
    else
    {
        return QNetworkSettingsType::Unknown;
    }
}

//passing the entire list of interfaces so i can check both ways if theres a new interface or a old one does not exist anymore
void QNetworkSettingsManagerPrivate::checkInterface(QStringList newInterfaces)
{
    Q_Q(QNetworkSettingsManager);
    bool found = false;
    foreach(QNetworkSettingsInterface *oldInterface, m_interfaceModel.getModel())
    {
        foreach (QString newInterface, newInterfaces)
        {
            if(oldInterface->name() == newInterface) //old interface still exists
            {
                found = true;
            }
        }
        if(found == false) //if false i didnt find the old interface
        {
            m_interfaceModel.removeInterface(oldInterface->name()); //removing an interface that does not exist anymore
            m_androidInterface->interfaceChanged = true; //to notify the user
        }
        else
        {
            found = false; //need to set found as false for the next cycle
        }
    }
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
