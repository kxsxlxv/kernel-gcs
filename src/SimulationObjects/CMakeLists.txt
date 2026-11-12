# Simulation telemetry is read-only and independent of QGC's MAVLink Vehicle path.
target_sources(${CMAKE_PROJECT_NAME}
    PRIVATE
        SimulationObjectProtocol.cc
        SimulationObjectProtocol.h
        SimulationObjectManager.cc
        SimulationObjectManager.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
