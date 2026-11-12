#pragma once

#include "UnitTest.h"

class MapTrailHistoryTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _rollingWindow();
    void _stationaryAndResume();
    void _limitsAndDisable();
};
