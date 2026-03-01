cmake_minimum_required(VERSION 3.20)
project(WinTools VERSION 1.1.0 LANGUAGES CXX)

set(WINTOOLS_APP_VERSION "${PROJECT_VERSION}")
set(WINTOOLS_APP_VERSION_OVERRIDE "" CACHE STRING "Override WinTools app version (for CI/release builds).")
if(NOT WINTOOLS_APP_VERSION_OVERRIDE STREQUAL "")
    set(WINTOOLS_APP_VERSION "${WINTOOLS_APP_VERSION_OVERRIDE}")
endif()

find_package(Git QUIET)
if(GIT_FOUND AND WINTOOLS_APP_VERSION_OVERRIDE STREQUAL "")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --exact-match
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE WINTOOLS_GIT_TAG_EXACT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE WINTOOLS_GIT_TAG_EXACT_RESULT
    )

    if(WINTOOLS_GIT_TAG_EXACT_RESULT EQUAL 0 AND NOT WINTOOLS_GIT_TAG_EXACT STREQUAL "")
        set(WINTOOLS_APP_VERSION "${WINTOOLS_GIT_TAG_EXACT}")
    endif()
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(WIN32 AND NOT Qt6_DIR AND NOT DEFINED CMAKE_PREFIX_PATH)
    set(_qt_hint_paths
        "C:/Qt/6.9.3/mingw_64/lib/cmake"
        "C:/Qt/6.9.2/mingw_64/lib/cmake"
        "C:/Qt/6.8.3/mingw_64/lib/cmake"
        "C:/Qt/6.8.2/mingw_64/lib/cmake"
        "C:/Qt/6.8.1/mingw_64/lib/cmake"
        "C:/Qt/6.8.0/mingw_64/lib/cmake"
        "C:/Qt/6.7.3/mingw_64/lib/cmake"
        "C:/Qt/6.7.2/mingw_64/lib/cmake"
        "C:/Qt/6.7.1/mingw_64/lib/cmake"
        "C:/Qt/6.7.0/mingw_64/lib/cmake"
        "C:/Qt/6.6.3/mingw_64/lib/cmake"
        "C:/Qt/6.6.2/mingw_64/lib/cmake"
        "C:/Qt/6.6.1/mingw_64/lib/cmake"
        "C:/Qt/6.6.0/mingw_64/lib/cmake"
        "C:/Qt/6.5.3/mingw_64/lib/cmake"
        "C:/Qt/6.5.2/mingw_64/lib/cmake"
        "C:/Qt/6.5.1/mingw_64/lib/cmake"
        "C:/Qt/6.5.0/mingw_64/lib/cmake"
    )

    foreach(_qt_path IN LISTS _qt_hint_paths)
        if(EXISTS "${_qt_path}/Qt6/Qt6Config.cmake" OR EXISTS "${_qt_path}/Qt6Config.cmake")
            list(APPEND CMAKE_PREFIX_PATH "${_qt_path}")
            message(STATUS "Using auto-detected Qt path: ${_qt_path}")
            break()
        endif()
    endforeach()
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Network Test Multimedia Concurrent)

configure_file(
    ${CMAKE_SOURCE_DIR}/src/common/build/wintools_version.hpp.in
    ${CMAKE_BINARY_DIR}/generated/wintools_version.hpp
    @ONLY
)

set(WINTOOLS_SOURCES
    src/main.cpp
    src/logger/logger.cpp
    src/logger/log_sink.cpp
    src/logger/ui/log_viewer_window.cpp
    src/common/display/dpi_utils.cpp
    src/common/display/monitors.cpp
    src/common/themes/window_colour.cpp
    src/common/themes/theme_helper.cpp
    src/common/themes/theme_listener.cpp
    src/common/themes/icon_helper.cpp
    src/common/themes/fluent_style.cpp
    src/common/hotkey/hotkey_engine.cpp
    src/common/hotkey/hotkey_config.cpp
    src/common/tray/tray_manager.cpp
    src/common/ui/console_manager.cpp
    src/common/ui/hotkey_bind_dialog.cpp
    src/common/ui/launch_page_section_factory.cpp
    src/common/ui/screen_relative_size.cpp
    src/common/ui/launch_page.cpp
    src/common/ui/settings_page.cpp
    src/common/ui/setup_wizard.cpp
    src/modules/mediabar/src/base/mediabar_controller.cpp
    src/modules/mediabar/src/app/app_main.cpp
    src/modules/mediabar/src/clients/unified_client.cpp
    src/modules/mediabar/src/clients/spotify_client.cpp
    src/modules/mediabar/src/clients/sonos_client.cpp
    src/modules/mediabar/src/core/config.cpp
    src/modules/mediabar/src/core/lyrics_timing.cpp
    src/modules/mediabar/src/services/lyrics_fetcher.cpp
    src/modules/mediabar/src/ui/gui.cpp
    src/modules/mediabar/src/ui/gui_setup.cpp
    src/modules/mediabar/src/ui/gui_playback.cpp
    src/modules/mediabar/src/ui/gui_sidebar.cpp
    src/modules/mediabar/src/ui/mini_player.cpp
    src/modules/disksentinel/src/scanner/disk_scanner.cpp
    src/modules/disksentinel/src/model/storage_model.cpp
    src/modules/disksentinel/src/ui/treemap_widget.cpp
    src/modules/disksentinel/src/ui/pie_chart_widget.cpp
    src/modules/disksentinel/src/ui/disk_sentinel_window.cpp
    src/modules/AdvancedTaskManager/src/core/process_monitor.cpp
    src/modules/AdvancedTaskManager/src/model/process_model.cpp
    src/modules/AdvancedTaskManager/src/ui/perf_graph_widget.cpp
    src/modules/AdvancedTaskManager/src/ui/profiler_overlay_controller.cpp
    src/modules/AdvancedTaskManager/src/ui/profiler_overlay_widget.cpp
    src/modules/AdvancedTaskManager/src/ui/task_manager_window.cpp
    src/modules/GameVault/src/core/game_library.cpp
    src/modules/GameVault/src/core/gamevault_settings.cpp
    src/modules/GameVault/src/core/playtime_tracker.cpp
    src/modules/GameVault/src/core/game_tag_store.cpp
    src/modules/GameVault/src/core/steamgriddb_client.cpp
    src/modules/GameVault/src/scanners/steam_scanner.cpp
    src/modules/GameVault/src/scanners/epic_scanner.cpp
    src/modules/GameVault/src/scanners/gog_scanner.cpp
    src/modules/GameVault/src/scanners/xbox_scanner.cpp
    src/modules/GameVault/src/scanners/retroarch_scanner.cpp
    src/modules/GameVault/src/scanners/emulator_scanners.cpp
    src/modules/GameVault/src/scanners/custom_folder_scanner.cpp
    src/modules/GameVault/src/model/game_model.cpp
    src/modules/GameVault/src/ui/game_card_delegate.cpp
    src/modules/GameVault/src/ui/gamevault_window.cpp
    src/modules/StreamVault/src/core/streamvault_settings.cpp
    src/modules/StreamVault/src/core/tmdb_client.cpp
    src/modules/StreamVault/src/core/watchlist_store.cpp
    src/modules/StreamVault/src/core/episode_progress_store.cpp
    src/modules/StreamVault/src/core/content_notification_service.cpp
    src/modules/StreamVault/src/model/stream_model.cpp
    src/modules/StreamVault/src/ui/stream_card_delegate.cpp
    src/modules/StreamVault/src/ui/streamvault_window.cpp
    src/modules/module_provider.cpp
    src/modules/AudioMaster/src/ui/audiomaster_window.cpp
    src/modules/AudioMaster/src/ui/virtual_devices_widget.cpp
    src/modules/AudioMaster/src/ui/vu_meter_widget.cpp
    src/modules/AudioMaster/src/core/audio_backend.cpp
    src/update/update.cpp
    resources/resources.qrc
)

if(WIN32)
    add_executable(WinTools WIN32 ${WINTOOLS_SOURCES})
elseif(APPLE)
    add_executable(WinTools MACOSX_BUNDLE ${WINTOOLS_SOURCES})
else()
    add_executable(WinTools ${WINTOOLS_SOURCES})
endif()

target_include_directories(WinTools PRIVATE
    ${CMAKE_BINARY_DIR}/generated
    src
    src/modules/MediaBar/src/app
    src/modules/MediaBar/src/core
    src/modules/MediaBar/src/ui
    src/modules/MediaBar/src/clients
    src/modules/MediaBar/src/services
)

target_link_libraries(WinTools PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Network Qt6::Multimedia Qt6::Concurrent)

if(WIN32)
    target_link_libraries(WinTools PRIVATE user32 psapi iphlpapi powrprof pdh advapi32 version)
elseif(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(COREAUDIO_FRAMEWORK CoreAudio)
    find_library(COREMEDIA_FRAMEWORK CoreMediaIO OPTIONAL)
    target_link_libraries(WinTools PRIVATE
        ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${COREAUDIO_FRAMEWORK})
    if(COREMEDIA_FRAMEWORK)
        target_link_libraries(WinTools PRIVATE ${COREMEDIA_FRAMEWORK})
    endif()
elseif(UNIX)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(PIPEWIRE QUIET libpipewire-0.3)
        pkg_check_modules(X11 QUIET x11)
    endif()
    if(PIPEWIRE_FOUND)
        target_include_directories(WinTools PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
        target_link_libraries(WinTools PRIVATE ${PIPEWIRE_LIBRARIES})
    endif()
    if(X11_FOUND)
        target_link_libraries(WinTools PRIVATE ${X11_LIBRARIES})
    endif()
endif()

if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE
        NAMES windeployqt windeployqt.exe
        HINTS
            "C:/Qt/6.9.3/mingw_64/bin"
            "C:/Qt/6.9.2/mingw_64/bin"
            "C:/Qt/6.8.3/mingw_64/bin"
            "C:/Qt/6.8.2/mingw_64/bin"
            "C:/Qt/6.8.1/mingw_64/bin"
            "C:/Qt/6.8.0/mingw_64/bin"
            "C:/Qt/6.7.3/mingw_64/bin"
            "C:/Qt/6.7.2/mingw_64/bin"
            "C:/Qt/6.7.1/mingw_64/bin"
            "C:/Qt/6.7.0/mingw_64/bin"
            "C:/Qt/6.6.3/mingw_64/bin"
            "C:/Qt/6.6.2/mingw_64/bin"
            "C:/Qt/6.6.1/mingw_64/bin"
            "C:/Qt/6.6.0/mingw_64/bin"
            "C:/Qt/6.5.3/mingw_64/bin"
            "C:/Qt/6.5.2/mingw_64/bin"
            "C:/Qt/6.5.1/mingw_64/bin"
            "C:/Qt/6.5.0/mingw_64/bin"
    )

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET WinTools POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --compiler-runtime "$<TARGET_FILE:WinTools>"
            COMMENT "Deploying Qt runtime dependencies for WinTools"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt not found; WinTools.exe may fail to start outside a Qt-enabled PATH.")
    endif()
endif()

if(MSVC)
    target_compile_options(WinTools PRIVATE /W4)
else()
    target_compile_options(WinTools PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(MediaBarLyricsWindowTests
        tests/mediabar/lyrics_window_tests.cpp
        src/modules/mediabar/src/ui/gui.cpp
        src/modules/mediabar/src/ui/gui_setup.cpp
        src/modules/mediabar/src/ui/gui_playback.cpp
        src/modules/mediabar/src/ui/gui_sidebar.cpp
        src/modules/mediabar/src/core/config.cpp
        src/logger/logger.cpp
        src/logger/log_sink.cpp
        src/common/themes/fluent_style.cpp
        src/common/themes/theme_helper.cpp
        src/common/themes/theme_listener.cpp
        src/common/themes/window_colour.cpp
    )

    target_include_directories(MediaBarLyricsWindowTests PRIVATE
        src
        src/modules/MediaBar/src/core
        src/modules/MediaBar/src/ui
    )

    target_link_libraries(MediaBarLyricsWindowTests PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Test
    )

    add_test(NAME MediaBarLyricsWindowTests.hasCoreControlsAndDefaultSourceMode COMMAND MediaBarLyricsWindowTests hasCoreControlsAndDefaultSourceMode)
    add_test(NAME MediaBarLyricsWindowTests.sourceModeSelectionSyncsButtons COMMAND MediaBarLyricsWindowTests sourceModeSelectionSyncsButtons)
    add_test(NAME MediaBarLyricsWindowTests.sidebarToggleCollapsesAndRestoresLabels COMMAND MediaBarLyricsWindowTests sidebarToggleCollapsesAndRestoresLabels)
    add_test(NAME MediaBarLyricsWindowTests.updateTrackInfoSetsFallbacksWhenNoArt COMMAND MediaBarLyricsWindowTests updateTrackInfoSetsFallbacksWhenNoArt)
    add_test(NAME MediaBarLyricsWindowTests.playPauseButtonIconReflectsPlaybackState COMMAND MediaBarLyricsWindowTests playPauseButtonIconReflectsPlaybackState)
    add_test(NAME MediaBarLyricsWindowTests.progressFormattingAndClampingWorks COMMAND MediaBarLyricsWindowTests progressFormattingAndClampingWorks)
    add_test(NAME MediaBarLyricsWindowTests.favoriteStateReflectsSpotifyAvailability COMMAND MediaBarLyricsWindowTests favoriteStateReflectsSpotifyAvailability)
    add_test(NAME MediaBarLyricsWindowTests.shuffleStateReflectsSupportAndEnabledStatus COMMAND MediaBarLyricsWindowTests shuffleStateReflectsSupportAndEnabledStatus)
    add_test(NAME MediaBarLyricsWindowTests.displayLyricsAndHighlightingStylesLines COMMAND MediaBarLyricsWindowTests displayLyricsAndHighlightingStylesLines)
    add_test(NAME MediaBarLyricsWindowTests.showNoLyricsOnlyWhenLyricsAreEmpty COMMAND MediaBarLyricsWindowTests showNoLyricsOnlyWhenLyricsAreEmpty)
    add_test(NAME MediaBarLyricsWindowTests.showWaitingResetsUiToIdleState COMMAND MediaBarLyricsWindowTests showWaitingResetsUiToIdleState)

    add_executable(MediaBarUnifiedClientTests
        tests/mediabar/unified_client_integration_tests.cpp
        src/modules/mediabar/src/clients/unified_client.cpp
        src/modules/mediabar/src/clients/spotify_client.cpp
        src/modules/mediabar/src/clients/sonos_client.cpp
        src/modules/mediabar/src/core/config.cpp
        src/logger/logger.cpp
        src/logger/log_sink.cpp
    )

    target_compile_definitions(MediaBarUnifiedClientTests PRIVATE MEDIABAR_ENABLE_TEST_HOOKS)

    target_include_directories(MediaBarUnifiedClientTests PRIVATE
        src
        src/modules/MediaBar/src/core
        src/modules/MediaBar/src/clients
    )

    target_link_libraries(MediaBarUnifiedClientTests PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Network
        Qt6::Test
    )

    add_test(NAME MediaBarUnifiedClientTests.spotifyModeReturnsSpotifyPlayback COMMAND MediaBarUnifiedClientTests spotifyModeReturnsSpotifyPlayback)
    add_test(NAME MediaBarUnifiedClientTests.sonosModeReturnsSonosPlayback COMMAND MediaBarUnifiedClientTests sonosModeReturnsSonosPlayback)
    add_test(NAME MediaBarUnifiedClientTests.forcedSourceModeUsesSelectedProvider COMMAND MediaBarUnifiedClientTests forcedSourceModeUsesSelectedProvider)
    add_test(NAME MediaBarUnifiedClientTests.transportRoutesToCurrentSource COMMAND MediaBarUnifiedClientTests transportRoutesToCurrentSource)
    add_test(NAME MediaBarUnifiedClientTests.shuffleBehaviorIsSpotifyOnly COMMAND MediaBarUnifiedClientTests shuffleBehaviorIsSpotifyOnly)

    add_executable(MediaBarLyricsTimingTests
        tests/mediabar/lyrics_timing_tests.cpp
        src/modules/mediabar/src/core/lyrics_timing.cpp
    )

    target_include_directories(MediaBarLyricsTimingTests PRIVATE
        src/modules/MediaBar/src/core
    )

    target_link_libraries(MediaBarLyricsTimingTests PRIVATE
        Qt6::Core
        Qt6::Test
    )

    add_test(NAME MediaBarLyricsTimingTests COMMAND MediaBarLyricsTimingTests)
endif()
