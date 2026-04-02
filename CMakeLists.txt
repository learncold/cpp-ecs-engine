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

set(SAFECROWD_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_library(ecs_engine STATIC
    src/engine/Entity.h
    src/engine/EntityRegistry.h
    src/engine/ComponentRegistry.h
    src/engine/EcsCore.h
    src/engine/IComponentStorage.h
    src/engine/EngineConfig.h
    src/engine/EngineRuntime.h
    src/engine/EngineState.h
    src/engine/EngineStats.h
    src/engine/EngineStepContext.h
    src/engine/EngineSystem.h
    src/engine/FrameClock.h
    src/engine/PackedComponentStorage.h
    src/engine/EntityRegistry.cpp
    src/engine/EngineRuntime.cpp
    src/engine/FrameClock.cpp
)

target_include_directories(ecs_engine
    PUBLIC
        ${SAFECROWD_SRC_DIR}
)

configure_project_target(ecs_engine)

add_library(safecrowd_domain STATIC
    src/domain/SafeCrowdDomain.h
    src/domain/SafeCrowdDomain.cpp
)

target_include_directories(safecrowd_domain
    PUBLIC
        ${SAFECROWD_SRC_DIR}
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
        tests/EngineRegistryTests.cpp
        tests/FrameClockTests.cpp
        tests/EcsCoreTests.cpp
        tests/EngineRuntimeTests.cpp
        tests/PackedComponentStorageTests.cpp
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
        src/application/MainWindow.h
        src/application/main.cpp
        src/application/MainWindow.cpp
    )

    target_include_directories(safecrowd_app
        PRIVATE
            ${SAFECROWD_SRC_DIR}
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
