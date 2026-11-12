#pragma once

#include "UnitTest.h"

class SimulationObjectTest : public UnitTest
{
    Q_OBJECT

private slots:
    void _validatesProtocol();
    void _identityAndLifecycle();
    void _mapProjectionMetadata();
    void _headingFromVelocity();
    void _headingFromCoordinates();
    void _staleAndDisconnect();
    void _receiverStatus();
};
