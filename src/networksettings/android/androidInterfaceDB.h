#ifndef ANDROIDINTERFACEDB_H
#define ANDROIDINTERFACEDB_H

#include <QObject>
#include <qnetworksettings.h>

QT_BEGIN_NAMESPACE

class AndroidInterfaceDB : public QObject
{
    Q_OBJECT
public:
    void updateProperty(const QString &key, const QVariant &val);
    AndroidInterfaceDB();
    static AndroidInterfaceDB *getInstance();
Q_SIGNALS:
    void interfacePropertyChanged(QString key,QVariant value);
public:
    QString interfaceName;
    QNetworkSettingsType interfaceType;
    QNetworkSettingsState interfaceState;
    bool interfacePowered;
    bool interfaceChanged;
private:
    static AndroidInterfaceDB *interfaceInstance;
};

QT_END_NAMESPACE

#endif // ANDROIDINTERFACEDB_H
