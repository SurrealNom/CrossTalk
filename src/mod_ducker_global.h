#pragma once

#include <QObject>
#include "teamspeak/public_definitions.h"
#include "module.h"
#include "volumes.h"
#include "talkers.h"
#include "ts_infodata_qt.h"
#include "ts_context_menu_qt.h"
#include "dsp_volume_ducker.h"

class Ducker_Global : public Module, public InfoDataInterface, public ContextMenuInterface
{
    Q_OBJECT
    Q_INTERFACES(InfoDataInterface ContextMenuInterface)
    Q_PROPERTY(bool isActive
               READ isActive
               WRITE setActive
               NOTIFY activeSet)
    Q_PROPERTY(float value
               READ getValue
               WRITE setValue
               NOTIFY valueSet)

public:
    explicit Ducker_Global(QObject *parent = 0);

    float getValue() const;

    void AddMusicBot(uint64 serverConnectionHandlerID, anyID clientID);
    void RemoveMusicBot(uint64 serverConnectionHandlerID, anyID clientID);
    void ToggleMusicBot(uint64 serverConnectionHandlerID, anyID clientID);

    // events forwarded from plugin.cpp
    void onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID myID);
    bool onEditPlaybackVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels);
    bool isClientMusicBot(uint64 serverConnectionHandlerID, anyID clientID);
    bool isClientMusicBotRt(uint64 serverConnectionHandlerID, anyID clientID);
    void onTalkStatusChanged(uint64 serverConnectionHandlerID, int status, bool isReceivedWhisper, anyID clientID, bool isMe);

    bool isActive() {return m_isActive;}
    void setActive(bool); // for testing command, move to private later

    QMap<QString,QString>* DuckTargets;

    bool onInfoDataChanged(uint64 serverConnectionHandlerID, uint64 id, enum PluginItemType type, uint64 mine, QTextStream &data);


signals:
    void valueSet(float);
    void activeSet(bool);
public slots:
    void setValue(float newValue);
    void onContextMenuEvent(uint64 serverConnectionHandlerID, PluginMenuType type, int menuItemID, uint64 selectedItemID);

//    void saveSettings();

protected:
    void onRunningStateChanged(bool value);

private:
    bool m_isActive = false;

    float value() {return m_value;}
    float m_value = 0.0f;

    Talkers* talkers;
    Volumes* vols;

    DspVolumeDucker* AddMusicBotVolume(uint64 serverConnectionHandlerID, anyID clientID);

    void SaveDuckTargets();
    void UpdateActive();

    int m_ContextMenuToggleMusicBot = -1;
};
