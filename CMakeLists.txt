cmake_minimum_required(VERSION 3.15.0)

include(.cmake.conf)
project(QtDeviceUtilities
    VERSION "${QT_REPO_MODULE_VERSION}"
    DESCRIPTION "Qt Device Utilities Libraries"
    HOMEPAGE_URL "https://qt.io/"
    LANGUAGES CXX C
)

find_package(Qt6 ${PROJECT_VERSION} CONFIG REQUIRED COMPONENTS BuildInternals Core)
find_package(Qt6 ${PROJECT_VERSION} QUIET CONFIG OPTIONAL_COMPONENTS Gui Network Quick Qml QuickControls2 DBus androidextras)

macro(assertTargets)
    foreach(qtTarget IN ITEMS ${ARGN})
        if(NOT TARGET Qt::${qtTarget})
           message(NOTICE "Skipping the build as the condition \"TARGET Qt::${qtTarget}\" is not met.")
           return()
        endif()
    endforeach()
endmacro()

if(LINUX)
    assertTargets(Quick DBus)
elseif(ANDROID)
    assertTargets(Quick)
endif()

if(NOT LINUX)
    if(NOT ANDROID)
        message(NOTICE "Skipping the build as the condition \"LINUX\" is not met.")
        return()
    endif()
endif()
if(WASM)
    message(NOTICE "Skipping the build as the condition \"NOT WASM\" is not met.")
    return()
endif()

qt_build_repo()
