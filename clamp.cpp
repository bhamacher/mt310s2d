#include <QDateTime>
#include <QByteArray>
#include <QDataStream>
#include <QDomDocument>
#include <QDomElement>
#include <QBuffer>
#include <syslog.h>

#include "clamp.h"
#include "adjustment.h"
#include "justdata.h"
#include "mt310s2d.h"
#include "mt310s2dglobal.h"
#include "systeminterface.h"
#include "i2csettings.h"
#include "debugsettings.h"
#include "senseinterface.h"
#include "senserange.h"
#include "clampjustdata.h"
#include "protonetcommand.h"

cClamp::cClamp(cMT310S2dServer *server, QString channelName)
    :cAdjFlash(server->m_pI2CSettings->getDeviceNode(), server->m_pDebugSettings->getDebugLevel(), server->m_pI2CSettings->getI2CAdress(i2cSettings::clampflash)), m_pMyServer(server), m_sChannelName(channelName)
{
    m_pSCPIInterface = m_pMyServer->getSCPIInterface();

    m_bSet = false;
    m_sSerial = "1234567890"; // our default serial number
    m_sName = "unknown";
    m_sVersion = "unknown";
    m_nFlags = 0;
    m_nType = 0;

    addSystAdjInterface(); // we have an interface at once after clamp was connected

    quint8 type;
    if ((type = readClampType()) != undefined) // we try to read the clamp's type
    {
        m_nType = type;
        initClamp(m_nType); // and if it's a well known type we init the clamp
        importAdjFlash();
        addSense();
        addSenseInterface();
        m_bSet = true;
    }
}


cClamp::~cClamp()
{
    // first we remove the ranges from the sense interface
    m_pMyServer->m_pSenseInterface->getChannel(m_sChannelName)->removeRangeList(m_RangeList);

    // then we delete all our ranges including their scpi interface
    for (int i = 0; i < m_RangeList.count(); i++)
    {
        cSenseRange* ptr;
        ptr = m_RangeList.at(i);
        delete ptr; // the cSenseRange objects will also remove their interfaces including that for adjustment data
    }

}


void cClamp::initSCPIConnection(QString)
{
}


void cClamp::executeCommand(int cmdCode, cProtonetCommand *protoCmd)
{
    switch (cmdCode)
    {
    case clamp::cmdSerial:
        protoCmd->m_sOutput = m_ReadWriteSerial(protoCmd->m_sInput);
        break;
    case clamp::cmdVersion:
        protoCmd->m_sOutput = m_ReadWriteVersion(protoCmd->m_sInput);
        break;
    case clamp::cmdType:
        protoCmd->m_sOutput = m_ReadWriteType(protoCmd->m_sInput);
        break;
    case clamp::cmdName:
        protoCmd->m_sOutput = m_ReadWriteName(protoCmd->m_sInput);
        break;
    case clamp::cmdFlashWrite:
        protoCmd->m_sOutput = m_WriteFlash(protoCmd->m_sInput);
        break;
    case clamp::cmdFlashRead:
        protoCmd->m_sOutput = m_ReadFlash(protoCmd->m_sInput);
        break;
    case clamp::cmdChksum:
        protoCmd->m_sOutput = m_ReadChksum(protoCmd->m_sInput);
        break;
    case clamp::cmdStatAdjustment:
        protoCmd->m_sOutput = m_ReadAdjStatus(protoCmd->m_sInput);
        break;
    }

    if (protoCmd->m_bwithOutput)
        emit cmdExecutionDone(protoCmd);
}


void cClamp::exportAdjData(QDataStream &stream)
{
    QDateTime DateTime;

    mDateTime = DateTime.currentDateTime();

    stream << m_nType;
    stream << m_nFlags;
    stream << m_sName.toLatin1(); // the clamp's name
    stream << m_sVersion.toLatin1(); // version
    stream << m_sSerial.toLatin1(); //  serial
    stream << mDateTime.toString(Qt::TextDate).toLatin1(); // date, time

    for (int i = 0; i < m_RangeList.count(); i++)
    {
        QString spec;

        spec = QString("%1").arg(m_RangeList.at(i)->getName());
        stream << spec.toLatin1();
        m_RangeList.at(i)->getJustData()->Serialize(stream);
    }
}


bool cClamp::importAdjData(QDataStream &stream)
{
    int n;
    QString dts;

    n = 0;

    stream >> m_nType;
    stream >> m_nFlags;
    stream >> m_sName;
    stream >> m_sVersion;
    stream >> m_sSerial;
    stream >> dts;

    mDateTime = QDateTime::fromString(dts);

    while (!stream.atEnd())
    {
        QString rngName;
        stream >> rngName;
        cSenseRange* range = getRange(rngName);
        if (range != 0)
        {
            n++;
            range->getJustData()->Deserialize(stream);
        }
        else
        {
            cMT310S2JustData *dummy = new cMT310S2JustData(m_pSCPIInterface); // if we did not find this range....something has changed
            dummy->Deserialize(stream); // we read the data from stream to keep it in flow
            delete dummy;
        }
    }

    return (n == m_RangeList.count()); // it's ok if we found data for all ranges in our list
}


QString cClamp::exportXMLString()
{
    QDateTime DateTime;

    QString s = QString("%1AdjustmentData").arg(m_sName);
    QDomDocument justqdom (s);

    QDomElement pcbtag = justqdom.createElement( "CLAMP" );
    justqdom.appendChild( pcbtag );

    QDomElement tag = justqdom.createElement( "Type" );
    pcbtag.appendChild( tag );
    QDomText t = justqdom.createTextNode(QString("%1").arg(m_nType));
    tag.appendChild( t );

    tag = justqdom.createElement( "VersionNumber" );
    pcbtag.appendChild( tag );
    t = justqdom.createTextNode(m_sVersion);
    tag.appendChild( t );

    tag = justqdom.createElement( "SerialNumber" );
    pcbtag.appendChild( tag );
    t = justqdom.createTextNode(m_sSerial);
    tag.appendChild( t );

    tag = justqdom.createElement( "Date" );
    pcbtag.appendChild( tag );
    QDate d=DateTime.currentDateTime().date();
    t = justqdom.createTextNode(d.toString(Qt::TextDate));
    tag.appendChild( t );

    tag = justqdom.createElement( "Time" );
    pcbtag.appendChild( tag );
    QTime ti=DateTime.currentDateTime().time();
    t = justqdom.createTextNode(ti.toString(Qt::TextDate));
    tag.appendChild( t );

    QDomElement adjtag = justqdom.createElement( "Adjustment" );
    pcbtag.appendChild( adjtag );

    QDomElement chksumtag = justqdom.createElement("Chksum");
    adjtag.appendChild(chksumtag);
    t = justqdom.createTextNode(QString("0x%1").arg(m_nChecksum,0,16));
    chksumtag.appendChild(t);

    QDomElement typeTag = justqdom.createElement( "Sense");
    adjtag.appendChild(typeTag);

    for (int j = 0; j < m_RangeList.count(); j++)
    {
        cSenseRange* rng = m_RangeList.at(j);

        QDomElement rtag = justqdom.createElement( "Range" );
        typeTag.appendChild( rtag );

        QDomElement nametag = justqdom.createElement( "Name" );
        rtag.appendChild(nametag);

        t = justqdom.createTextNode(rng->getName());
        nametag.appendChild( t );

        QDomElement gpotag = justqdom.createElement( "Gain" );
        rtag.appendChild(gpotag);
        QDomElement tag = justqdom.createElement( "Status" );
        QString jdata = rng->getJustData()->m_pGainCorrection->SerializeStatus();
        t = justqdom.createTextNode(jdata);
        gpotag.appendChild(tag);
        tag.appendChild(t);
        tag = justqdom.createElement( "Coefficients" );
        gpotag.appendChild(tag);
        jdata = rng->getJustData()->m_pGainCorrection->SerializeCoefficients();
        t = justqdom.createTextNode(jdata);
        tag.appendChild(t);
        tag = justqdom.createElement( "Nodes" );
        gpotag.appendChild(tag);
        jdata = rng->getJustData()->m_pGainCorrection->SerializeNodes();
        t = justqdom.createTextNode(jdata);
        tag.appendChild(t);

        gpotag = justqdom.createElement( "Phase" );
        rtag.appendChild(gpotag);
        tag = justqdom.createElement( "Status" );
        jdata = rng->getJustData()->m_pPhaseCorrection->SerializeStatus();
        t = justqdom.createTextNode(jdata);
        tag.appendChild(t);
        gpotag.appendChild(tag);
        tag = justqdom.createElement( "Coefficients" );
        gpotag.appendChild(tag);
        jdata = rng->getJustData()->m_pPhaseCorrection->SerializeCoefficients();
        t = justqdom.createTextNode(jdata);
        tag.appendChild(t);
        tag = justqdom.createElement( "Nodes" );
        gpotag.appendChild(tag);
        jdata = rng->getJustData()->m_pPhaseCorrection->SerializeNodes();
        t = justqdom.createTextNode(jdata);
        tag.appendChild(t);

        gpotag = justqdom.createElement( "Offset" );
        rtag.appendChild(gpotag);
        tag = justqdom.createElement( "Coefficients" );
        gpotag.appendChild(tag);
        jdata = rng->getJustData()->m_pOffsetCorrection->SerializeCoefficients();
        t = justqdom.createTextNode(jdata);
        tag.appendChild(t);
        tag = justqdom.createElement( "Nodes" );
        gpotag.appendChild(tag);
        jdata = rng->getJustData()->m_pOffsetCorrection->SerializeNodes();
        t = justqdom.createTextNode(jdata);
        tag.appendChild(t);
    }

    return justqdom.toString();
}


bool cClamp::importXMLDocument(QDomDocument *qdomdoc)
{
    QDateTime DateTime;
    QDomDocumentType TheDocType = qdomdoc->doctype ();

    if  (TheDocType.name() != QString("%1AdjustmentData").arg(m_sName))
    {
        if DEBUG1 syslog(LOG_ERR,"justdata import, wrong xml documentype\n");
        return false;
    }

    QDomElement rootElem = qdomdoc->documentElement();
    QDomNodeList nl = rootElem.childNodes();

    bool TypeOK = false;
    bool VersionNrOK = false;
    bool SerialNrOK = false;
    bool DateOK = false;
    bool TimeOK = false;

    for (int i = 0; i < nl.length() ; i++)
    {
        QDomNode qdNode = nl.item(i);
        QDomElement qdElem = qdNode.toElement();
        if ( qdElem.isNull() )
        {
            if DEBUG1 syslog(LOG_ERR,"justdata import, format error in xml file\n");
            return false;
        }

        QString tName = qdElem.tagName();

        if (tName == "Type")
        {
            if ( !(TypeOK = (qdElem.text().toInt() == m_nType)))
            {
                if DEBUG1 syslog(LOG_ERR,"justdata import, wrong type information in xml file\n");
                return false;
            }
        }

        else

        if (tName == "SerialNumber")
        {
            SerialNrOK = true;
            m_sSerial = qdElem.text();
        }

        else

        if (tName == "VersionNumber")
        {
           VersionNrOK = true;
           m_sVersion = qdElem.text();
        }

        else

        if (tName=="Date")
        {
            QDate d = QDate::fromString(qdElem.text(),Qt::TextDate);
            DateTime.setDate(d);
            DateOK = true;
        }

        else

        if (tName=="Time")
        {
            QTime t = QTime::fromString(qdElem.text(),Qt::TextDate);
            DateTime.setTime(t);
            TimeOK = true;
        }

        else

        if (tName == "Adjustment")
        {
            if ( TypeOK && VersionNrOK && SerialNrOK && DateOK && TimeOK)
            {
                bool done = false;

                QDomNodeList adjChildNl = qdElem.childNodes();
                for (qint32 j = 0; j < adjChildNl.length(); j++)
                {
                    qdNode = adjChildNl.item(j);

                    qDebug() << qdNode.toElement().tagName();
                    if (qdNode.toElement().tagName() == "Sense") // we look for the sense entry
                    {
                        done = true;

                        QDomNodeList sensNl = qdNode.childNodes(); // we iterate over all ranges
                        for (qint32 j = 0; j < sensNl.length(); j++)
                        {
                            cSenseRange* rngPtr;
                            QString Name;

                            QDomNode RangeJustNode = sensNl.item(j);
                            qdElem = RangeJustNode.toElement();
                            QString tName = qdElem.tagName();
                            qDebug() << tName;

                            if (tName == "Range")
                            {
                                QDomNodeList rngJustNl = RangeJustNode.childNodes();
                                for (qint32 k = 0; k < rngJustNl.length(); k++)
                                {
                                    QDomNode RangeJustNode = rngJustNl.item(k);

                                    qdElem = RangeJustNode.toElement();
                                    tName = qdElem.tagName();
                                    qDebug() << tName;

                                    if (tName == "Name")
                                    {
                                        Name = qdElem.text();
                                        qDebug() << Name;
                                        rngPtr = getRange(Name);
                                    }

                                    cJustData* pJustData = 0;

                                    if (rngPtr != 0)
                                    {
                                        if (tName == "Gain")
                                            pJustData = rngPtr->getJustData()->m_pGainCorrection;

                                        if (tName == "Phase")
                                            pJustData = rngPtr->getJustData()->m_pPhaseCorrection;

                                        if (tName == "Offset")
                                            pJustData = rngPtr->getJustData()->m_pOffsetCorrection;
                                    }

                                    if (pJustData)
                                    {
                                        QDomNodeList jdataNl = RangeJustNode.childNodes();
                                        for (qint32 k = 0; k < jdataNl.count(); k++)
                                        {
                                            QDomNode jTypeNode = jdataNl.item(k);
                                            QString jTypeName = jTypeNode.toElement().tagName();
                                            QString jdata = jTypeNode.toElement().text();

                                            if (jTypeName == "Status")
                                                pJustData->DeserializeStatus(jdata);

                                            if (jTypeName == "Coefficients")
                                                pJustData->DeserializeCoefficients(jdata);

                                            if (jTypeName == "Nodes")
                                                pJustData->DeserializeNodes(jdata);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                if (!done)
                    return done;
            }
            else
            {
                if DEBUG1 syslog(LOG_ERR,"justdata import, xml file contains strange data\n");
                return false;
            }
        }
        else
        {
            if DEBUG1 syslog(LOG_ERR,"justdata import, xml file contains strange data\n");
            return false;
        }
    }

    return true;
}



void cClamp::setI2CMux()
{

}


quint8 cClamp::getAdjustmentStatus()
{
    quint8 stat = 255;
    for (int i = 0; i < m_RangeList.count(); i++)
        stat &= m_RangeList.at(i)->getAdjustmentStatus();

    if ((stat & JustData::Justified)== 0)
        return Adjustment::notAdjusted;
    else
        return Adjustment::adjusted;
}


quint8 cClamp::readClampType()
{
    QByteArray ba;
    if (readFlash(ba)) // flash data could be read with correct chksum
    {
        quint8 type;
        QDataStream stream(&ba, QIODevice::ReadWrite);
        stream.setVersion(QDataStream::Qt_5_4);
        stream.skipRawData(6);
        stream >> type;
        return type;
    }
    else
        return 0;
}


void cClamp::initClamp(quint8 type)
{
    cClampJustData* clampJustData;

    switch (type)
    {
    case CL120A:
        m_sName = QString("CL120A");

        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("1V")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,  "C100A",  "C100A", true, 100.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("500mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,   "C50A",   "C50A", true,  50.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("100mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,   "C10A",   "C10A", true,  10.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("50mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,    "C5A",    "C5A", true,   5.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("10mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,    "C1A",    "C1A", true,   1.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("5mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface, "C500mA", "C500mA", true,   0.5, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("2mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface, "C100mA", "C100mA", true,   0.1, 1785096.0, 2177818.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("2mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,  "C50mA",  "C50mA", true,  0.05,  892548.0, 1088909.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("2mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,  "C10mA",  "C10mA", true,  0.01,  178510.0,  217783.0, 0, SenseRange::Phys, clampJustData));

        break;

    case CL300A:
        m_sName = QString("CL300A");

        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("1V")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,  "C300A",  "C300A", true, 300.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("500mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,  "C150A",  "C150A", true, 150.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("100mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,   "C30A",   "C30A", true,  30.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("50mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,   "C15A",   "C15A", true,  15.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("10mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,    "C3A",    "C3A", true,   3.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("5mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,  "C1.5A",  "C1.5A", true,   1.5, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("2mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface, "C300mA", "C300mA", true,   0.3, 1785096.0, 2177818.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("2mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface, "C150mA", "C150mA", true,  0.15,  892548.0, 1088909.0, 0, SenseRange::Phys, clampJustData));

        break;

    case CL1000A:
        m_sName = QString("CL1000A");

        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("1V")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface, "C1000A", "C1000A", true, 1000.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("500mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,  "C500A",  "C500A", true,  500.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("100mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,  "C100A",  "C100A", true,  100.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("50mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,   "C50A",   "C50A", true,   50.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("10mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,   "C10A",   "C10A", true,   10.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("5mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,    "C5A",    "C5A", true,    5.0, 4132497.0, 5041647.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("2mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface,    "C1A",    "C1A", true,    1.0, 1785096.0, 2177818.0, 0, SenseRange::Phys, clampJustData));
        clampJustData = new cClampJustData(m_pSCPIInterface, m_pMyServer->m_pSenseInterface->getRange(m_sChannelName, QString("2mV")));
        m_RangeList.append(new cSenseRange(m_pSCPIInterface, "C500mA", "C500mA", true,    0.5,  892548.0, 1088909.0, 0, SenseRange::Phys, clampJustData));

        break;
    }
}


void cClamp::addSense()
{
    m_pMyServer->m_pSenseInterface->getChannel(m_sChannelName)->addRangeList(m_RangeList);
}


void cClamp::addSenseInterface()
{
    for (int i = 0; i < m_RangeList.count(); i++)
        m_RangeList.at(i)->initSCPIConnection(QString("SENSE:%1").arg(m_sName));
}


void cClamp::addSystAdjInterface()
{
    QString cmdParent;
    cSCPIDelegate* delegate;

    cmdParent = QString("SYSTEM:CLAMP:%1").arg(m_sChannelName);

    delegate = new cSCPIDelegate(cmdParent, "SERIALNUMBER", SCPI::isQuery | SCPI::isCmdwP, m_pSCPIInterface, clamp::cmdSerial);
    m_DelegateList.append(delegate);
    connect(delegate, SIGNAL(execute(int, cProtonetCommand*)), this, SLOT(executeCommand(int, cProtonetCommand*)));
    delegate = new cSCPIDelegate(cmdParent, "VERSION", SCPI::isQuery | SCPI::isCmdwP, m_pSCPIInterface, clamp::cmdVersion);
    m_DelegateList.append(delegate);
    connect(delegate, SIGNAL(execute(int, cProtonetCommand*)), this, SLOT(executeCommand(int, cProtonetCommand*)));
    delegate = new cSCPIDelegate(cmdParent, "TYPE",SCPI::isQuery | SCPI::isCmdwP, m_pSCPIInterface, clamp::cmdType);
    m_DelegateList.append(delegate);
    connect(delegate, SIGNAL(execute(int, cProtonetCommand*)), this, SLOT(executeCommand(int, cProtonetCommand*)));
    delegate = new cSCPIDelegate(cmdParent, "NAME",SCPI::isQuery | SCPI::isCmdwP, m_pSCPIInterface, SenseSystem::cmdMModeCat );
    m_DelegateList.append(delegate);
    connect(delegate, SIGNAL(execute(int, cProtonetCommand*)), this, SLOT(executeCommand(int, cProtonetCommand*)));

    cmdParent = QString("SYSTEM:ADJUSTMENT:CLAMP:%1:FLASH").arg(m_sChannelName);

    delegate = new cSCPIDelegate(cmdParent,"WRITE", SCPI::isCmd, m_pSCPIInterface, clamp::cmdFlashWrite);
    m_DelegateList.append(delegate);
    connect(delegate, SIGNAL(execute(int, cProtonetCommand*)), this, SLOT(executeCommand(int, cProtonetCommand*)));
    delegate = new cSCPIDelegate(cmdParent,"READ", SCPI::isCmd, m_pSCPIInterface, clamp::cmdFlashRead);
    m_DelegateList.append(delegate);
    connect(delegate, SIGNAL(execute(int, cProtonetCommand*)), this, SLOT(executeCommand(int, cProtonetCommand*)));
    delegate = new cSCPIDelegate(cmdParent,"CHKSUM", SCPI::isQuery, m_pSCPIInterface, SenseSystem::initAdjData);
    m_DelegateList.append(delegate);
    connect(delegate, SIGNAL(execute(int, cProtonetCommand*)), this, SLOT(executeCommand(int, cProtonetCommand*)));

    cmdParent = QString("STATUS:CLAMP:%1").arg(m_sChannelName);

    delegate = new cSCPIDelegate(cmdParent, "ADJUSTMENT", SCPI::isQuery, m_pSCPIInterface, clamp::cmdStatAdjustment);
    m_DelegateList.append(delegate);
    connect(delegate, SIGNAL(execute(int, cProtonetCommand*)), this, SLOT(executeCommand(int, cProtonetCommand*)));

}


cSenseRange* cClamp::getRange(QString name)
{
    cSenseRange* rng = 0;
    for (int i = 0; i < m_RangeList.count(); i++)
    {
        if (m_RangeList.at(i)->getName() == name)
        {
            rng = m_RangeList.at(i);
            break;
        }
    }
    return rng;
}


QString cClamp::m_ReadWriteSerial(QString& sInput)
{
    QString answer;
    cSCPICommand cmd =sInput;

    if (cmd.isQuery())
    {
        answer = m_sSerial;
    }
    else
    {
        if (cmd.isCommand(1))
        {
            QString serial = cmd.getParam(0);
            if (serial.length() == 10)
            {
                m_sSerial = serial;
                answer = SCPI::scpiAnswer[SCPI::ack];
            }
            else
                answer = SCPI::scpiAnswer[SCPI::errval];
        }
        else
            answer = SCPI::scpiAnswer[SCPI::nak];
    }

    return answer;
}


QString cClamp::m_ReadWriteVersion(QString &sInput)
{
    QString answer;
    cSCPICommand cmd =sInput;

    if (cmd.isQuery())
    {
        answer = m_sVersion;
    }
    else
    {
        if (cmd.isCommand(1))
        {
            QString version = cmd.getParam(0);
            if (version.length() == 4)
            {
                m_sVersion = version;
                answer = SCPI::scpiAnswer[SCPI::ack];
            }
            else
                answer = SCPI::scpiAnswer[SCPI::errval];
        }
        else
            answer = SCPI::scpiAnswer[SCPI::nak];
    }

    return answer;
}


QString cClamp::m_ReadWriteType(QString& sInput)
{
    QString answer;
    cSCPICommand cmd =sInput;

    if (cmd.isQuery())
    {
        answer = QString("%1").arg(m_nType);
    }
    else
    {
        if (cmd.isCommand(1))
        {
            quint8 type;
            type = cmd.getParam(0).toInt();

            if ( (type > undefined) && (type < anzCL))
            {
                if (m_bSet)
                {
                    // first we remove the already set ranges
                    m_pMyServer->m_pSenseInterface->getChannel(m_sChannelName)->removeRangeList(m_RangeList);
                    // then we delete them what automatically destroys their interfaces
                    for (int i = 0; i < m_RangeList.count(); i++)
                        delete m_RangeList.at(i);
                }

                m_nType = type;
                initClamp(type);
                exportAdjFlash();
                addSense();
                addSenseInterface();

                answer = SCPI::scpiAnswer[SCPI::ack];

            }

            else
                answer = SCPI::scpiAnswer[SCPI::errval];
        }

        else
            answer = SCPI::scpiAnswer[SCPI::nak];
    }

    return answer;
}


QString cClamp::m_ReadWriteName(QString& sInput)
{
    QString answer;
    cSCPICommand cmd =sInput;

    if (cmd.isQuery())
    {
        answer = m_sName;
    }
    else
    {
        if (cmd.isCommand(1))
        {
            QString name = cmd.getParam(0);
            if (name.length() < 21)
            {
                m_sName = name;
                answer = SCPI::scpiAnswer[SCPI::ack];
            }
            else
                answer = SCPI::scpiAnswer[SCPI::errval];
        }
        else
            answer = SCPI::scpiAnswer[SCPI::nak];
    }

    return answer;
}


QString cClamp::m_WriteFlash(QString& sInput)
{
    QString answer;
    cSCPICommand cmd = sInput;

    if (cmd.isCommand(1) && (cmd.getParam(0) == ""))
    {
        if (exportAdjFlash())
            answer = SCPI::scpiAnswer[SCPI::ack];
        else
            answer = SCPI::scpiAnswer[SCPI::errexec];
    }

    else
        answer = SCPI::scpiAnswer[SCPI::nak];

    return answer;
}


QString cClamp::m_ReadFlash(QString& sInput)
{
    QString answer;
    cSCPICommand cmd = sInput;

    if (cmd.isCommand(1) && (cmd.getParam(0) == ""))
    {
        if (readClampType() == m_nType) // we first look whether the type matches
        {
            importAdjFlash();
            answer = SCPI::scpiAnswer[SCPI::ack];
        }
        else
            answer = SCPI::scpiAnswer[SCPI::errexec];
    }

    else
        answer = SCPI::scpiAnswer[SCPI::nak];

    return answer;
}


QString cClamp::m_ReadChksum(QString& sInput)
{
    QString answer;
    cSCPICommand cmd = sInput;

    if (cmd.isQuery())
    {
        answer = QString("0x%1").arg(getChecksum(),0,16); // hex output
    }
    else
    {
        answer = SCPI::scpiAnswer[SCPI::nak];
    }

    return answer;
}


QString cClamp::m_ReadAdjStatus(QString &sInput)
{
    QString answer;
    cSCPICommand cmd = sInput;

    if (cmd.isQuery())
    {
        answer = QString("%1").arg(getAdjustmentStatus()); // hex output
    }
    else
    {
        answer = SCPI::scpiAnswer[SCPI::nak];
    }

    return answer;
}






