#include <QList>
#include <scpi.h>

#include "xmlsettings.h"
#include "scpiconnection.h"
#include "resource.h"
#include "mt310s2d.h"
#include "hkeysettings.h"
#include "hkeyinterface.h"
#include "hkeychannel.h"
#include "protonetcommand.h"


cHKeyInterface::cHKeyInterface(cMT310S2dServer *server)
    :m_pMyServer(server)
{
    m_pSCPIInterface = m_pMyServer->getSCPIInterface();

    QList<HKeySystem::cChannelSettings*> channelSettings;
    channelSettings = m_pMyServer->m_pHKeySettings->getChannelSettings();

    // we have 1 hand key input channel
    cHKeyChannel* pChannel;
    pChannel = new cHKeyChannel(m_pSCPIInterface, "Hand key input", 0, channelSettings.at(0) );
    m_ChannelList.append(pChannel);

    m_sVersion = HKeySystem::Version;
}


cHKeyInterface::~cHKeyInterface()
{
    cHKeyChannel* cptr;
    for ( int i = 0; i < m_ChannelList.count(); i++)
    {
        cptr = m_ChannelList.at(i);
        delete cptr;
    }
}


void cHKeyInterface::initSCPIConnection(QString leadingNodes)
{
    cSCPIDelegate* delegate;

    if (leadingNodes != "")
        leadingNodes += ":";

    delegate = new cSCPIDelegate(QString("%1HKEY").arg(leadingNodes),"VERSION",SCPI::isQuery, m_pSCPIInterface, HKeySystem::cmdVersion);
    m_DelegateList.append(delegate);
    connect(delegate, SIGNAL(execute(int, cProtonetCommand*)), this, SLOT(executeCommand(int, cProtonetCommand*)));
    delegate = new cSCPIDelegate(QString("%1HKEY:CHANNEL").arg(leadingNodes),"CATALOG", SCPI::isQuery, m_pSCPIInterface, HKeySystem::cmdChannelCat);
    m_DelegateList.append(delegate);
    connect(delegate, SIGNAL(execute(int, cProtonetCommand*)), this, SLOT(executeCommand(int, cProtonetCommand*)));

    for (int i = 0; i < m_ChannelList.count(); i++)
    {
        connect(m_ChannelList.at(i), SIGNAL(notifier(cNotificationString*)), this, SIGNAL(notifier(cNotificationString*)));
        connect(m_ChannelList.at(i), SIGNAL(cmdExecutionDone(cProtonetCommand*)), this, SIGNAL(cmdExecutionDone(cProtonetCommand*)));
        m_ChannelList.at(i)->initSCPIConnection(QString("%1HKEY").arg(leadingNodes));
    }
}


void cHKeyInterface::registerResource(cRMConnection *rmConnection, quint16 port)
{
    cHKeyChannel* pChannel;
    for (int i = 0; i < m_ChannelList.count(); i++)
    {
        pChannel = m_ChannelList.at(i);
        register1Resource(rmConnection, m_pMyServer->getMsgNr(), QString("HKEY;%1;1;%2;%3;").arg(pChannel->getName()).arg(pChannel->getDescription()).arg(port));
    }
}


void cHKeyInterface::unregisterResource(cRMConnection *rmConnection)
{
    cHKeyChannel* pChannel;
    for (int i = 0; i < m_ChannelList.count(); i++)
    {
        pChannel = m_ChannelList.at(i);
        unregister1Resource(rmConnection, m_pMyServer->getMsgNr(), QString("HKEY;%1;").arg(pChannel->getName()));
    }
}


void cHKeyInterface::executeCommand(int cmdCode, cProtonetCommand *protoCmd)
{
    switch (cmdCode)
    {
    case HKeySystem::cmdVersion:
        protoCmd->m_sOutput = m_ReadVersion(protoCmd->m_sInput);
        break;
    case HKeySystem::cmdChannelCat:
        protoCmd->m_sOutput = m_ReadChannelCatalog(protoCmd->m_sInput);
        break;
    }

    if (protoCmd->m_bwithOutput)
        emit cmdExecutionDone(protoCmd);
}


QString cHKeyInterface::m_ReadVersion(QString &sInput)
{
    cSCPICommand cmd = sInput;

    if (cmd.isQuery())
        return m_sVersion;
    else
        return SCPI::scpiAnswer[SCPI::nak];
}


QString cHKeyInterface::m_ReadChannelCatalog(QString &sInput)
{
    cSCPICommand cmd = sInput;

    if (cmd.isQuery())
    {
        int i;
        QString s;
        for (i = 0; i < m_ChannelList.count()-1; i++ )
            s += m_ChannelList.at(i)->getName() + ";";
        s += m_ChannelList.at(i)->getName();

        return s;
    }
    else
        return SCPI::scpiAnswer[SCPI::nak];
}
