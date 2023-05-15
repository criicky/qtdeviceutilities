#include "androidServiceDB.h"

#define PropertyName QStringLiteral("Name")
#define PropertyType QStringLiteral("Type")

QT_BEGIN_NAMESPACE

const QString PropertyIPv4(QStringLiteral("IPv4"));
const QString PropertyIPv6(QStringLiteral("IPv6"));
const QString PropertyNameservers(QStringLiteral("Nameservers"));
const QString PropertyDomains(QStringLiteral("Domains"));
const QString PropertyProxy(QStringLiteral("Proxy"));
const QString PropertyStrength(QStringLiteral("Strength"));
const QString PropertySecurity(QStringLiteral("Security"));

AndroidServiceDB* AndroidServiceDB::androidInstance = nullptr;

AndroidServiceDB::AndroidServiceDB()
{
}

void AndroidServiceDB::setProperty(QString key,QVariant val) //this class is used to encapsulate data and send them to the corresponding service
{
    if(key == "ID")
    {
        serviceId = val.toString();
    }
    if(key == PropertyIPv4){ //mask is made by using a method
        serviceIpv4.setAddress(val.value<QNetworkSettingsIPv4 *>()->address());
        serviceIpv4.setGateway(val.value<QNetworkSettingsIPv4 *>()->gateway());
        serviceIpv4.setMask(AndroidServiceDB::ensureMask(val.value<QNetworkSettingsIPv4 *>()->mask().toInt()));
        serviceIpv4.setMethod(val.value<QNetworkSettingsIPv4 *>()->method());
        QVariant variant;
        variant.setValue(&serviceIpv4);
        emit servicePropertyChanged("IPv4",variant);
    }
    if(key == PropertyName)
    { //now the ssid is the same as the name because i need to find something to make a unique id for every connection
        serviceName = val.toString();
        emit servicePropertyChanged("Name",val);
    }
    if(key == PropertyIPv6)
    { //ipv6 has a prefixLength and not a mask
        serviceIpv6.setAddress(val.value<QNetworkSettingsIPv6 *>()->address());
        serviceIpv6.setGateway(val.value<QNetworkSettingsIPv6 *>()->gateway());
        serviceIpv6.setMethod(val.value<QNetworkSettingsIPv6 *>()->method());
        serviceIpv6.setPrefixLength(val.value<QNetworkSettingsIPv6 *>()->prefixLength());
        emit servicePropertyChanged("IPv6",val);
    }
    if(key == PropertyProxy)
    {
        serviceProxy.setUrl(val.value<QNetworkSettingsProxy *>()->url());
        QStringList list =(val.value<QNetworkSettingsProxy *>()->excludes())->index(0,0).data().toStringList();
        serviceProxy.setExcludes(list);
        emit servicePropertyChanged("Proxy",val);
    }
    if(key == PropertyDomains)
    {
        serviceDNS.setStringList(val.value<QStringList>());
        emit servicePropertyChanged("Domains",val);
    }
    if(key == PropertyType)
    {
        serviceType.setType(val.value<QNetworkSettingsType *>()->type());
        emit servicePropertyChanged("Type",val);
    }
    if(key == PropertyNameservers)
    {
        serviceNameServers.setStringList(val.value<QStringList>());
        emit servicePropertyChanged("Nameservers",val);

    }
    if(key == PropertyStrength)
    {
        serviceWireless.setSignalStrength(val.toInt());
        emit servicePropertyChanged("Strength",val);
    }
    if(key == PropertySecurity)
    { //android has 15 security types but the library is made for only 4
        switch(val.toInt())
        {
        case -1:
            serviceWireless.setSecurity(QNetworkSettingsWireless::Security::None);
        case 1:
            serviceWireless.setSecurity(QNetworkSettingsWireless::Security::WEP);
        default:
            serviceWireless.setSecurity(QNetworkSettingsWireless::Security::WPA | QNetworkSettingsWireless::Security::WPA2);
            break;
        }
        emit servicePropertyChanged("Security",val);
    }
}

QString AndroidServiceDB::ensureMask(int prefixLength) //function to generate the mask from its prefixLength
{
    int mask = 0xFFFFFFFF;
    mask <<= (32 - prefixLength);

    QString subnetMask;
    for (int i = 0; i < 4; ++i)
    {
        subnetMask += QString::number((mask >> (8 * (3 - i))) & 0xFF);
        if (i < 3)
        {
            subnetMask += ".";
        }
    }
    return subnetMask;
}

AndroidServiceDB* AndroidServiceDB::getInstance() //singleton class so i need to return its instance
{
    if(androidInstance == nullptr)
    {
        androidInstance = new AndroidServiceDB();
    }
    return androidInstance;
}

QT_END_NAMESPACE
