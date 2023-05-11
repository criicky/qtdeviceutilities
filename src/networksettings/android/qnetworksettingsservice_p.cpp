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
#include "qnetworksettingsservice_p.h"
#include <QHostAddress>

#define PropertyName QStringLiteral("Name")
#define PropertyType QStringLiteral("Type")

QT_BEGIN_NAMESPACE

const QString PropertyIPv4(QStringLiteral("IPv4"));
const QString PropertyQNetworkSettingsIPv4(QStringLiteral("IPv4.Configuration"));
const QString PropertyIPv6(QStringLiteral("IPv6"));
const QString PropertyQNetworkSettingsIPv6(QStringLiteral("IPv6.Configuration"));
const QString PropertyNameservers(QStringLiteral("Nameservers"));
const QString PropertyNameserversConfig(QStringLiteral("Nameservers.Configuration"));
const QString PropertyDomains(QStringLiteral("Domains"));
const QString PropertyDomainsConfig(QStringLiteral("Domains.Configuration"));
const QString PropertyProxy(QStringLiteral("Proxy"));
const QString PropertyQNetworkSettingsProxy(QStringLiteral("Proxy.Configuration"));
const QString PropertyAddress(QStringLiteral("Address"));
const QString PropertyNetMask(QStringLiteral("Netmask"));
const QString PropertyGateway(QStringLiteral("Gateway"));
const QString PropertyPrefixLength(QStringLiteral("PrefixLength"));
const QString PropertyMethod(QStringLiteral("Method"));
const QString PropertyPrivacy(QStringLiteral("Privacy"));
const QString PropertyUrl(QStringLiteral("URL"));
const QString PropertyServers(QStringLiteral("Servers"));
const QString PropertyExcludes(QStringLiteral("Excludes"));
const QString PropertyStrength(QStringLiteral("Strength"));
const QString PropertySecurity(QStringLiteral("Security"));

const QString AttributeAuto(QStringLiteral("auto"));
const QString AttributeDhcp(QStringLiteral("dhcp"));
const QString AttributeManual(QStringLiteral("manual"));
const QString AttributeOff(QStringLiteral("off"));
const QString AttributeDisabled(QStringLiteral("disabled"));
const QString AttributeEnabled(QStringLiteral("enabled"));
const QString AttributePreferred(QStringLiteral("preferred"));
const QString AttributeDirect(QStringLiteral("direct"));
const QString AttributeNone(QStringLiteral("none"));
const QString AttributeWep(QStringLiteral("wep"));
const QString AttributePsk(QStringLiteral("psk"));
const QString AttributeIeee(QStringLiteral("ieee8021x"));
const QString AttributeWps(QStringLiteral("wps"));
const QString AttributeInvalidKey(QStringLiteral("invalid-key"));

QNetworkSettingsServicePrivate::QNetworkSettingsServicePrivate(const QString& id, QNetworkSettingsService *parent) :
    QObject(parent)
    ,q_ptr(parent)
    ,m_id(id)
{
}

bool QNetworkSettingsServicePrivate::updateProperties(){
    return false;
}

void QNetworkSettingsServicePrivate::propertyCall(QString key,QVariant val){
    Q_Q(QNetworkSettingsService);

    if(key == PropertyIPv4){
        m_ipv4config.setAddress(val.value<QNetworkSettingsIPv4 *>()->address());
        m_ipv4config.setGateway(val.value<QNetworkSettingsIPv4 *>()->gateway());
        m_ipv4config.setMask(QNetworkSettingsServicePrivate::ensureMask(val.value<QNetworkSettingsIPv4 *>()->mask().toInt()));
        m_ipv4config.setMethod(val.value<QNetworkSettingsIPv4 *>()->method());
    }
    if(key == PropertyName){ //salvo il nome uguale all'ssid
        m_name = val.toString();
    }
    if(key == PropertyIPv6){ //per l' ipv6 non c'è una variabile per la maschera
        m_ipv6config.setAddress(val.value<QNetworkSettingsIPv6 *>()->address());
        m_ipv6config.setGateway(val.value<QNetworkSettingsIPv6 *>()->gateway());
        m_ipv6config.setMethod(val.value<QNetworkSettingsIPv6 *>()->method());
        m_ipv6config.setPrefixLength(val.value<QNetworkSettingsIPv6 *>()->prefixLength());
    }
    if(key == PropertyProxy){
        m_proxyConfig.setUrl(val.value<QNetworkSettingsProxy *>()->url());
        QStringList lista =(val.value<QNetworkSettingsProxy *>()->excludes())->index(0,0).data().toStringList();
        /*foreach(const QString &item,lista){
            qDebug() << "escluso" << item;
        }*/
        m_proxyConfig.setExcludes(lista);
    }
    if(key == PropertyDomains){
        m_domainsConfig.setStringList(val.value<QStringList>());
    }
    if(key == PropertyType){
        m_type.setType(val.value<QNetworkSettingsType *>()->type());
    }
    if(key == PropertyNameservers){
        m_nameserverConfig.setStringList(val.value<QStringList>());
    }
    if(key == PropertyStrength){
        m_wifiConfig.setSignalStrength(val.toInt());
    }
    if(key == PropertySecurity){ //devo assegnare il valore di enum a seconda dei vari tipi di
        switch(val.toInt()){
        case -1:
            m_wifiConfig.setSecurity(QNetworkSettingsWireless::Security::None);
        case 1:
            m_wifiConfig.setSecurity(QNetworkSettingsWireless::Security::WEP);
        default:
            m_wifiConfig.setSecurity(QNetworkSettingsWireless::Security::WPA | QNetworkSettingsWireless::Security::WPA2);
        break;
        }
    }
}

QString QNetworkSettingsServicePrivate::ensureMask(int prefixLength)
{
    int mask = 0xFFFFFFFF;
    mask <<= (32 - prefixLength);

    QString subnetMask;
    for (int i = 0; i < 4; ++i) {
        subnetMask += QString::number((mask >> (8 * (3 - i))) & 0xFF);
        if (i < 3) {
            subnetMask += ".";
        }
    }
    return subnetMask;
}

void QNetworkSettingsServicePrivate::setAutoConnect(bool autoconnect)
{
}

bool QNetworkSettingsServicePrivate::autoConnect() const
{
    return false;
}

void QNetworkSettingsServicePrivate::setupIpv6Config()
{
}

void QNetworkSettingsServicePrivate::setupNameserversConfig()
{
}

void QNetworkSettingsServicePrivate::setupDomainsConfig()
{
}

void QNetworkSettingsServicePrivate::setupQNetworkSettingsProxy()
{
}

void QNetworkSettingsServicePrivate::setupIpv4Config()
{

}

void QNetworkSettingsServicePrivate::connectService()
{
}

void QNetworkSettingsServicePrivate::disconnectService()
{
}

void QNetworkSettingsServicePrivate::removeService()
{
    qWarning() << "Test";
}

void QNetworkSettingsServicePrivate::setPlaceholderState(bool placeholderState)
{
}

bool QNetworkSettingsServicePrivate::placeholderState() const
{
    return false;
}

QT_END_NAMESPACE
