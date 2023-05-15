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
#ifndef QNETWORKSETTINGSMANAGERPRIVATE_H
#define QNETWORKSETTINGSMANAGERPRIVATE_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists for the convenience
// of other Qt classes.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QObject>
#include <QJniObject>
#include <QPointer>
#include "qnetworksettingsmanager.h"
#include "qnetworksettingsinterfacemodel.h"
#include "QNetworkSettingsType"
#include "androidServiceDB.h"
#include "androidInterfaceDB.h"

QT_BEGIN_NAMESPACE

class QNetworkSettingsService;
class QNetworkSettingsServiceModel;
class QNetworkSettingsServiceFilter;

class QNetworkSettingsManagerPrivate : public QObject
{
    Q_OBJECT
    Q_DECLARE_PUBLIC(QNetworkSettingsManager)
public:
    explicit QNetworkSettingsManagerPrivate(QNetworkSettingsManager *parent);
    QNetworkSettingsManager *q_ptr;
    void setUserAgent(QNetworkSettingsUserAgent *agent);
    QNetworkSettingsUserAgent *userAgent() const {return m_agent;}
    QNetworkSettingsInterfaceModel* interfaceModel() {return &m_interfaceModel;}
    QNetworkSettingsServiceModel* serviceModel() const {return m_serviceModel;}
    QNetworkSettingsServiceFilter* serviceFilter() const {return m_serviceFilter;}
    void connectBySsid(const QString &name);
    void clearConnectionState();
    void tryNextConnection();
    void setCurrentWifiConnection(QNetworkSettingsService *connection);
    QNetworkSettingsService* currentWifiConnection() const;
    void setCurrentWiredConnection(QNetworkSettingsService *connection);
    QNetworkSettingsService* currentWiredConnection() const;
    void test();
    bool constructor();
    void updateServices();
    bool checkExistence(QString ssid);
    QString formattedSSID(QString ssid);
    void dnsConfig(QNetworkSettingsService *oldService,QNetworkSettingsService *newService);
    void proxyConfig(QNetworkSettingsService *oldService,QNetworkSettingsService *newService);
    QString savingGateway(QJniObject linkProperties,QString key);
    void wirelessConfig(QNetworkSettingsService *oldService,QNetworkSettingsService *newService);
    void updateProperties(QNetworkSettingsService *oldService,QNetworkSettingsService *newService);
    bool changes(QNetworkSettingsService *oldService,QNetworkSettingsService *newService);
    void checkInterface(QList<QNetworkSettingsInterface *> interfacesList);
    QNetworkSettingsType::Type checkInterfaceType(QString name);
    bool checkProperties(QNetworkSettingsInterface *oldInterface,QNetworkSettingsInterface *newInterface);

public slots:
    void requestInput(const QString& service, const QString& type);
    void onTechnologyAdded();
    void serviceReady();

private:
    bool initialize();
    void onServicesChanged();
    void handleNewService(const QString &servicePath);
public:

protected:
    QNetworkSettingsInterfaceModel m_interfaceModel;
    QNetworkSettingsServiceFilter *m_serviceFilter;
    QMap<QString, QNetworkSettingsService*> m_unknownServices;
    QMap<QString, QNetworkSettingsService*> m_unnamedServices;
    QMap<QString, QNetworkSettingsService*> m_unnamedServicesForSsidConnection;
    void timerEvent(QTimerEvent *event) override;
private:
    QJniObject m_context;
    QJniObject m_connectivitymanager;
    QJniObject m_wifimanager;
    QNetworkSettingsUserAgent *m_agent;
    QString m_currentSsid;
    QPointer<QNetworkSettingsService> m_currentWifiConnection;
    QPointer<QNetworkSettingsService> m_currentWiredConnection;
    bool m_initialized;
    QNetworkSettingsServiceModel *m_serviceModel;
    bool m_changes;
    AndroidServiceDB *m_androidService;
    AndroidInterfaceDB *m_androidInterface;

Q_SIGNALS:
    void priorityCall(QString key,QVariant val);
};

QT_END_NAMESPACE

#endif // QNETWORKSETTINGSMANAGERPRIVATE_H
