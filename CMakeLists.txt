cmake_minimum_required(VERSION 3.20)

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(SC4TerrainExtensions VERSION 1.0.0 LANGUAGES CXX)

if(NOT WIN32)
    message(STATUS "SC4TerrainExtensions DLL is only built on Win32. Skipping target.")
    return()
endif()

set(SC4_RENDER_SERVICES_ROOT "${CMAKE_SOURCE_DIR}/vendor/sc4-render-services" CACHE PATH "Path to sc4-render-services")
set(IMGUI_PUBLIC_DIR "${SC4_RENDER_SERVICES_ROOT}/src")
set(IMGUI_INCLUDE_DIR "${SC4_RENDER_SERVICES_ROOT}/vendor/d3d7imgui/ImGui")
set(IMGUI_LIB_DIR "${SC4_RENDER_SERVICES_ROOT}" CACHE PATH "Path containing imgui.lib from sc4-render-services build")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DWIN32 -D_WINDOWS -DUNICODE -D_UNICODE)

if(MSVC)
    add_compile_options(/W4 /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    # Keep PDBs for optimized builds to help SC4 crash diagnosis.
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")
endif()

add_subdirectory(vendor/spdlog)
add_subdirectory(vendor/args)
add_subdirectory(vendor/mINI)

option(SC4_BUILD_RENDER_SERVICES "Build vendor/sc4-render-services targets (imgui.dll + samples)" OFF)
if(SC4_BUILD_RENDER_SERVICES)
    add_subdirectory(vendor/sc4-render-services EXCLUDE_FROM_ALL)
endif()

include(FetchContent)
set(WIL_BUILD_TESTS FALSE)
FetchContent_Declare(WIL
    URL https://github.com/microsoft/wil/archive/refs/tags/v1.0.250325.1.zip
)
FetchContent_MakeAvailable(WIL)

FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)
FetchContent_MakeAvailable(json)

set(GZCOM_DIR "${CMAKE_SOURCE_DIR}/vendor/gzcom-dll/gzcom-dll")
file(GLOB_RECURSE GZCOM_HEADERS CONFIGURE_DEPENDS
    "${GZCOM_DIR}/include/*.h"
    "${GZCOM_DIR}/include/*.hpp"
)
file(GLOB_RECURSE GZCOM_SOURCES CONFIGURE_DEPENDS
    "${GZCOM_DIR}/src/*.cpp"
)
list(REMOVE_ITEM GZCOM_SOURCES
    "${GZCOM_DIR}/src/main.cpp"
    "${GZCOM_DIR}/src/cGZCityHallUpgradeDllDirector.cpp"
    "${GZCOM_DIR}/src/cGZCustomDllDirector.cpp"
    "${GZCOM_DIR}/src/cGZExampleDllDirector.cpp"
    "${GZCOM_DIR}/src/cGZExtraCheatsDllDirector.cpp"
    "${GZCOM_DIR}/src/cGZExtraExtraCheatsDllDirector.cpp"
)

add_library(terrain_gzcom2 STATIC ${GZCOM_SOURCES} ${GZCOM_HEADERS})
target_include_directories(terrain_gzcom2 PUBLIC "${GZCOM_DIR}/include")

set(SC4_DLL_BASICS_DIR "${CMAKE_SOURCE_DIR}/vendor/sc4-dll-basics")
file(GLOB_RECURSE SC4_DLL_BASICS_HEADERS CONFIGURE_DEPENDS
    "${SC4_DLL_BASICS_DIR}/include/*.h"
    "${SC4_DLL_BASICS_DIR}/include/*.hpp"
)
file(GLOB_RECURSE SC4_DLL_BASICS_SOURCES CONFIGURE_DEPENDS
    "${SC4_DLL_BASICS_DIR}/src/*.cpp"
)

add_library(sc4_dll_basics STATIC ${SC4_DLL_BASICS_SOURCES} ${SC4_DLL_BASICS_HEADERS})
target_include_directories(sc4_dll_basics PUBLIC "${SC4_DLL_BASICS_DIR}/include")
target_link_libraries(sc4_dll_basics PUBLIC WIL::WIL)

# Public API from sc4-render-services (headers only).
add_library(sc4_render_services_api INTERFACE)
target_include_directories(sc4_render_services_api INTERFACE
    "${IMGUI_PUBLIC_DIR}"
    "${IMGUI_INCLUDE_DIR}"
)

set(SC4_IMGUI_LINK_TARGET "")
if(TARGET imgui)
    set(SC4_IMGUI_LINK_TARGET imgui)
    message(STATUS "Using in-tree imgui target from vendor/sc4-render-services")
else()
    find_library(IMGUI_LIB NAMES imgui
        PATHS
            "${IMGUI_LIB_DIR}"
            "${IMGUI_LIB_DIR}/Release"
            "${IMGUI_LIB_DIR}/Debug"
            "${IMGUI_LIB_DIR}/build/Release"
            "${IMGUI_LIB_DIR}/build/Debug"
            "${IMGUI_LIB_DIR}/cmake-build-debug-visual-studio/Debug"
            "${IMGUI_LIB_DIR}/cmake-build-release-visual-studio/Release"
        NO_DEFAULT_PATH
    )

    if(IMGUI_LIB)
        add_library(sc4_render_services_imgui UNKNOWN IMPORTED GLOBAL)
        set_target_properties(sc4_render_services_imgui PROPERTIES
            IMPORTED_LOCATION "${IMGUI_LIB}"
        )
        set(SC4_IMGUI_LINK_TARGET sc4_render_services_imgui)
        message(STATUS "Using external imgui.lib: ${IMGUI_LIB}")
    else()
        message(WARNING "imgui.lib not found in sc4-render-services build locations. Build may fail if imgui symbols are referenced.")
    endif()
endif()

file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)
# These .cpp files are #include'd directly by TerrainExtensionsDllDirector.cpp
list(REMOVE_ITEM PROJECT_SOURCES
    "${CMAKE_SOURCE_DIR}/src/tools/ConstantGradeTool.cpp"
    "${CMAKE_SOURCE_DIR}/src/tools/FlattenTool.cpp"
    "${CMAKE_SOURCE_DIR}/src/tools/BlueprintCaptureTool.cpp"
    "${CMAKE_SOURCE_DIR}/src/tools/BlueprintExportTool.cpp"
    "${CMAKE_SOURCE_DIR}/src/tools/BlueprintStampTool.cpp"
    "${CMAKE_SOURCE_DIR}/src/tools/SlopeMapTool.cpp"
    "${CMAKE_SOURCE_DIR}/src/viz/TerrainSlopeRenderer.cpp"
)
list(APPEND PROJECT_SOURCES "${CMAKE_SOURCE_DIR}/src/version.rc")
set(SC4_DLL_DEF "${CMAKE_SOURCE_DIR}/SC4TerrainExtensions.def")

add_library(${PROJECT_NAME} SHARED ${PROJECT_SOURCES} ${SC4_DLL_DEF})
target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX)

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${GZCOM_DIR}/include"
    "${SC4_DLL_BASICS_DIR}/include"
)

set(SC4_DLL_LINK_LIBS
    spdlog
    WIL::WIL
    args
    mINI
    nlohmann_json
    terrain_gzcom2
    sc4_dll_basics
    sc4_render_services_api
)

if(SC4_IMGUI_LINK_TARGET)
    list(APPEND SC4_DLL_LINK_LIBS ${SC4_IMGUI_LINK_TARGET})
endif()

if(WIN32)
    list(APPEND SC4_DLL_LINK_LIBS kernel32 user32 Version dxgi ddraw dxguid)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE ${SC4_DLL_LINK_LIBS})

set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME "SC4TerrainExtensions"
    SUFFIX ".dll"
    PREFIX ""
)

if(MSVC)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()

if(WIN32)
    set(SC4_INSTALL_PATHS
        "C:/Program Files (x86)/SimCity 4 Deluxe Edition"
        "C:/Program Files (x86)/Maxis/SimCity 4 Deluxe"
        "C:/Program Files/Maxis/SimCity 4 Deluxe"
        "C:/Program Files (x86)/Steam/steamapps/common/SimCity 4 Deluxe"
        "C:/Program Files/Steam/steamapps/common/SimCity 4 Deluxe"
        "C:/Program Files (x86)/GOG Galaxy/Games/SimCity 4 Deluxe Edition"
        "C:/GOG Games/SimCity 4 Deluxe Edition"
    )

    foreach(SC4_PATH ${SC4_INSTALL_PATHS})
        if(EXISTS "${SC4_PATH}/Apps/SimCity 4.exe")
            set(SC4_INSTALL_DIR "${SC4_PATH}")
            break()
        endif()
    endforeach()

    set(SC4_PLUGINS_DIR "$ENV{USERPROFILE}/Documents/SimCity 4/Plugins")

    if(SC4_INSTALL_DIR)
        message(STATUS "Found SC4 installation at: ${SC4_INSTALL_DIR}")

        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SC4_PLUGINS_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:${PROJECT_NAME}>
                "${SC4_PLUGINS_DIR}/"
            COMMENT "Deploying SC4TerrainExtensions.dll to Plugins folder"
        )

        message(STATUS "Deployment targets configured:")
        message(STATUS "  - SC4TerrainExtensions.dll -> ${SC4_PLUGINS_DIR}/")
    else()
        message(WARNING "SC4 installation not found. Manual deployment required.")
    endif()
endif()

install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

if(EXISTS "${CMAKE_SOURCE_DIR}/docs")
    install(DIRECTORY docs/
        DESTINATION docs
        FILES_MATCHING PATTERN "*"
    )
endif()
