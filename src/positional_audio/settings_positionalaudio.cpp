#include "settings_positionalaudio.h"

#include "ts_helpers_qt.h"
#include "ts_logging_qt.h"
#include "definitions_positionalaudio.h"
#include "ts_serversinfo.h"
#ifdef Q_OS_WIN
  #include "plugin_qt.h"        //only used for gw2 map atm
#endif

#include <QSpacerItem>
#include <QGridLayout>
#ifdef Q_OS_WIN
  #include <QDesktopServices>   //only used for gw2 map atm
#endif


SettingsPositionalAudio* SettingsPositionalAudio::m_Instance = 0;

SettingsPositionalAudio::SettingsPositionalAudio() :
    m_ContextMenuUi(-1)
{
    this->setObjectName("SettingsPositionalAudio");
}

SettingsPositionalAudio::~SettingsPositionalAudio(){}

void SettingsPositionalAudio::Init(PositionalAudio *positionalAudio)
{
    if(m_ContextMenuUi == -1)
    {
        m_ContextMenuUi = TSContextMenu::instance()->Register(this,PLUGIN_MENU_TYPE_GLOBAL,"Positional Audio","radar_16.png");
#ifdef Q_OS_WIN
        m_ContextMenuGW2Map = TSContextMenu::instance()->Register(this,PLUGIN_MENU_TYPE_GLOBAL,"Gw2 Map","");
#endif
        connect(TSContextMenu::instance(), &TSContextMenu::MenusInitialized, this, &SettingsPositionalAudio::onMenusInitialized, Qt::AutoConnection);
        connect(TSContextMenu::instance(), &TSContextMenu::FireContextMenuEvent, this, &SettingsPositionalAudio::onContextMenuEvent, Qt::AutoConnection);
    }

    connect(this, &SettingsPositionalAudio::EnabledSet, positionalAudio, &PositionalAudio::setEnabled);
    connect(this, &SettingsPositionalAudio::UseCameraSet, positionalAudio, &PositionalAudio::setUseCamera);
    connect(this, &SettingsPositionalAudio::UseAttenuationSet, positionalAudio, &PositionalAudio::setUseAttenuation);
    connect(this, &SettingsPositionalAudio::DistanceMinChanged, positionalAudio, &PositionalAudio::setDistanceMin);
    connect(this, &SettingsPositionalAudio::DistanceMaxChanged, positionalAudio,&PositionalAudio::setDistanceMax);
    connect(this, &SettingsPositionalAudio::RollOffChanged, positionalAudio, &PositionalAudio::setRollOff);
    connect(this, &SettingsPositionalAudio::RollOffMaxChanged, positionalAudio, &PositionalAudio::setRollOffMax);

    connect(this, &SettingsPositionalAudio::ServerSettingsAdd, positionalAudio, &PositionalAudio::AddServerSetting);
    connect(this, &SettingsPositionalAudio::ServerSettingsRemove, positionalAudio, &PositionalAudio::RemoveServerSetting);
    connect(this, &SettingsPositionalAudio::ServerEnabledSet, positionalAudio, &PositionalAudio::setServerSettingEnabled);
    connect(this, &SettingsPositionalAudio::ServerBlockSet, positionalAudio, &PositionalAudio::setServerSettingBlocked);
    connect(this, &SettingsPositionalAudio::ServerSendIntervalChange, positionalAudio, &PositionalAudio::setServerSettingSendInterval);
    connect(this, &SettingsPositionalAudio::ServerSendIntervalSilentIncChange, positionalAudio, &PositionalAudio::setServerSettingSendIntervalSilentInc);

    //pure display
    connect(positionalAudio, &PositionalAudio::myIdentityChanged, this, &SettingsPositionalAudio::UpdateUIStatusSelfName, Qt::QueuedConnection);
    connect(positionalAudio, &PositionalAudio::myVrChanged, this, &SettingsPositionalAudio::UpdateUIStatusSelfGame, Qt::QueuedConnection);


    QSettings cfg(TSHelpers::GetFullConfigPath(), QSettings::IniFormat);
    cfg.beginGroup(positionalAudio->objectName());

    cfg.beginGroup("server_s");
    auto groups = cfg.childGroups();
    if (groups.isEmpty())
    {
        emit ServerSettingsAdd("default","Default");
        emit ServerEnabledSet("default",true);
        emit ServerBlockSet("default",false);
        emit ServerSendIntervalChange("default",1.0f);
        emit ServerSendIntervalSilentIncChange("default",1.0f);
    }
    else
    {
        for (int i = 0; i < groups.size(); ++i)
        {
            auto sUId = groups.at(i);
            cfg.beginGroup(sUId);
            emit ServerSettingsAdd(sUId, cfg.value("sname").toString());
            emit ServerEnabledSet(sUId,cfg.value("enabled",true).toBool());
            emit ServerBlockSet(sUId,cfg.value("blocked",false).toBool());
            emit ServerSendIntervalChange(sUId,cfg.value("send_interval",1.0f).toFloat());
            emit ServerSendIntervalSilentIncChange(sUId,cfg.value("send_interval_silentinc",1.0f).toFloat());
            cfg.endGroup();
        }
    }
    cfg.endGroup();

    emit UseCameraSet(cfg.value("isUseCamera",true).toBool());
    emit EnabledSet(cfg.value("enabled",true).toBool());
    cfg.beginGroup("attenuation");
    emit UseAttenuationSet(cfg.value("enabled",false).toBool());
    emit DistanceMinChanged(cfg.value("distance_min",0).toInt());
    emit DistanceMaxChanged(cfg.value("distance_max",0).toInt());
    emit RollOffChanged(cfg.value("rolloff",-6.0f).toFloat());
    emit RollOffMaxChanged(cfg.value("rolloff_max",-200.0f).toFloat());

    cfg.endGroup();
    cfg.endGroup();

    mP_positionalAudio = positionalAudio;
}

void SettingsPositionalAudio::onContextMenuEvent(uint64 serverConnectionHandlerID, PluginMenuType type, int menuItemID, uint64 selectedItemID)
{
    Q_UNUSED(serverConnectionHandlerID);
    Q_UNUSED(selectedItemID);

    if (type == PLUGIN_MENU_TYPE_GLOBAL)
    {
        if (menuItemID == m_ContextMenuGW2Map)
        {
            QUrl gwUrl("http://thorwe.github.io/CrossTalk/misc/site/gw2/html/gw2maps-leaflet.html");
            quint16 port;
#ifdef USE_WEBSOCKET
            port = PluginQt::instance()->m_WebSocketServer->getPort();
            gwUrl.setQuery(QString("websocket_port=%1").arg(port));
#endif
            //port = PluginQt::instance()->getServerPort(); //todo add query
            QDesktopServices::openUrl(gwUrl);
        }
        else if (menuItemID == m_ContextMenuUi)
        {
            if (config)
                config.data()->activateWindow();
            else
            {
                auto p_config = new ConfigPositionalAudio(TSHelpers::GetMainWindow());  //has delete on close attribute

                QSettings cfg(TSHelpers::GetFullConfigPath(), QSettings::IniFormat);
                cfg.beginGroup(mP_positionalAudio.data()->objectName());
                //p_config->UpdateUIEnabledSet(cfg.value("enabled",true).toBool());
                this->connect(this, &SettingsPositionalAudio::UpdateUIEnabledSet, p_config, &ConfigPositionalAudio::UpdateUIEnabledSet);
                emit UpdateUIEnabledSet(cfg.value("enabled",true).toBool());
                this->connect(this, &SettingsPositionalAudio::UpdateUIUseCameraSet, p_config, &ConfigPositionalAudio::UpdateUIUseCameraSet);
                emit UpdateUIUseCameraSet(cfg.value("isUseCamera",true).toBool());

                cfg.beginGroup("attenuation");
                this->connect(this, &SettingsPositionalAudio::UpdateUIUseAttenuationSet, p_config, &ConfigPositionalAudio::UpdateUIUseAttenuationSet);
                emit UpdateUIUseAttenuationSet(cfg.value("enabled",false).toBool());
                this->connect(this, &SettingsPositionalAudio::UpdateUIDistanceMin, p_config, &ConfigPositionalAudio::UpdateUIDistanceMin);
                emit UpdateUIDistanceMin(cfg.value("distance_min",0).toInt());
                this->connect(this, &SettingsPositionalAudio::UpdateUIDistanceMax, p_config, &ConfigPositionalAudio::UpdateUIDistanceMax);
                emit UpdateUIDistanceMax(cfg.value("distance_max",0).toInt());
                this->connect(this, &SettingsPositionalAudio::UpdateUIRollOff, p_config, &ConfigPositionalAudio::UpdateUIRollOff);
                emit UpdateUIRollOff(cfg.value("rolloff",-6.0f).toFloat());
                this->connect(this, &SettingsPositionalAudio::UpdateUIRollOffMax, p_config, &ConfigPositionalAudio::UpdateUIRollOffMax);
                emit UpdateUIRollOffMax(cfg.value("rolloff_max",-200.0f).toFloat());
                cfg.endGroup();

                this->connect(this, &SettingsPositionalAudio::UpdateUIServerSettingsAdd, p_config, &ConfigPositionalAudio::UpdateUIServerAdd);
                this->connect(this, &SettingsPositionalAudio::UpdateUIServerSettingsRemove, p_config, &ConfigPositionalAudio::UpdateUIServerRemove);
                this->connect(this, &SettingsPositionalAudio::UpdateUIServerEnabled, p_config, &ConfigPositionalAudio::UpdateUIServerEnabled);
                this->connect(this, &SettingsPositionalAudio::UpdateUIServerBlock, p_config, &ConfigPositionalAudio::UpdateUIServerBlocked);
                this->connect(this, &SettingsPositionalAudio::UpdateUIServerSendInterval, p_config, &ConfigPositionalAudio::UpdateUIServerSendInterval);
                this->connect(this, &SettingsPositionalAudio::UpdateUIServerSendIntervalSilentInc, p_config, &ConfigPositionalAudio::UpdateUIServerSendIntervalSilentInc);
                this->connect(this, &SettingsPositionalAudio::UpdateUIServerSelect, p_config, &ConfigPositionalAudio::UpdateUIServerSelect);

                cfg.beginGroup("server_s");
                auto groups = cfg.childGroups();
                //QMap<QString,PositionalAudio_ServerSettings> map;
                if (groups.isEmpty())
                {
                    emit UpdateUIServerSettingsAdd("default","Default");
                    emit UpdateUIServerEnabled("default", true);
                    emit UpdateUIServerBlock("default", false);
                    emit UpdateUIServerSendInterval("default", 1.0f);
                    emit UpdateUIServerSendIntervalSilentInc("default",1.0f);
                }
                else
                {
                    groups.sort();
                    int defIndex = groups.indexOf("default");
                    if (defIndex > 0)
                        groups.insert(0,groups.takeAt(defIndex));

                    for (int i = 0; i < groups.size(); ++i)
                    {
                        QString sUId = groups.at(i);
                        cfg.beginGroup(sUId);
                        emit UpdateUIServerSettingsAdd(sUId, cfg.value("sname").toString());
                        emit UpdateUIServerEnabled(sUId, cfg.value("enabled").toBool());
                        emit UpdateUIServerBlock(sUId, cfg.value("blocked",false).toBool());
                        emit UpdateUIServerSendInterval(sUId, cfg.value("send_interval").toFloat());
                        emit UpdateUIServerSendIntervalSilentInc(sUId,cfg.value("send_interval_silentinc",1.0f).toFloat());
                        cfg.endGroup();
                    }
                }
//                p_config->UpdateServerSettingsSet(map);
//                emit UpdateServerSettingsSet(map);
                uint64 serverConnectionHandlerId = ts3Functions.getCurrentServerConnectionHandlerID();
                TSServerInfo* serverInfo = TSServersInfo::instance()->GetServerInfo(serverConnectionHandlerId);
                emit UpdateUIServerSelect((serverInfo != (TSServerInfo*)NULL)?serverInfo->getUniqueId():"default");
                cfg.endGroup();

                cfg.endGroup();

                this->connect(p_config, &ConfigPositionalAudio::EnabledSet, this, &SettingsPositionalAudio::EnabledSet);
                this->connect(p_config, &ConfigPositionalAudio::CameraSet, this, &SettingsPositionalAudio::UseCameraSet);
                this->connect(p_config, &ConfigPositionalAudio::AttenuationSet, this, &SettingsPositionalAudio::UseAttenuationSet);
                this->connect(p_config, &ConfigPositionalAudio::DistanceMinChanged, this, &SettingsPositionalAudio::DistanceMinChanged);
                this->connect(p_config, &ConfigPositionalAudio::DistanceMaxChanged, this, &SettingsPositionalAudio::DistanceMaxChanged);
                this->connect(p_config, &ConfigPositionalAudio::RollOffChanged, this, &SettingsPositionalAudio::RollOffChanged);
                this->connect(p_config, &ConfigPositionalAudio::RollOffMaxChanged, this, &SettingsPositionalAudio::RollOffMaxChanged);

                this->connect(p_config, &ConfigPositionalAudio::ServerEnabledSet, this, &SettingsPositionalAudio::ServerEnabledSet);
                this->connect(p_config, &ConfigPositionalAudio::ServerSendIntervalChange, this, &SettingsPositionalAudio::ServerSendIntervalChange);
                this->connect(p_config, &ConfigPositionalAudio::ServerSendIntervalSilentIncChange, this, &SettingsPositionalAudio::ServerSendIntervalSilentIncChange);
                this->connect(p_config, &ConfigPositionalAudio::ServerBlockSet, this, &SettingsPositionalAudio::ServerBlockSet);

                this->connect(p_config, &ConfigPositionalAudio::ServerAddButtonClicked, this, &SettingsPositionalAudio::AddCustomServerSettingForCurrentTab);
                this->connect(p_config, &ConfigPositionalAudio::ServerRemoveButtonClicked, this, &SettingsPositionalAudio::RemoveCustomServerSettingByServerUniqueId);

                connect(p_config, &ConfigPositionalAudio::finished, this, &SettingsPositionalAudio::saveSettings);

                //pure display
                this->connect(this, &SettingsPositionalAudio::UpdateUIStatusSelfName, p_config, &ConfigPositionalAudio::UpdateUIStatusSelfName);
                emit UpdateUIStatusSelfName(mP_positionalAudio.data()->getMyIdentity());
                this->connect(this, &SettingsPositionalAudio::UpdateUIStatusSelfGame, p_config, &ConfigPositionalAudio::UpdateUIStatusSelfGame);
                emit UpdateUIStatusSelfGame(mP_positionalAudio.data()->getMyVr());

                p_config->show();
                config = p_config;
            }
        }
    }
}

//! Test if context menu registered properly
void SettingsPositionalAudio::onMenusInitialized()
{
    if(m_ContextMenuUi == -1)
        TSLogging::Error(QString("%1: Menu wasn't registered.").arg(this->objectName()));
}

void SettingsPositionalAudio::AddCustomServerSettingForCurrentTab()
{
    uint64 scHandlerId = ts3Functions.getCurrentServerConnectionHandlerID();
    if (scHandlerId != (uint64)NULL)
        AddCustomServerSetting(scHandlerId);
}

void SettingsPositionalAudio::AddCustomServerSetting(uint64 serverConnectionHandlerID)
{
    TSServerInfo* info = TSServersInfo::instance()->GetServerInfo(serverConnectionHandlerID);
    if (info == NULL)
    {
        QMessageBox msgBoxNotConnected;
        msgBoxNotConnected.setText("You need to be connected to the server you want to add a custom seting for.");
        msgBoxNotConnected.exec();
        return;
    }

    QString serverName = info->getName();
    if (serverName == QString::null)
        return;

    int ret = QMessageBox::Yes;
    if (config && config.data()->isVisible())
    {
        QMessageBox msgBox(config.data());
        msgBox.moveToThread(config.data()->thread());
        //    msgBox->setAttribute( Qt::WA_DeleteOnClose, true ); //makes sure the msgbox is deleted automatically when closed
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setText(QString("Create Custom Server Setting for %1?").arg(serverName));
        msgBox.setInformativeText("Servers without custom settings will use the default setting");
        //msgBox.setDetailedText("");
        msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Ok);
        msgBox.setWindowTitle("Positional Audio: Add custom server setting");
        ret = msgBox.exec();    // blocking variant; might work to workaround api check disaster
        //    msgBox->open(this,SLOT(onButtonClicked(QAbstractButton*)));
        //    msgBox->deleteLater();
    }
    if (!((ret == QMessageBox::Ok) || (ret == QMessageBox::Yes)))
        return;

    QString serverUniqueId = info->getUniqueId();
    if (serverUniqueId == QString::null)
        return;

    QSettings cfg(TSHelpers::GetFullConfigPath(), QSettings::IniFormat);
    cfg.beginGroup(mP_positionalAudio.data()->objectName());
    cfg.beginGroup("server_s");

    cfg.beginGroup("default");
    bool isEnabled = cfg.value("enabled",true).toBool();
    float sendInterval = cfg.value("send_interval",1.0f).toFloat();
    float sendIntervalSilentInc = cfg.value("send_interval_silentinc",1.0f).toFloat();
    cfg.endGroup();

    cfg.beginGroup(serverUniqueId);
    cfg.setValue("sname",serverName);
    cfg.setValue("enabled",isEnabled);
    cfg.setValue("blocked",false);
    cfg.setValue("send_interval",sendInterval);
    cfg.setValue("send_interval_silentinc",sendIntervalSilentInc);
    cfg.endGroup();

    // Push to module
    emit ServerSettingsAdd(serverUniqueId, serverName);
    emit ServerEnabledSet(serverUniqueId, isEnabled);
    emit ServerBlockSet(serverUniqueId, false);
    emit ServerSendIntervalChange(serverUniqueId, sendInterval);
    emit ServerSendIntervalSilentIncChange(serverUniqueId, sendIntervalSilentInc);

    // Refresh UI
    emit UpdateUIServerSettingsAdd(serverUniqueId, serverName);
    emit UpdateUIServerEnabled(serverUniqueId, isEnabled);
    emit UpdateUIServerBlock(serverUniqueId, false);
    emit UpdateUIServerSendInterval(serverUniqueId, sendInterval);
    emit UpdateUIServerSendIntervalSilentInc(serverUniqueId, sendIntervalSilentInc);
    emit UpdateUIServerSelect(serverUniqueId);

//    if (config && config.data()->isVisible())
//    {
//        QStringList groups = cfg.childGroups();
//        QMap<QString,PositionalAudio_ServerSettings> map;
//        if (groups.isEmpty())
//            return;

//        for (int i = 0; i < groups.size(); ++i)
//        {
//            QString sUId = groups.at(i);
//            cfg.beginGroup(sUId);
//            PositionalAudio_ServerSettings setting;
//            setting.serverUniqueId = sUId;
//            setting.serverName = cfg.value("sname").toString();
//            setting.enabled = cfg.value("enabled").toBool();
//            setting.isBlocked = cfg.value("blocked",false).toBool();
//            setting.sendInterval = cfg.value("send_interval").toFloat();
//            setting.sendIntervalSilentInc = cfg.value("send_interval_silentinc",1.0f).toFloat();
//            map.insert(setting.serverName,setting);
//            cfg.endGroup();
//        }

////        if (config)
////            config.data()->UpdateServerSettingsSet(map);
//        emit UpdateServerSettingsSet(map);
//    }

    cfg.endGroup();
    cfg.endGroup();
}

void SettingsPositionalAudio::RemoveCustomServerSettingByServerUniqueId(QString serverUniqueId)
{
    if (!mP_positionalAudio)
        return;

    QSettings cfg(TSHelpers::GetFullConfigPath(), QSettings::IniFormat);
    cfg.beginGroup(mP_positionalAudio.data()->objectName());
    cfg.beginGroup("server_s");
    QStringList groups = cfg.childGroups();
    if (!groups.contains(serverUniqueId))
        TSLogging::Error("(RemoveCustomServerSettingByServerUniqueId) serverUniqueId not found in settings.");
    else
    {
        cfg.beginGroup(serverUniqueId);
        QString serverName = cfg.value("sname").toString();
        cfg.endGroup();
        cfg.remove(serverUniqueId);
        emit ServerSettingsRemove(serverUniqueId,serverName);
        emit UpdateUIServerSettingsRemove(serverUniqueId,serverName);
    }
    cfg.endGroup();
    cfg.endGroup();
}

void SettingsPositionalAudio::SetServerBlock(QString serverUniqueId, bool val, uint64 serverConnectionHandlerID)
{
    if (!mP_positionalAudio)
        return;

    QSettings cfg(TSHelpers::GetFullConfigPath(), QSettings::IniFormat);
    cfg.beginGroup(mP_positionalAudio.data()->objectName());
    cfg.beginGroup("server_s");
    QStringList groups = cfg.childGroups();
    if (groups.contains(serverUniqueId))
    {
        cfg.beginGroup(serverUniqueId);
        cfg.setValue("blocked",val);
        cfg.endGroup();
        emit ServerBlockSet(serverUniqueId,val);
        emit UpdateUIServerBlock(serverUniqueId,val);
        emit UpdateUIServerSelect(serverUniqueId);
    }
    else if (val)   // add a custom server setting
    {
        if (serverConnectionHandlerID != (uint64)NULL)
        {
            cfg.beginGroup("default");
            bool isEnabled = cfg.value("enabled",true).toBool();
            float sendInterval = cfg.value("send_interval",1.0f).toFloat();
            float sendIntervalSilentInc = cfg.value("send_interval_silentinc",1.0f).toFloat();
            cfg.endGroup();
            cfg.beginGroup(serverUniqueId);
            QString serverName = TSServersInfo::instance()->GetServerInfo(serverConnectionHandlerID)->getName();
            cfg.setValue("sname", serverName);
            cfg.setValue("enabled", isEnabled);
            cfg.setValue("blocked", val);
            cfg.setValue("send_interval", sendInterval);
            cfg.setValue("send_interval_silentinc", sendIntervalSilentInc);
            cfg.endGroup();

            // to mod
            emit ServerSettingsAdd(serverUniqueId, serverName);
            emit ServerEnabledSet(serverUniqueId, isEnabled);
            emit ServerBlockSet(serverUniqueId, val);
            emit ServerSendIntervalChange(serverUniqueId, sendInterval);
            emit ServerSendIntervalSilentIncChange(serverUniqueId, sendIntervalSilentInc);

            // to ui
            emit UpdateUIServerSettingsAdd(serverUniqueId, serverName);
            emit UpdateUIServerEnabled(serverUniqueId, isEnabled);
            emit UpdateUIServerBlock(serverUniqueId, val);
            emit UpdateUIServerSendInterval(serverUniqueId, sendInterval);
            emit UpdateUIServerSendIntervalSilentInc(serverUniqueId, sendIntervalSilentInc);
            emit UpdateUIServerSelect(serverUniqueId);
        }
        else
            TSLogging::Error("(SetServerBlock) Blocking a new server without serverConnectionHandlerID is not implemented yet.");
    }

    cfg.endGroup();
    cfg.endGroup();

    if (val)
    {
        QMessageBox* msgBox = new QMessageBox(TSHelpers::GetMainWindow());
        msgBox->setAttribute(Qt::WA_DeleteOnClose);
        msgBox->setModal(true);
        msgBox->setWindowTitle(ts3plugin_name());
        msgBox->setIcon(QMessageBox::Warning);
        msgBox->setText(QString("Flood Protection triggered for server %1.").arg(TSServersInfo::instance()->GetServerInfo(serverConnectionHandlerID)->getName()));
        msgBox->setInformativeText(tr("The server has been blocked for Positional Audio updates.\n To change your send intervals and unblock it go to the config window.\n Do so now?"));
        msgBox->setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox->setDefaultButton(QMessageBox::Yes);
//        msgBox->setFixedWidth(640); //bug in qt it seems
        QSpacerItem* horizontalSpacer = new QSpacerItem(480, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);
        QGridLayout* layout = (QGridLayout*)msgBox->layout();
        layout->addItem(horizontalSpacer, layout->rowCount(), 0, 1, layout->columnCount());

        msgBox->setProperty("TargetServerUniqueId",serverUniqueId);

        //int ret = msgBox.exec();
//        msgBox->show();
        msgBox->open( this, SLOT(serverBlockMsgBoxClosed(QAbstractButton*)));
//        msgBox.setVisible(true);
        mP_serverBlockMsgBox = msgBox;
    }
}

void SettingsPositionalAudio::saveSettings(int r)
{
    Q_UNUSED(r);
    if (!mP_positionalAudio)
        return;

    QSettings cfg(TSHelpers::GetFullConfigPath(), QSettings::IniFormat);
    cfg.beginGroup(mP_positionalAudio.data()->objectName());
    cfg.setValue("enabled",mP_positionalAudio.data()->isEnabled());
    cfg.setValue("isUseCamera",mP_positionalAudio.data()->isUseCamera());

    cfg.beginGroup("attenuation");
    cfg.setValue("enabled",mP_positionalAudio.data()->isUseAttenuation());
    cfg.setValue("distance_min",mP_positionalAudio.data()->getDistanceMin());
    cfg.setValue("distance_max",mP_positionalAudio.data()->getDistanceMax());
    cfg.setValue("rolloff",mP_positionalAudio.data()->getRollOff());
    cfg.setValue("rolloff_max",mP_positionalAudio.data()->getRollOffMax());
    cfg.endGroup();

    QMap<QString,PositionalAudio_ServerSettings> map = mP_positionalAudio.data()->getServerSettings();
    if (!map.isEmpty())
    {
        cfg.beginGroup("server_s");
        QMapIterator<QString,PositionalAudio_ServerSettings> i(map);
        while (i.hasNext())
        {
            i.next();
            cfg.beginGroup(i.key());
            cfg.setValue("sname",i.value().serverName);
            cfg.setValue("enabled", i.value().enabled);
            cfg.setValue("blocked", i.value().isBlocked);
            cfg.setValue("send_interval",i.value().sendInterval);
            cfg.setValue("send_interval_silentinc", i.value().sendIntervalSilentInc);
            cfg.endGroup();
        }
        cfg.endGroup();
    }
    cfg.endGroup();
}

void SettingsPositionalAudio::serverBlockMsgBoxClosed(QAbstractButton *button)
{
    if (mP_serverBlockMsgBox)
    {
        if (QMessageBox::YesRole == mP_serverBlockMsgBox.data()->buttonRole(button))
        {
            onContextMenuEvent((uint64)NULL, PLUGIN_MENU_TYPE_GLOBAL, m_ContextMenuUi, (uint64)NULL);
            emit UpdateUIServerSelect(mP_serverBlockMsgBox.data()->property("TargetServerUniqueId").toString());
        }
    }
    //mP_serverBlockMsgBox.data()->deleteLater();
}
