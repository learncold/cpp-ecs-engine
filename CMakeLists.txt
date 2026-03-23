cmake_minimum_required(VERSION 3.25)

project(SafeCrowd
    VERSION 0.1.0
    LANGUAGES CXX
)

include(CTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SAFECROWD_BUILD_APP "Build the Qt desktop application" ON)

if (SAFECROWD_BUILD_APP)
    set(CMAKE_AUTOMOC ON)
    set(CMAKE_AUTOUIC ON)
    set(CMAKE_AUTORCC ON)

    find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui Widgets)
    qt_standard_project_setup()
endif()

function(configure_project_target target_name)
    if (MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    set_property(TARGET ${target_name} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endfunction()

add_library(ecs_engine STATIC
    src/engine/include/engine/Entity.h
    src/engine/include/engine/EntityRegistry.h
    src/engine/include/engine/EngineConfig.h
    src/engine/include/engine/EngineRuntime.h
    src/engine/include/engine/EngineState.h
    src/engine/include/engine/EngineStats.h
    src/engine/include/engine/EngineStepContext.h
    src/engine/include/engine/EngineSystem.h
    src/engine/include/engine/FrameClock.h
    src/engine/src/EntityRegistry.cpp
    src/engine/src/EngineRuntime.cpp
    src/engine/src/FrameClock.cpp
)

target_include_directories(ecs_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/include
)

configure_project_target(ecs_engine)

add_library(safecrowd_domain STATIC
    src/domain/include/domain/SafeCrowdDomain.h
    src/domain/src/SafeCrowdDomain.cpp
)

target_include_directories(safecrowd_domain
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/domain/include
)

target_link_libraries(safecrowd_domain
    PUBLIC
        ecs_engine
)

configure_project_target(safecrowd_domain)

if (BUILD_TESTING)
    add_executable(safecrowd_tests
        tests/TestMain.cpp
        tests/TestSupport.h
        tests/EntityRegistryTests.cpp
        tests/FrameClockTests.cpp
        tests/EngineRuntimeTests.cpp
        tests/SafeCrowdDomainTests.cpp
    )

    target_include_directories(safecrowd_tests
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/tests
    )

    target_link_libraries(safecrowd_tests
        PRIVATE
            safecrowd_domain
    )

    configure_project_target(safecrowd_tests)

    add_test(NAME safecrowd_tests COMMAND safecrowd_tests)
endif()

if (SAFECROWD_BUILD_APP)
    add_executable(safecrowd_app
        src/application/include/application/MainWindow.h
        src/application/src/main.cpp
        src/application/src/MainWindow.cpp
    )

    target_include_directories(safecrowd_app
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/application/include
    )

    set_target_properties(safecrowd_app PROPERTIES
        AUTOMOC OFF
        AUTOUIC OFF
        AUTORCC OFF
    )

    target_link_libraries(safecrowd_app
        PRIVATE
            safecrowd_domain
            Qt6::Core
            Qt6::Gui
            Qt6::Widgets
    )

    configure_project_target(safecrowd_app)
endif()
