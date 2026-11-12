#pragma once

#include <QtQmlIntegration/QtQmlIntegration>

#include "SettingsGroup.h"

class SimulationObjectSettings : public SettingsGroup
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("")

public:
    SimulationObjectSettings(QObject* parent = nullptr);
    DEFINE_SETTING_NAME_GROUP()

    DEFINE_SETTINGFACT(enabled)
    DEFINE_SETTINGFACT(bindAddress)
    DEFINE_SETTINGFACT(listenPort)
    DEFINE_SETTINGFACT(allowedSender)
    DEFINE_SETTINGFACT(producerId)
    DEFINE_SETTINGFACT(completedRetentionMs)
    DEFINE_SETTINGFACT(staleTimeoutMs)
    DEFINE_SETTINGFACT(removalTimeoutMs)
};
