#pragma once

#include "UnitTest.h"

class MapTrailHistoryTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _rollingWindow();
    void _renderAnchor();
    void _stationaryAndResume();
    void _liveLifetimeChange();
    void _independentHistories();
    void _limitsAndDisable();
    void _qmlTypeRegistered();
};
