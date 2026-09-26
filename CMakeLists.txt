cmake_minimum_required(VERSION 3.21)

project(
    InventoryInjectorImproved
    VERSION 0.1.1
    DESCRIPTION "Fixes and improves I4 (Inventory Interface Information Injector)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PLUGIN_TESTS_ONLY "Build unit tests instead of the plugin" OFF)
option(I5_FRAME_PROBE "Build the per-frame spike probe (development diagnostics)" OFF)

if(PLUGIN_TESTS_ONLY)

    find_package(Catch2 3 CONFIG REQUIRED)

    include(CTest)
    include(Catch)

    add_executable(
        ${PROJECT_NAME}Tests
        test/CacheKeyTests.cpp
        test/CacheDeltaTests.cpp
        test/CacheSerializationTests.cpp
        test/ConfigHashTests.cpp
        test/ConsoleCommandTests.cpp
        test/CacheFingerprintTests.cpp
        test/FrameSpikeTests.cpp
        test/CategoryFlagsTests.cpp
        test/InvalidateFingerprintTests.cpp
        test/SkyUIConfigTests.cpp)

    target_include_directories(${PROJECT_NAME}Tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")

    target_link_libraries(${PROJECT_NAME}Tests PRIVATE Catch2::Catch2WithMain)

    catch_discover_tests(${PROJECT_NAME}Tests)

else()

    # Auto-init submodules if inside a git repo (self-heals if user skipped init.sh)
    find_package(Git QUIET)
    if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE _submod_result)
        if(NOT _submod_result EQUAL 0)
            message(FATAL_ERROR "git submodule update failed (exit code ${_submod_result})")
        endif()
    endif()

    # Dependencies

    find_package(spdlog CONFIG REQUIRED)
    # directxtk must be found before add_commonlibsse_plugin (CommonLibSSE-NG links
    # Microsoft::DirectXTK transitively).
    find_package(directxtk CONFIG REQUIRED)
    # rapidcsv must be in vcpkg.json (CommonLibSSE-NG unconditionally adds it as an include dir)

    # CommonLibSSE-NG

    set(ENABLE_SKYRIM_SE
        ON
        CACHE BOOL "" FORCE)
    set(ENABLE_SKYRIM_AE
        ON
        CACHE BOOL "" FORCE)
    set(ENABLE_SKYRIM_VR
        OFF
        CACHE BOOL "" FORCE)
    set(BUILD_TESTS
        OFF
        CACHE BOOL "" FORCE)
    set(SKSE_SUPPORT_XBYAK
        OFF
        CACHE BOOL "" FORCE)

    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlibsse-ng" "${CMAKE_CURRENT_BINARY_DIR}/commonlibsse-ng"
                     EXCLUDE_FROM_ALL)

    include("${CMAKE_CURRENT_SOURCE_DIR}/lib/commonlibsse-ng/cmake/CommonLibSSE.cmake")

    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/src")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/src/version.rc" @ONLY)

    # Plugin — auto-generates SKSEPlugin_Version; Plugin.cpp only needs SKSEPlugin_Load
    add_commonlibsse_plugin(
        ${PROJECT_NAME}
        NAME
        "InventoryInjectorImproved"
        AUTHOR
        "codepuncher"
        SOURCES
        src/Plugin.cpp
        src/I4Hook.cpp
        src/ConsoleHook.cpp
        src/FavoritesCrashFix.cpp
        src/InvalidateListFix.cpp
        src/InvalidateMemo.cpp
        "${CMAKE_CURRENT_BINARY_DIR}/src/version.rc"
        USE_ADDRESS_LIBRARY)

    source_group("Resources" FILES "${CMAKE_CURRENT_BINARY_DIR}/src/version.rc")

    # cmake_llvm_rc preprocesses RC files using the target's INCLUDE_DIRECTORIES, not CMAKE_C_FLAGS.
    # The xwin SDK paths are in CMAKE_C_FLAGS_INIT as /imsvc flags and are invisible to cmake_llvm_rc,
    # so we add them explicitly here for the RC compiler to find winres.h.
    if(DEFINED XWIN_DIR)
        target_include_directories(
            ${PROJECT_NAME} SYSTEM PRIVATE "$<$<COMPILE_LANGUAGE:RC>:${XWIN_DIR}/sdk/include/um>"
                                           "$<$<COMPILE_LANGUAGE:RC>:${XWIN_DIR}/sdk/include/shared>")
    endif()

    target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

    if(I5_FRAME_PROBE)
        target_sources(${PROJECT_NAME} PRIVATE src/FrameProbe.cpp)
        target_compile_definitions(${PROJECT_NAME} PRIVATE I5_FRAME_PROBE)
    endif()

    target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
                                                       "${CMAKE_CURRENT_BINARY_DIR}/src")

    set_target_properties(
        ${PROJECT_NAME}
        PROPERTIES OUTPUT_NAME InventoryInjectorImproved
                   PREFIX ""
                   SUFFIX ".dll")

    # Compiler/linker flags: /Zi emits PDB in all configs; Release gets full optimization + /DEBUG:FULL
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /Zi "$<$<CONFIG:Release>:/Zc:inline;/JMC->")
        target_link_options(${PROJECT_NAME} PRIVATE "$<$<CONFIG:Debug>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
                            "$<$<CONFIG:Release>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>")
    endif()

    # LTO/IPO — guarded: unconditionally setting IPO causes a fatal error on toolchains that don't support it
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_supported OUTPUT _ipo_output)
    if(_ipo_supported)
        set_target_properties(${PROJECT_NAME} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    else()
        message(VERBOSE "IPO not supported: ${_ipo_output}")
    endif()

    # Deploy: copies DLL + PDB to SKYRIM_MODS_FOLDER, SKYRIM_FOLDER/Data, or build/deploy/ as fallback

    if(DEFINED ENV{SKYRIM_MODS_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_MODS_FOLDER}")
        set(_default_deploy "$ENV{SKYRIM_MODS_FOLDER}/${PROJECT_NAME}")
    elseif(DEFINED ENV{SKYRIM_FOLDER} AND IS_DIRECTORY "$ENV{SKYRIM_FOLDER}/Data")
        set(_default_deploy "$ENV{SKYRIM_FOLDER}/Data")
    else()
        set(_default_deploy "${CMAKE_BINARY_DIR}/deploy/${PROJECT_NAME}")
    endif()

    set(DEPLOY_DIR
        "${_default_deploy}"
        CACHE PATH "Deploy destination (set SKYRIM_MODS_FOLDER or SKYRIM_FOLDER in .env)")

    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION "${DEPLOY_DIR}/SKSE/Plugins")

    install(
        FILES "$<TARGET_PDB_FILE:${PROJECT_NAME}>"
        DESTINATION "${DEPLOY_DIR}/SKSE/Plugins"
        OPTIONAL)

    # `cmake --build --preset deploy`  (or  cmake --workflow --preset deploy)
    add_custom_target(
        deploy
        COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
        DEPENDS ${PROJECT_NAME}
        COMMENT "Deploying to ${DEPLOY_DIR}/SKSE/Plugins")

endif() # PLUGIN_TESTS_ONLY
