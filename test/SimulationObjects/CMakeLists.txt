target_sources(QGCUnitTests
    PRIVATE
        SimulationObjectTest.cc
        SimulationObjectTest.h
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

add_qgc_test(SimulationObjectTest LABELS Unit)
