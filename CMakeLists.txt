cmake_minimum_required(VERSION 3.20)
project(WinToolsNative VERSION 0.1.0 LANGUAGES CXX)

set(WINTOOLS_APP_VERSION "${PROJECT_VERSION}")
find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --abbrev=0
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE WINTOOLS_GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE WINTOOLS_GIT_TAG_RESULT
    )
    if(WINTOOLS_GIT_TAG_RESULT EQUAL 0 AND NOT WINTOOLS_GIT_TAG STREQUAL "")
        set(WINTOOLS_APP_VERSION "${WINTOOLS_GIT_TAG}")
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

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Sql Network Test)

add_executable(WinToolsNative WIN32
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
    src/common/ui/theme_manager.cpp
    src/common/ui/hotkey_bind_dialog.cpp
    src/common/ui/launch_page_section_factory.cpp
    src/common/ui/screen_relative_size.cpp
    src/common/ui/launch_page.cpp
    src/modules/mediabar/src/base/mediabar_controller.cpp
    src/modules/mediabar/src/app/app_main.cpp
    src/modules/mediabar/src/clients/unified_client.cpp
    src/modules/mediabar/src/clients/spotify_client.cpp
    src/modules/mediabar/src/clients/sonos_client.cpp
    src/modules/mediabar/src/core/config.cpp
    src/modules/mediabar/src/core/lyrics_timing.cpp
    src/modules/mediabar/src/services/lyrics_fetcher.cpp
    src/modules/mediabar/src/ui/gui.cpp
    src/modules/mediabar/src/ui/mini_player.cpp
    src/modules/disksentinel/src/scanner/disk_scanner.cpp
    src/modules/disksentinel/src/model/storage_model.cpp
    src/modules/disksentinel/src/ui/treemap_widget.cpp
    src/modules/disksentinel/src/ui/disk_sentinel_window.cpp
    src/modules/AdvancedTaskManager/src/core/process_monitor.cpp
    src/modules/AdvancedTaskManager/src/model/process_model.cpp
    src/modules/AdvancedTaskManager/src/ui/perf_graph_widget.cpp
    src/modules/AdvancedTaskManager/src/ui/task_manager_window.cpp
    src/modules/GameVault/src/core/game_library.cpp
    src/modules/GameVault/src/core/gamevault_settings.cpp
    src/modules/GameVault/src/core/playtime_tracker.cpp
    src/modules/GameVault/src/scanners/steam_scanner.cpp
    src/modules/GameVault/src/scanners/epic_scanner.cpp
    src/modules/GameVault/src/scanners/gog_scanner.cpp
    src/modules/GameVault/src/scanners/retroarch_scanner.cpp
    src/modules/GameVault/src/scanners/emulator_scanners.cpp
    src/modules/GameVault/src/scanners/custom_folder_scanner.cpp
    src/modules/GameVault/src/model/game_model.cpp
    src/modules/GameVault/src/ui/game_card_delegate.cpp
    src/modules/GameVault/src/ui/gamevault_window.cpp
    src/modules/StreamVault/src/core/streamvault_settings.cpp
    src/modules/StreamVault/src/core/tmdb_client.cpp
    src/modules/StreamVault/src/model/stream_model.cpp
    src/modules/StreamVault/src/ui/stream_card_delegate.cpp
    src/modules/StreamVault/src/ui/streamvault_window.cpp
    src/modules/module_provider.cpp
    src/update/update.cpp
    resources/resources.qrc
)

target_include_directories(WinToolsNative PRIVATE
    src
    src/modules/MediaBar/src/app
    src/modules/MediaBar/src/core
    src/modules/MediaBar/src/ui
    src/modules/MediaBar/src/clients
    src/modules/MediaBar/src/services
)

target_compile_definitions(WinToolsNative PRIVATE
    WINTOOLS_APP_VERSION="${WINTOOLS_APP_VERSION}"
)

target_link_libraries(WinToolsNative PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Sql Qt6::Network)

if(WIN32)
    target_link_libraries(WinToolsNative PRIVATE user32 psapi iphlpapi powrprof)
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
        add_custom_command(TARGET WinToolsNative POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-translations --compiler-runtime "$<TARGET_FILE:WinToolsNative>"
            COMMENT "Deploying Qt runtime dependencies for WinToolsNative"
            VERBATIM
        )
    else()
        message(WARNING "windeployqt not found; WinToolsNative.exe may fail to start outside a Qt-enabled PATH.")
    endif()
endif()

if(MSVC)
    target_compile_options(WinToolsNative PRIVATE /W4)
else()
    target_compile_options(WinToolsNative PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(MediaBarLyricsWindowTests
        tests/mediabar/lyrics_window_tests.cpp
        src/modules/mediabar/src/ui/gui.cpp
        src/modules/mediabar/src/core/config.cpp
        src/logger/logger.cpp
        src/logger/log_sink.cpp
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

    add_test(NAME MediaBarUnifiedClientTests.autoModePrefersActiveSpotify COMMAND MediaBarUnifiedClientTests autoModePrefersActiveSpotify)
    add_test(NAME MediaBarUnifiedClientTests.autoModePrefersActiveSonosWhenSpotifyPaused COMMAND MediaBarUnifiedClientTests autoModePrefersActiveSonosWhenSpotifyPaused)
    add_test(NAME MediaBarUnifiedClientTests.forcedSourceModeUsesSelectedProvider COMMAND MediaBarUnifiedClientTests forcedSourceModeUsesSelectedProvider)
    add_test(NAME MediaBarUnifiedClientTests.transportRoutesToCurrentSourceAndFallbacks COMMAND MediaBarUnifiedClientTests transportRoutesToCurrentSourceAndFallbacks)
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
