#include "androidInterfaceDB.h"

QT_BEGIN_NAMESPACE

#define PropertyName QStringLiteral("Name")
#define PropertyType QStringLiteral("Type")
#define PropertyConnected QStringLiteral("Connected")
#define PropertyPowered QStringLiteral("Powered")

AndroidInterfaceDB *AndroidInterfaceDB::interfaceInstance = nullptr;

AndroidInterfaceDB::AndroidInterfaceDB()
{

}

void AndroidInterfaceDB::updateProperty(const QString &key, const QVariant &val)
{
    if(key == PropertyName)
    {
        interfaceName = val.toString();
    }
    if(key == PropertyType)
    {
        interfaceType.setType(val.value<QNetworkSettingsType *>()->type());
    }
    if(key == PropertyConnected)
    {
        interfaceState.setState(val.value<QNetworkSettingsState *>()->state());
    }
    if(key == PropertyPowered)
    {
        interfacePowered = val.toBool();
    }
}

AndroidInterfaceDB *AndroidInterfaceDB::getInstance()
{
    if(interfaceInstance == nullptr)
    {
        interfaceInstance = new AndroidInterfaceDB();
    }
    return interfaceInstance;
}

QT_END_NAMESPACE
