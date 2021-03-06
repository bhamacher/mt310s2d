#ifndef CLAMPINTERFACE
#define CLAMPINTERFACE

#include <QHash>
#include <QStringList>

#include "scpiconnection.h"


// here we hold the clamps that are hotplugged to the system

class cMT310S2dServer;
class cATMEL;
class cClamp;


namespace ClampSystem
{

enum ClampCommands
{
    cmdClampChannelCat,
    cmdClampWrite,
    cmdClampImportExport
};
}

class cClampInterface: public cSCPIConnection
{

public:
    cClampInterface(cMT310S2dServer *server, cATMEL* controler);
    virtual void initSCPIConnection(QString leadingNodes);
    void actualizeClampStatus();
    void addChannel(QString channel);
    void removeChannel(QString channel);

protected slots:
    virtual void executeCommand(int cmdCode, cProtonetCommand* protoCmd);

private:
    cMT310S2dServer *m_pMyServer;
    cATMEL *m_pControler;
    QStringList m_ClampChannelList;

    quint16 m_nClampStatus;
    QHash<int, cClamp*> clampHash;

    QString m_ReadClampChannelCatalog(QString& sInput);
    QString m_WriteAllClamps(QString& sInput);
    QString m_ImportExportAllClamps(QString& sInput);

};

#endif // CLAMPINTERFACE

