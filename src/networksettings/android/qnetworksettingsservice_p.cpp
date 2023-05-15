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
    m_service = AndroidServiceDB::getInstance(); //retrieving the instance of AndroidDB that already exists

    connect(m_service,SIGNAL(servicePropertyChanged(QString,QVariant)),
                    this,SLOT(updateProperty(QString,QVariant)));
}

bool QNetworkSettingsServicePrivate::updateProperties(){
    return false;
}

void QNetworkSettingsServicePrivate::updateProperty(QString name,QVariant value){
    Q_Q(QNetworkSettingsService);
    //all the data are already inside androidServiceDB i only need to check if they changed from before and uptade in that case
    //need to check if the androidServiceDB id is the same as this service
    if(m_service->serviceId == m_id)
    {

        if(name == PropertyIPv4)
        {
            if(value.value<QNetworkSettingsIPv4 *>()->address() != m_ipv4config.address() ||
                value.value<QNetworkSettingsIPv4 *>()->mask() != m_ipv4config.mask() ||
                value.value<QNetworkSettingsIPv4 *>()->gateway() != m_ipv4config.gateway() ||
                value.value<QNetworkSettingsIPv4 *>()->method() != m_ipv4config.method())
            {
                m_ipv4config.setAddress(value.value<QNetworkSettingsIPv4 *>()->address());
                m_ipv4config.setGateway(value.value<QNetworkSettingsIPv4 *>()->gateway());
                m_ipv4config.setMask(value.value<QNetworkSettingsIPv4 *>()->mask());
                m_ipv4config.setMethod(value.value<QNetworkSettingsIPv4 *>()->method());
                m_service->changes = true;
                emit q->ipv4Changed();
            }
        }
        if(name == PropertyName)
        { //now the ssid is the same as the name because i need to find something to make a unique id for every connection
            if(value.toString() != m_name)
            {
                m_name = value.toString();
                m_service->changes = true;
                emit q->nameChanged();
            }
        }
        if(name == PropertyIPv6)
        { //ipv6 has a prefixLength and not a mask
            if(value.value<QNetworkSettingsIPv6 *>()->address() != m_ipv6config.address() ||
                value.value<QNetworkSettingsIPv6 *>()->prefixLength() != m_ipv6config.prefixLength() ||
                value.value<QNetworkSettingsIPv6 *>()->gateway() != m_ipv6config.gateway() ||
                value.value<QNetworkSettingsIPv6 *>()->method() != m_ipv6config.method())
            {
                m_ipv6config.setAddress(value.value<QNetworkSettingsIPv6 *>()->address());
                m_ipv6config.setGateway(value.value<QNetworkSettingsIPv6 *>()->gateway());
                m_ipv6config.setMethod(value.value<QNetworkSettingsIPv6 *>()->method());
                m_ipv6config.setPrefixLength(value.value<QNetworkSettingsIPv6 *>()->prefixLength());
                m_service->changes = true;
                emit q->ipv6Changed();
            }
        }
        if(name == PropertyProxy)
        { //updating proxy
            if(value.value<QNetworkSettingsProxy *>()->url() != m_proxyConfig.url())
            {
                m_proxyConfig.setUrl(value.value<QNetworkSettingsProxy *>()->url());
                m_service->changes = true;
                emit q->proxyChanged();
            }
            QStringList newList =(value.value<QNetworkSettingsProxy *>()->excludes())->index(0,0).data().toStringList();
            QStringList oldList = m_proxyConfig.excludes()->index(0,0).data().toStringList();
            if(stringlistCompare(oldList,newList))
            {
                m_proxyConfig.setExcludes(newList);
                m_service->changes = true;
                emit q->proxyChanged();
            }
        }
        if(name == PropertyDomains)
        { //uptading domains
            QStringList newList = value.value<QStringList>();
            QStringList oldList = m_domainsConfig.getAddresses();
            if(stringlistCompare(oldList,newList))
            {
                m_domainsConfig.setStringList(newList);
                m_service->changes = true;
                emit q->domainsChanged();
            }
        }
        if(name == PropertyType)
        { //updating type
            if(value.value<QNetworkSettingsType *>()->type() != m_type.type())
            {
                m_type.setType(value.value<QNetworkSettingsType *>()->type());
                m_service->changes = true;
                emit q->typeChanged();
            }
        }
        if(name == PropertyNameservers)
        { //updating name servers
            QStringList newList = value.value<QStringList>();
            QStringList oldList = m_nameserverConfig.getAddresses();
            if(stringlistCompare(oldList,newList))
            {
                m_nameserverConfig.setStringList(newList);
                m_service->changes = true;
                emit q->nameserversChanged();
            }
        }
        if(name == PropertyStrength)
        { //updating signal strength
            if(value.toInt() != m_wifiConfig.signalStrength())
            {
                m_wifiConfig.setSignalStrength(value.toInt());
                m_service->changes = true;
                emit q->wirelessChanged();
            }

        }
        if(name == PropertySecurity)
        { //android has 15 security types but the library is made for only 4
            if(m_service->serviceWireless.supportsSecurity(QNetworkSettingsWireless::Security::None) &&
                !m_wifiConfig.supportsSecurity(QNetworkSettingsWireless::Security::None))
            {
                m_wifiConfig.setSecurity(QNetworkSettingsWireless::Security::None);
                m_service->changes = true;
                emit q->wirelessChanged();
            }
            else if(m_service->serviceWireless.supportsSecurity(QNetworkSettingsWireless::Security::WEP) &&
                    !m_wifiConfig.supportsSecurity(QNetworkSettingsWireless::Security::WEP))
            {
                m_wifiConfig.setSecurity(QNetworkSettingsWireless::Security::WEP);
                m_service->changes = true;
                emit q->wirelessChanged();
            }
            else if((m_service->serviceWireless.supportsSecurity(QNetworkSettingsWireless::Security::WPA) ||
                      m_service->serviceWireless.supportsSecurity(QNetworkSettingsWireless::Security::WPA2))
                     &&
                     (!m_wifiConfig.supportsSecurity(QNetworkSettingsWireless::Security::WPA) ||
                      !m_wifiConfig.supportsSecurity(QNetworkSettingsWireless::Security::WPA2)))
            {
                m_wifiConfig.setSecurity(QNetworkSettingsWireless::Security::WPA | QNetworkSettingsWireless::Security::WPA2);
                m_service->changes = true;
                emit q->wirelessChanged();
            }
        }
    }
}

bool QNetworkSettingsServicePrivate::stringlistCompare(QStringList oldList,QStringList newList)
{ //function used for every QStringList in the infos because they all work the same
    Q_Q(QNetworkSettingsService);
    bool changed = false;
    if((newList.isEmpty() && !oldList.isEmpty()) || (!newList.isEmpty() && oldList.isEmpty()))
    {
        changed = true;
    }
    else
    {
        foreach(const QString &str, newList)
        {
            if(!oldList.contains(str))
            {
                changed = true;
            }
        }
        foreach(const QString &str, oldList)
        {
            if(!newList.contains(str))
            {
                changed = true;
            }
        }
    }
    return changed;
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
