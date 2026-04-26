cmake_minimum_required(VERSION 3.25)

project(SafeCrowd
    VERSION 0.1.0
    LANGUAGES CXX
)

include(CTest)
include(cmake/SafeCrowdImportStack.cmake)

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
    src/engine/IComponentStorage.h
    src/engine/PackedComponentStorage.h
    src/engine/ComponentRegistry.h
    src/engine/EcsCore.h
    src/engine/EngineConfig.h
    src/engine/DeterministicRng.h
    src/engine/EngineRuntime.h
    src/engine/EngineWorld.h
    src/engine/EngineState.h
    src/engine/EngineStats.h
    src/engine/EngineStepContext.h
    src/engine/EngineSystem.h
    src/engine/FrameClock.h
    src/engine/ResourceStore.h
    src/engine/WorldQuery.h
    src/engine/CommandBuffer.h
    src/engine/UpdatePhase.h
    src/engine/TriggerPolicy.h
    src/engine/SystemDescriptor.h
    src/engine/SystemScheduler.h
    src/engine/EntityRegistry.cpp
    src/engine/EngineRuntime.cpp
    src/engine/FrameClock.cpp
    src/engine/internal/EngineWorldFactory.h
    src/engine/internal/EngineRuntimeTestAccess.h
    src/engine/SystemScheduler.cpp
)

target_include_directories(ecs_engine
    PUBLIC
        ${SAFECROWD_SRC_DIR}
)

configure_project_target(ecs_engine)

add_library(safecrowd_domain STATIC
    src/domain/SafeCrowdDomain.h
    src/domain/SafeCrowdDomain.cpp
    src/domain/DemoLayouts.h
    src/domain/DemoLayouts.cpp
    src/domain/Geometry2D.h
    src/domain/PopulationSpec.h
    src/domain/ScenarioAuthoring.h
    src/domain/ProjectRepository.h
    src/domain/ScenarioTemplateCatalog.h
    src/domain/RawImportModel.h
    src/domain/CanonicalGeometry.h
    src/domain/DemoFixtureService.h
    src/domain/DemoFixtureService.cpp
    src/domain/DxfImportService.h
    src/domain/DxfImportService.cpp
    src/domain/FacilityLayout2D.h
    src/domain/FacilityLayoutBuilder.h
    src/domain/FacilityLayoutBuilder.cpp
    src/domain/ImportIssue.h
    src/domain/ImportIssue.cpp
    src/domain/ImportResult.h
    src/domain/ImportOrchestrator.h
    src/domain/ImportValidationService.h
    src/domain/ImportValidationService.cpp
    src/domain/ImportContracts.h
    src/domain/AgentComponents.h
    src/domain/CompressionSystem.cpp
    src/domain/CompressionSystem.h
    src/domain/Metrics.h
    src/domain/Snapshot.cpp
    src/domain/Snapshot.h
)

target_include_directories(safecrowd_domain
    PUBLIC
        ${SAFECROWD_SRC_DIR}
)

target_link_libraries(safecrowd_domain
    PUBLIC
        ecs_engine
)

safecrowd_configure_domain_import_stack(safecrowd_domain)

configure_project_target(safecrowd_domain)

if (BUILD_TESTING)
    add_executable(safecrowd_tests
        tests/TestMain.cpp
        tests/TestSupport.h
        tests/EngineRegistryTests.cpp
        tests/FrameClockTests.cpp
        tests/EngineRuntimeTests.cpp
        tests/PackedComponentStorageTests.cpp
        tests/SafeCrowdDomainTests.cpp
        tests/CompressionSystemTests.cpp
        tests/SnapshotTests.cpp
        tests/EcsCoreTests.cpp
        tests/ImportContractsTests.cpp
        tests/DxfImportServiceTests.cpp
        tests/DemoFixtureServiceTests.cpp
        tests/FacilityLayoutBuilderTests.cpp
        tests/WorldQueryTests.cpp
        tests/CommandBufferTests.cpp
        tests/SystemSchedulerTests.cpp
        tests/EngineIntegrationTests.cpp
        tests/ResourceStoreTests.cpp
        tests/DeterministicRngTests.cpp
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
        src/application/IssueCardWidget.h
        src/application/LayoutPreviewWidget.h
        src/application/LayoutReviewWidget.h
        src/application/MainWindow.h
        src/application/NewProjectRequest.h
        src/application/NewProjectWidget.h
        src/application/ProjectListWidget.h
        src/application/ProjectMetadata.h
        src/application/ProjectNavigatorActions.h
        src/application/ProjectNavigatorWidget.h
        src/application/ProjectPersistence.h
        src/application/ScenarioAuthoringWidget.h
        src/application/UiStyle.h
        src/application/WorkspaceShell.h
        src/application/IssueCardWidget.cpp
        src/application/LayoutPreviewWidget.cpp
        src/application/LayoutReviewWidget.cpp
        src/application/main.cpp
        src/application/MainWindow.cpp
        src/application/NewProjectWidget.cpp
        src/application/ProjectListWidget.cpp
        src/application/ProjectNavigatorActions.cpp
        src/application/ProjectNavigatorWidget.cpp
        src/application/ProjectPersistence.cpp
        src/application/ScenarioAuthoringWidget.cpp
        src/application/UiStyle.cpp
        src/application/WorkspaceShell.cpp
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

    set(_qt_platform_plugin
        "$<IF:$<CONFIG:Debug>,${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/debug/Qt6/plugins/platforms/qwindowsd.dll,${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/Qt6/plugins/platforms/qwindows.dll>"
    )
    set(_qt_platform_plugin_name
        "$<IF:$<CONFIG:Debug>,qwindowsd.dll,qwindows.dll>"
    )

    add_custom_command(TARGET safecrowd_app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:safecrowd_app>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${_qt_platform_plugin}"
            "$<TARGET_FILE_DIR:safecrowd_app>/platforms/${_qt_platform_plugin_name}"
        VERBATIM
    )

    configure_project_target(safecrowd_app)
endif()
