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
#include "qnetworksettingsinterface_p.h"

#define PropertyName QStringLiteral("Name")
#define PropertyType QStringLiteral("Type")
#define PropertyConnected QStringLiteral("Connected")
#define PropertyPowered QStringLiteral("Powered")

QT_BEGIN_NAMESPACE

QNetworkSettingsInterfacePrivate::QNetworkSettingsInterfacePrivate(QNetworkSettingsInterface* parent)
    : QObject(parent)
    ,q_ptr(parent)
{ //setting all the variables
    m_powered = false;
    m_state.setState(QNetworkSettingsState::State::Ready);
}

void QNetworkSettingsInterfacePrivate::updateProperty(const QString &key, const QVariant &val)
{ //this method gets activated from the singleton class to check if there was an update or not
    Q_Q(QNetworkSettingsInterface);
    if(m_technology->interfaceName == m_name)
    {
        if(key == PropertyType && (m_technology->interfaceType.type() != m_type.type()))
        {
            m_technology->interfaceChanged = true;
            m_type.setType(val.value<QNetworkSettingsType *>()->type());
            emit q->typeChanged(); //need to send a signal if the type changed
        }
        if(key == PropertyConnected && (m_technology->interfaceState.state() != m_state.state()))
        {
            m_technology->interfaceChanged = true;
            m_state.setState(val.value<QNetworkSettingsState *>()->state());
            emit q->stateChanged(); //need to send a signal if the state changed
        }
        if(key == PropertyPowered && (m_technology->interfacePowered != m_powered))
        {
            m_technology->interfaceChanged = true;
            m_powered = val.toBool();
            emit q->poweredChanged(); //need to send a signal if the power changed
        }
    }
}

void QNetworkSettingsInterfacePrivate::initialize(const QString& name)
{
    m_name = name;
    m_technology = AndroidInterfaceDB::getInstance(); //retreiving the instance of the singleton class

    connect(m_technology,SIGNAL(interfacePropertyChanged(QString,QVariant)),
            this,SLOT(updateProperty(QString,QVariant))); //i need to connect the interface to the singleton just to get signals from it
}

void QNetworkSettingsInterfacePrivate::setState(QNetworkSettingsState::State aState)
{
    Q_Q(QNetworkSettingsInterface);
    m_state.setState(aState);
    emit q->stateChanged();
}

void QNetworkSettingsInterfacePrivate::setPowered(const bool aPowered)
{
    m_powered = aPowered;
}

void QNetworkSettingsInterfacePrivate::scan() //do not need this because the private manager already does it
{
}


QT_END_NAMESPACE
