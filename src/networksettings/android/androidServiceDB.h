#ifndef ANDROIDDB_H
#define ANDROIDDB_H

#include <QObject>
#include <QNetworkSettingsIPv4>
#include <qnetworksettings.h>

QT_BEGIN_NAMESPACE

class AndroidServiceDB : public QObject
{
    Q_OBJECT
public:
    void setProperty(QString key,QVariant val);
    QString ensureMask(int prefixLength);
    static AndroidServiceDB* getInstance();
    AndroidServiceDB();

Q_SIGNALS:
    void servicePropertyChanged(QString key,QVariant value);

public:
    QString serviceName;
    QString serviceId;
    bool changes = false;
    QNetworkSettingsIPv4 serviceIpv4;
    QNetworkSettingsIPv6 serviceIpv6;
    QNetworkSettingsAddressModel serviceDNS;
    QNetworkSettingsAddressModel serviceNameServers;
    QNetworkSettingsProxy serviceProxy;
    QNetworkSettingsWireless serviceWireless;
    QNetworkSettingsType serviceType;
private:
    static AndroidServiceDB* androidInstance;
};

QT_END_NAMESPACE

#endif // ANDROIDDB_H
