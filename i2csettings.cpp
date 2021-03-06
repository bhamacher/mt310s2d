#include <QVariant>
#include <xmlconfigreader.h>

#include "mt310s2dglobal.h"
#include "i2csettings.h"


cI2CSettings::cI2CSettings(Zera::XMLConfig::cReader *xmlread)
{
    m_pXMLReader = xmlread;
    m_ConfigXMLMap["mt310s2dconfig:connectivity:i2c:device:node"] = i2cSettings::SetDevNode;
    m_ConfigXMLMap["mt310s2dconfig:connectivity:i2c:adress:master"] = i2cSettings::SetMasterAdr;
    m_ConfigXMLMap["mt310s2dconfig:connectivity:i2c:adress:atmel"] = i2cSettings::SetAtmelAdr;
    m_ConfigXMLMap["mt310s2dconfig:connectivity:i2c:adress:clampmux"] = i2cSettings::SetFlashMuxAdr;
    m_ConfigXMLMap["mt310s2dconfig:connectivity:i2c:adress:flash"] = i2cSettings::SetFlashAdr;
    m_ConfigXMLMap["mt310s2dconfig:connectivity:i2c:adress:clampflash"] = i2cSettings::SetClampFlashAdr;
    m_sDeviceNode = defaultI2CDeviceNode;
    m_nMasterAdr = defaultI2CMasterAdress;
    m_nAtmelAdr = defaultI2CAtmelAdress;
    m_nFlashMuxAdr = defaultI2CFlashMuxAdress;
    m_nFlashAdr = defaultI2CFlashAdress;
    m_nClampFlashAdr = defaultI2CClampFlashAdr;
}


quint8 cI2CSettings::getI2CAdress(i2cSettings::member member)
{
    quint8 r;

    switch (member)
    {
    case i2cSettings::master:
        r = m_nMasterAdr;
        break;
    case i2cSettings::atmel:
        r = m_nAtmelAdr;
        break;
    case i2cSettings::flashmux:
        r = m_nFlashMuxAdr;
        break;
    case i2cSettings::flash:
        r = m_nFlashAdr;
        break;
    case i2cSettings::clampflash:
        r = m_nClampFlashAdr;
        break;
    }

    return r;
}


QString& cI2CSettings::getDeviceNode()
{
    return m_sDeviceNode;
}


void cI2CSettings::configXMLInfo(QString key)
{
    bool ok;

    if (m_ConfigXMLMap.contains(key))
    {
        switch (m_ConfigXMLMap[key])
        {
        case i2cSettings::SetDevNode:
            m_sDeviceNode = m_pXMLReader->getValue(key);
            break;
        case i2cSettings::SetMasterAdr:
            m_nMasterAdr = m_pXMLReader->getValue(key).toInt(&ok);
            break;
        case i2cSettings::SetAtmelAdr:
            m_nAtmelAdr = m_pXMLReader->getValue(key).toInt(&ok);
            break;
        case i2cSettings::SetFlashMuxAdr:
            m_nFlashMuxAdr = m_pXMLReader->getValue(key).toInt(&ok);
            break;
        case i2cSettings::SetFlashAdr:
            m_nFlashAdr = m_pXMLReader->getValue(key).toInt(&ok);
            break;
        case i2cSettings::SetClampFlashAdr:
            m_nClampFlashAdr = m_pXMLReader->getValue(key).toInt(&ok);
            break;
        }
    }
}
