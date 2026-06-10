cmake_minimum_required(VERSION 3.14.0)

option(SMTG_ENABLE_VST3_PLUGIN_EXAMPLES "Enable VST 3 Plug-in Examples" OFF)
option(SMTG_ENABLE_VST3_HOSTING_EXAMPLES "Enable VST 3 Hosting Examples" OFF)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.13 CACHE STRING "")

set(vst3sdk_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../backends/vst3sdk" CACHE PATH "Path to the bundled VST3 SDK")
if(NOT vst3sdk_SOURCE_DIR)
    message(FATAL_ERROR "Path to VST3 SDK is empty!")
endif()

project(TEMPLATE
    # This is your plug-in version number. Change it here only.
    # Version number symbols usable in C++ can be found in
    # source/version.h and ${PROJECT_BINARY_DIR}/projectversion.h.
    VERSION 1.0.0.0 
    DESCRIPTION "TEMPLATE VST 3 Plug-in"
)

set(SMTG_VSTGUI_ROOT "${vst3sdk_SOURCE_DIR}")

add_subdirectory(${vst3sdk_SOURCE_DIR} ${PROJECT_BINARY_DIR}/vst3sdk)
smtg_enable_vst3_sdk()

smtg_add_vst3plugin(TEMPLATE
    source/version.h
    source/TPcids.h
    source/TPprocessor.h
    source/TPprocessor.cpp
    source/TPcontroller.h
    source/TPcontroller.cpp
    source/TPentry.cpp
)

#- VSTGUI Wanted ----
if(SMTG_ENABLE_VSTGUI_SUPPORT)
    target_sources(TEMPLATE
        PRIVATE
            resource/TPeditor.uidesc
    )
    target_link_libraries(TEMPLATE
        PRIVATE
            vstgui_support
    )
    smtg_target_add_plugin_resources(TEMPLATE
        RESOURCES
            "resource/TPeditor.uidesc"
    )
endif(SMTG_ENABLE_VSTGUI_SUPPORT)
# -------------------

smtg_target_add_plugin_snapshots (TEMPLATE
    RESOURCES
        resource/4C1869D37D355591AD0561280DEC98D7_snapshot.png
        resource/4C1869D37D355591AD0561280DEC98D7_snapshot_2.0x.png
)

target_link_libraries(TEMPLATE
    PRIVATE
        sdk
)

smtg_target_configure_version_file(TEMPLATE)

if(SMTG_MAC)
    smtg_target_set_bundle(TEMPLATE
        BUNDLE_IDENTIFIER Plugins
        COMPANY_NAME "Cloves Plugins"
    )
    smtg_target_set_debug_executable(TEMPLATE
        "/Applications/VST3PluginTestHost.app"
        "--pluginfolder;$(BUILT_PRODUCTS_DIR)"
    )
elseif(SMTG_WIN)
    target_sources(TEMPLATE PRIVATE 
        resource/win32resource.rc
    )
    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT TEMPLATE)

        smtg_target_set_debug_executable(TEMPLATE
            "$(ProgramW6432)/Steinberg/VST3PluginTestHost/VST3PluginTestHost.exe"
            "--pluginfolder \"$(OutDir)/\""
        )
    endif()
endif(SMTG_MAC)
