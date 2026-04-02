include_guard(GLOBAL)

include(CheckCXXSourceCompiles)

option(SAFECROWD_ENABLE_IMPORT_STACK "Enable the local domain import stack wiring." OFF)

set(SAFECROWD_IMPORT_STACK_ROOT "" CACHE PATH "Common prefix that contains include/ and lib/ for the local import stack.")
set(SAFECROWD_LIBDXFRW_ROOT "" CACHE PATH "Install prefix or build root for libdxfrw.")
set(SAFECROWD_IFCOPENSHELL_ROOT "" CACHE PATH "Install prefix or build root for IfcOpenShell.")
set(SAFECROWD_CLIPPER2_ROOT "" CACHE PATH "Install prefix or build root for Clipper2.")
set(SAFECROWD_BOOST_ROOT "" CACHE PATH "Install prefix or build root for Boost.")
set(SAFECROWD_RECAST_ROOT "" CACHE PATH "Install prefix or build root for RecastNavigation.")

function(_safecrowd_collect_import_hints out_var dependency_root dependency_folder)
    set(_hints "")

    if (SAFECROWD_IMPORT_STACK_ROOT)
        list(APPEND _hints
            "${SAFECROWD_IMPORT_STACK_ROOT}"
            "${SAFECROWD_IMPORT_STACK_ROOT}/${dependency_folder}"
            "${SAFECROWD_IMPORT_STACK_ROOT}/${dependency_folder}/install"
            "${SAFECROWD_IMPORT_STACK_ROOT}/${dependency_folder}/build/install"
            "${SAFECROWD_IMPORT_STACK_ROOT}/${dependency_folder}/out/install/x64-windows"
        )
    endif()

    if (dependency_root)
        list(APPEND _hints
            "${dependency_root}"
            "${dependency_root}/install"
            "${dependency_root}/build/install"
            "${dependency_root}/out/install/x64-windows"
        )
    endif()

    list(REMOVE_DUPLICATES _hints)
    set(${out_var} "${_hints}" PARENT_SCOPE)
endfunction()

function(_safecrowd_resolve_import_dependency dependency_key)
    set(options HEADER_ONLY)
    set(oneValueArgs ROOT_PATH ROOT_VAR_NAME INCLUDE_VAR LIBRARY_VAR)
    set(multiValueArgs HEADER_NAMES LIB_NAMES)
    cmake_parse_arguments(DEP "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    _safecrowd_collect_import_hints(_dependency_hints "${DEP_ROOT_PATH}" "${dependency_key}")

    find_path(${DEP_INCLUDE_VAR}
        NAMES ${DEP_HEADER_NAMES}
        HINTS ${_dependency_hints}
        PATH_SUFFIXES include Include src
    )

    if (NOT DEFINED ${DEP_INCLUDE_VAR} OR NOT IS_DIRECTORY "${${DEP_INCLUDE_VAR}}")
        message(FATAL_ERROR
            "SafeCrowd import stack is enabled, but ${dependency_key} headers were not found. "
            "Set ${DEP_ROOT_VAR_NAME} or SAFECROWD_IMPORT_STACK_ROOT."
        )
    endif()

    set(${DEP_INCLUDE_VAR} "${${DEP_INCLUDE_VAR}}" PARENT_SCOPE)

    if (DEP_HEADER_ONLY)
        return()
    endif()

    find_library(${DEP_LIBRARY_VAR}
        NAMES ${DEP_LIB_NAMES}
        HINTS ${_dependency_hints}
        PATH_SUFFIXES lib lib64 build build/Debug build/Release Debug Release
    )

    if (NOT DEFINED ${DEP_LIBRARY_VAR} OR NOT EXISTS "${${DEP_LIBRARY_VAR}}")
        message(FATAL_ERROR
            "SafeCrowd import stack is enabled, but ${dependency_key} libraries were not found. "
            "Set ${DEP_ROOT_VAR_NAME} or SAFECROWD_IMPORT_STACK_ROOT."
        )
    endif()

    set(${DEP_LIBRARY_VAR} "${${DEP_LIBRARY_VAR}}" PARENT_SCOPE)
endfunction()

function(_safecrowd_validate_import_stack include_dirs link_libraries)
    set(_saved_required_includes "${CMAKE_REQUIRED_INCLUDES}")
    set(_saved_required_libraries "${CMAKE_REQUIRED_LIBRARIES}")
    set(_saved_required_quiet "${CMAKE_REQUIRED_QUIET}")

    set(CMAKE_REQUIRED_INCLUDES "${include_dirs}")
    set(CMAKE_REQUIRED_LIBRARIES "${link_libraries}")
    set(CMAKE_REQUIRED_QUIET TRUE)

    unset(SAFECROWD_IMPORT_STACK_SMOKE_OK CACHE)
    unset(SAFECROWD_IMPORT_STACK_SMOKE_OK)

    check_cxx_source_compiles(
        [=[
        #if __has_include("libdxfrw.h")
        #include "libdxfrw.h"
        #elif __has_include("dxfrw.h")
        #include "dxfrw.h"
        #else
        #error "Missing libdxfrw header"
        #endif

        #if __has_include("ifcparse/IfcFile.h")
        #include "ifcparse/IfcFile.h"
        #elif __has_include("IfcParse.h")
        #include "IfcParse.h"
        #else
        #error "Missing IfcOpenShell parser header"
        #endif

        #if __has_include("clipper2/clipper.h")
        #include "clipper2/clipper.h"
        #elif __has_include("clipper.h")
        #include "clipper.h"
        #else
        #error "Missing Clipper2 header"
        #endif

        #include <boost/geometry.hpp>

        #if __has_include("Recast.h")
        #include "Recast.h"
        #elif __has_include("recast/Recast.h")
        #include "recast/Recast.h"
        #elif __has_include("recastnavigation/Recast.h")
        #include "recastnavigation/Recast.h"
        #else
        #error "Missing Recast header"
        #endif

        #if __has_include("DetourNavMesh.h")
        #include "DetourNavMesh.h"
        #elif __has_include("recast/DetourNavMesh.h")
        #include "recast/DetourNavMesh.h"
        #elif __has_include("recastnavigation/DetourNavMesh.h")
        #include "recastnavigation/DetourNavMesh.h"
        #else
        #error "Missing Detour header"
        #endif

        int main() {
            return 0;
        }
        ]=]
        SAFECROWD_IMPORT_STACK_SMOKE_OK
    )

    set(CMAKE_REQUIRED_INCLUDES "${_saved_required_includes}")
    set(CMAKE_REQUIRED_LIBRARIES "${_saved_required_libraries}")
    set(CMAKE_REQUIRED_QUIET "${_saved_required_quiet}")

    if (NOT SAFECROWD_IMPORT_STACK_SMOKE_OK)
        message(FATAL_ERROR
            "SafeCrowd import stack smoke validation failed. "
            "Check the configured include/lib roots for libdxfrw, IfcOpenShell, Clipper2, Boost, Recast, and Detour."
        )
    endif()
endfunction()

function(safecrowd_configure_domain_import_stack target_name)
    if (NOT TARGET ${target_name})
        message(FATAL_ERROR "Target ${target_name} does not exist.")
    endif()

    if (NOT SAFECROWD_ENABLE_IMPORT_STACK)
        target_compile_definitions(${target_name} PUBLIC SAFECROWD_IMPORT_STACK_ENABLED=0)
        message(STATUS "SafeCrowd import stack: disabled")
        return()
    endif()

    _safecrowd_resolve_import_dependency(
        libdxfrw
        ROOT_PATH "${SAFECROWD_LIBDXFRW_ROOT}"
        ROOT_VAR_NAME "SAFECROWD_LIBDXFRW_ROOT"
        INCLUDE_VAR SAFECROWD_LIBDXFRW_INCLUDE_DIR
        LIBRARY_VAR SAFECROWD_LIBDXFRW_LIBRARY
        HEADER_NAMES libdxfrw.h dxfrw.h
        LIB_NAMES dxfrw libdxfrw
    )

    _safecrowd_resolve_import_dependency(
        ifcopenshell
        ROOT_PATH "${SAFECROWD_IFCOPENSHELL_ROOT}"
        ROOT_VAR_NAME "SAFECROWD_IFCOPENSHELL_ROOT"
        INCLUDE_VAR SAFECROWD_IFCOPENSHELL_INCLUDE_DIR
        LIBRARY_VAR SAFECROWD_IFCOPENSHELL_LIBRARY
        HEADER_NAMES ifcparse/IfcFile.h IfcParse.h
        LIB_NAMES IfcParse ifcparse libIfcParse
    )

    _safecrowd_resolve_import_dependency(
        clipper2
        HEADER_ONLY
        ROOT_PATH "${SAFECROWD_CLIPPER2_ROOT}"
        ROOT_VAR_NAME "SAFECROWD_CLIPPER2_ROOT"
        INCLUDE_VAR SAFECROWD_CLIPPER2_INCLUDE_DIR
        HEADER_NAMES clipper2/clipper.h clipper.h
    )

    _safecrowd_resolve_import_dependency(
        boost
        HEADER_ONLY
        ROOT_PATH "${SAFECROWD_BOOST_ROOT}"
        ROOT_VAR_NAME "SAFECROWD_BOOST_ROOT"
        INCLUDE_VAR SAFECROWD_BOOST_INCLUDE_DIR
        HEADER_NAMES boost/geometry.hpp
    )

    _safecrowd_resolve_import_dependency(
        recastnavigation
        ROOT_PATH "${SAFECROWD_RECAST_ROOT}"
        ROOT_VAR_NAME "SAFECROWD_RECAST_ROOT"
        INCLUDE_VAR SAFECROWD_RECAST_INCLUDE_DIR
        LIBRARY_VAR SAFECROWD_RECAST_LIBRARY
        HEADER_NAMES Recast.h recast/Recast.h recastnavigation/Recast.h
        LIB_NAMES Recast recast libRecast
    )

    _safecrowd_resolve_import_dependency(
        recastnavigation
        ROOT_PATH "${SAFECROWD_RECAST_ROOT}"
        ROOT_VAR_NAME "SAFECROWD_RECAST_ROOT"
        INCLUDE_VAR SAFECROWD_DETOUR_INCLUDE_DIR
        LIBRARY_VAR SAFECROWD_DETOUR_LIBRARY
        HEADER_NAMES DetourNavMesh.h recast/DetourNavMesh.h recastnavigation/DetourNavMesh.h
        LIB_NAMES Detour detour libDetour
    )

    set(_import_include_dirs
        "${SAFECROWD_LIBDXFRW_INCLUDE_DIR}"
        "${SAFECROWD_IFCOPENSHELL_INCLUDE_DIR}"
        "${SAFECROWD_CLIPPER2_INCLUDE_DIR}"
        "${SAFECROWD_BOOST_INCLUDE_DIR}"
        "${SAFECROWD_RECAST_INCLUDE_DIR}"
        "${SAFECROWD_DETOUR_INCLUDE_DIR}"
    )
    list(REMOVE_DUPLICATES _import_include_dirs)

    set(_import_link_libraries
        "${SAFECROWD_LIBDXFRW_LIBRARY}"
        "${SAFECROWD_IFCOPENSHELL_LIBRARY}"
        "${SAFECROWD_RECAST_LIBRARY}"
        "${SAFECROWD_DETOUR_LIBRARY}"
    )

    _safecrowd_validate_import_stack("${_import_include_dirs}" "${_import_link_libraries}")

    target_compile_definitions(${target_name} PUBLIC SAFECROWD_IMPORT_STACK_ENABLED=1)
    target_include_directories(${target_name}
        SYSTEM PRIVATE
            ${_import_include_dirs}
    )
    target_link_libraries(${target_name}
        PRIVATE
            ${_import_link_libraries}
    )

    message(STATUS "SafeCrowd import stack: enabled")
    message(STATUS "  libdxfrw: ${SAFECROWD_LIBDXFRW_INCLUDE_DIR}")
    message(STATUS "  IfcOpenShell: ${SAFECROWD_IFCOPENSHELL_INCLUDE_DIR}")
    message(STATUS "  Clipper2: ${SAFECROWD_CLIPPER2_INCLUDE_DIR}")
    message(STATUS "  Boost: ${SAFECROWD_BOOST_INCLUDE_DIR}")
    message(STATUS "  Recast/Detour: ${SAFECROWD_RECAST_INCLUDE_DIR}")
endfunction()
