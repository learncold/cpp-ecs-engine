cmake_minimum_required(VERSION 3.25)

project(SafeCrowd
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 CONFIG REQUIRED COMPONENTS Core Gui Widgets)
qt_standard_project_setup()

set(SAFECROWD_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

function(configure_project_target target_name)
    if (MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    set_property(TARGET ${target_name} PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endfunction()

add_library(ecs_engine STATIC
    src/engine/EngineConfig.h
    src/engine/EngineRuntime.cpp
    src/engine/EngineRuntime.h
    src/engine/EngineState.h
    src/engine/EngineStats.h
    src/engine/EngineStepContext.h
    src/engine/EngineSystem.h
    src/engine/FrameClock.cpp
    src/engine/FrameClock.h
)

target_include_directories(ecs_engine
    PUBLIC
        ${SAFECROWD_SRC_DIR}
)

configure_project_target(ecs_engine)

add_library(safecrowd_domain STATIC
    src/domain/SafeCrowdDomain.cpp
    src/domain/SafeCrowdDomain.h
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

add_executable(safecrowd_app
    src/application/main.cpp
    src/application/MainWindow.cpp
    src/application/MainWindow.h
)

target_include_directories(safecrowd_app
    PRIVATE
        ${SAFECROWD_SRC_DIR}
)

target_link_libraries(safecrowd_app
    PRIVATE
        safecrowd_domain
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
)

configure_project_target(safecrowd_app)
