#ifndef I2CSETTINGS_H
#define I2CSETTINGS_H

#include <QObject>
#include <QString>

#include "xmlsettings.h"


namespace i2cSettings
{
enum member
{
    master,
    atmel,
    flashmux,
    flash,
    clampflash
};

enum configstate
{
    SetDevNode,
    SetMasterAdr,
    SetAtmelAdr,
    SetFlashMuxAdr,
    SetFlashAdr,
    SetClampFlashAdr
};
}


namespace Zera
{
namespace XMLConfig
{
    class cReader;
}
}


class cI2CSettings : public cXMLSettings
{
    Q_OBJECT

public:
    cI2CSettings(Zera::XMLConfig::cReader *xmlread);
    quint8 getI2CAdress(i2cSettings::member member);
    QString& getDeviceNode();

public slots:
    virtual void configXMLInfo(QString key);

private:
    QString m_sDeviceNode;
    quint8 m_nMasterAdr, m_nAtmelAdr, m_nFlashMuxAdr, m_nFlashAdr, m_nClampFlashAdr;
};


#endif // I2CSETTINGS_H
