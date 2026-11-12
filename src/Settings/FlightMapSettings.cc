#include "FlightMapSettings.h"

DECLARE_SETTINGGROUP(FlightMap, "FlightMap")
{
}

DECLARE_SETTINGSFACT(FlightMapSettings, mapProvider)
DECLARE_SETTINGSFACT(FlightMapSettings, mapType)
DECLARE_SETTINGSFACT(FlightMapSettings, elevationMapProvider)
DECLARE_SETTINGSFACT(FlightMapSettings, vehicleTrailEnabled)
DECLARE_SETTINGSFACT(FlightMapSettings, vehicleTrailLifetimeSec)
DECLARE_SETTINGSFACT(FlightMapSettings, vehicleTrailStyle)
DECLARE_SETTINGSFACT(FlightMapSettings, vehicleTrailColor)
DECLARE_SETTINGSFACT(FlightMapSettings, vehicleTrailWidth)
DECLARE_SETTINGSFACT(FlightMapSettings, vehicleTrailOpacity)
DECLARE_SETTINGSFACT(FlightMapSettings, vehicleTrailClearWhenStopped)
