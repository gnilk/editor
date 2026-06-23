#
# GoatEdit - CMakeLists.txt for Linux and macOS
# Note: I throughly dislike CMake but I find it better than plain makefiles so that's it..
#
cmake_minimum_required(VERSION 3.28)
project(goatedit VERSION 0.1.0)
set(CMAKE_CXX_STANDARD 20)

# ---------------------------------------------------------------------------
# Version — single source of truth is project(VERSION) above.
# GEDIT_VERSION_SUFFIX: empty on tag builds, "-dev[+sha]" otherwise.
# Bundle/package use clean ${PROJECT_VERSION}; GEDIT_VERSION_STR is display-only.
# ---------------------------------------------------------------------------
if (DEFINED ENV{GITHUB_REF} AND "$ENV{GITHUB_REF}" MATCHES "refs/tags/")
    set(GEDIT_VERSION_SUFFIX "")
else()
    set(GEDIT_VERSION_SUFFIX "-dev")
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GEDIT_GIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if (GEDIT_GIT_SHA)
        string(APPEND GEDIT_VERSION_SUFFIX "+${GEDIT_GIT_SHA}")
    endif()
endif()
configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/Version.h.in
    ${CMAKE_BINARY_DIR}/generated/Version.h
    @ONLY
)

# When changing the Assets/CMake/Info.plist.in - which will rebuild the bundle Info.plist we must
# reset the cache in macos for this to take effect...
# /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -kill -seed

set(EDITOR_ASSET_DIR ${CMAKE_CURRENT_BINARY_DIR}/resources)

message(STATUS "Assets directory: ${EDITOR_ASSET_DIR}")
if (NOT EXISTS ${EDITOR_ASSET_DIR})
    file(MAKE_DIRECTORY ${EDITOR_ASSET_DIR})
endif ()

if (APPLE)
    option(GEDIT_BUILD_SDL3 "Build SDL3" ON)
    option(GEDIT_BUILD_SDL2 "Build SDL2" OFF)
else()
    option(GEDIT_BUILD_SDL3 "Build SDL3" OFF)
    option(GEDIT_BUILD_SDL2 "Build SDL2" ON)
endif()
    #set(SDL_HOME /usr/local)
#set(SDL_HOME /opt/homebrew)


list(APPEND appsrc main.cpp)

# Vendored: duktape (pre-processed from upstream v2.7.0; stays vendored — see docs/cmake-cleanup.md §2)
set(DUKTAPE_HOME src/ext/duktape-2.7.0/src)
set(DUKTAPE_EXTRAS src/ext/duktape-2.7.0/extras)
list(APPEND duktape ${DUKTAPE_HOME}/duktape.c ${DUKTAPE_HOME}/duk_config.h ${DUKTAPE_HOME}/duktape.h)
list(APPEND duktape ${DUKTAPE_EXTRAS}/module-node/duk_module_node.c)

# Fetch / configure source dependencies and locate trun testrunner headers
include(cmake/CMakeDeps.cmake)

set(CUR_TARGET goatedit)

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")

    add_definitions(-DGEDIT_MACOS)

    # Detect Homebrew prefix — works for both Apple Silicon (/opt/homebrew) and Intel (/usr/local).
    # Setting CMAKE_PREFIX_PATH here means find_package(yaml-cpp), find_package(SDL3/SDL2), etc.
    # all resolve without per-package path hints, regardless of where brew is installed.
    find_program(BREW brew)
    if (BREW)
        execute_process(COMMAND ${BREW} --prefix
            OUTPUT_VARIABLE HOMEBREW_PREFIX
            OUTPUT_STRIP_TRAILING_WHITESPACE)
        list(PREPEND CMAKE_PREFIX_PATH "${HOMEBREW_PREFIX}")
        message(STATUS "Homebrew prefix: ${HOMEBREW_PREFIX}")
    else()
        message(WARNING "brew not found — set CMAKE_PREFIX_PATH to your package manager prefix if deps are missing")
    endif()

    list(APPEND platformsrc src/Core/macOS/MacOSKBEmulator.cpp src/Core/macOS/MacOSKBEmulator.h)
    list(APPEND platformsrc src/Core/macOS/MacOSFolderMonitor.cpp src/Core/macOS/MacOSFolderMonitor.h)
    # list(APPEND platformsrc src/Core/macOS/MacOSKeyboardMonitor.cpp src/Core/macOS/MacOSKeyboardMonitor.h)
    list(APPEND platformsrc src/Core/unix/Shell.cpp src/Core/unix/Shell.h)

    # CoreServices — MacOSFolderMonitor calls FSEventStream* directly
    # CoreFoundation — Editor.cpp calls CF* directly
    # IOKit / Cocoa / Carbon — not directly called by our code; SDL3 brings them in transitively
    find_library(CORE_SERVICES CoreServices REQUIRED)
    find_library(CORE_FOUNDATION CoreFoundation REQUIRED)

    list(APPEND extlibs yaml-cpp::yaml-cpp ${CORE_SERVICES} ${CORE_FOUNDATION})


    set(ICON_FILE ${CMAKE_SOURCE_DIR}/Assets/CMake/GoatEdit.icns)
    set(PLIST_FILE_IN ${CMAKE_SOURCE_DIR}/Assets/CMake/Info.plist.in)

    # for MacOS add - mark as 'MACOSX_BUNDLE' and add the ICON as well
    add_executable(${CUR_TARGET} MACOSX_BUNDLE ${appsrc} ${ICON_FILE})
    # Now, set some properties to tell CMake where it should go..
    set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    # When changing the Assets/CMake/Info.plist.in - which will rebuild the bundle Info.plist we must
    # reset the cache in macos for this to take effect...
    # /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -kill -seed

    set_target_properties(${CUR_TARGET} PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER gnilk.com.${CUR_TARGET}
            MACOSX_BUNDLE_BUNDLE_NAME ${CUR_TARGET}
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_ICON_FILE GoatEdit
            MACOSX_BUNDLE_INFO_PLIST ${PLIST_FILE_IN}
            )

    # This copies the 'goatedit' (shared resource folder) in to the application bundle...
    # Don't know if this is possible with some other CMake command - perhaps it is - this did work for me...
    add_custom_command(
            TARGET ${CUR_TARGET}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory ${EDITOR_ASSET_DIR} $<TARGET_FILE_DIR:${CUR_TARGET}>/../SharedSupport
    )


    install(TARGETS ${CUR_TARGET} BUNDLE DESTINATION .)
    install(DIRECTORY ${EDITOR_ASSET_DIR}/
            DESTINATION ${CUR_TARGET}.app/Contents/SharedSupport)

    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_PACKAGE_NAME "GoatEdit")
    set(CPACK_PACKAGE_VENDOR "gnilk")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_FILE_NAME "GoatEdit-${PROJECT_VERSION}-macOS")
    set(CPACK_DMG_VOLUME_NAME "GoatEdit ${PROJECT_VERSION}")
    set(CPACK_PACKAGE_EXECUTABLES "${CUR_TARGET};GoatEdit")
    # Codesigning/notarisation: optional follow-up for distribution outside personal use.
    # Requires an Apple Developer ID certificate; see CPACK_BUNDLE_APPLE_CERT_APP + notarytool.
    include(CPack)
elseif(UNIX)
    message(STATUS "Linux build")

    list(APPEND platformsrc src/Core/unix/Shell.cpp src/Core/unix/Shell.h)
    list(APPEND platformsrc src/Core/Linux/LinuxFolderMonitor.cpp src/Core/Linux/LinuxFolderMonitor.h)

    add_definitions(-DGEDIT_LINUX)

    list(APPEND extlibs util)   ## see: man forkpty
    list(APPEND extlibs yaml-cpp::yaml-cpp)
    add_executable(${CUR_TARGET} ${appsrc})

    #
    # Linux packaging — .deb via CPack (AppImage is built separately, see scripts/build-appimage.sh).
    #
    include(GNUInstallDirs)
    install(TARGETS ${CUR_TARGET} RUNTIME DESTINATION bin)
    install(DIRECTORY ${EDITOR_ASSET_DIR}/. DESTINATION ${CMAKE_INSTALL_DATADIR}/goatedit)
    install(FILES ${CMAKE_SOURCE_DIR}/Assets/CMake/com.gnilk.goatedit.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    # Scalable SVG is the primary icon; the 512x512 PNG is the raster fallback (scalable/ is
    # for SVG only, so the PNG goes in the matching size dir).
    install(FILES ${CMAKE_SOURCE_DIR}/Assets/Graphics/com.gnilk.goatedit.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps)
    install(FILES ${CMAKE_SOURCE_DIR}/Assets/Graphics/com.gnilk.goatedit.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps)

    SET(CPACK_GENERATOR "DEB")
    SET(CPACK_PACKAGE_NAME "goatedit")
    SET(CPACK_PACKAGE_VENDOR "gnilk")
    SET(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    SET(CPACK_PACKAGE_FILE_NAME "goatedit-${PROJECT_VERSION}-linux-${CMAKE_SYSTEM_PROCESSOR}")
    SET(CPACK_PACKAGE_EXECUTABLES goatedit;GoatEdit)
    SET(CPACK_PACKAGE_DESCRIPTION "A text editor for my own personal use..")
    SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "not a very fancy text/code editor..")
    SET(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/gnilk/editor")
    SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "gnilk") #required
    SET(CPACK_DEBIAN_PACKAGE_SECTION "editors")
    # SHLIBDEPS auto-resolves runtime deps from the linked .so set (libsdl2, libyaml-cpp,
    # libstdc++, libc6, ...) via dpkg-shlibdeps — more robust than a hand-maintained list.
    SET(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
    INCLUDE(CPack)

endif ()

# yaml-cpp — found via CMAKE_PREFIX_PATH (set from brew --prefix on macOS; standard paths on Linux)
find_package(yaml-cpp REQUIRED)

# Per-compiler warning flags, build-type optimisation, and sanitiser option
include(cmake/CMakeBuildFlags.cmake)

target_compile_definitions(${CUR_TARGET} PUBLIC GEDIT_APP_NAME=GoatEdit)
target_include_directories(${CUR_TARGET} PUBLIC ${CMAKE_BINARY_DIR}/generated)


#
# Source lists
#

# ---------------------------------------------------------------------------
# coresrc — Core foundation: data model, config/theme/session, language/syntax,
#            util/primitives, app glue, input handling
# ---------------------------------------------------------------------------

# stb single-header implementations (one TU per binary)
list(APPEND coresrc src/ext/stb/stb_impl.cpp)

# App glue + data model
list(APPEND coresrc src/Core/AssetLoaderBase.cpp src/Core/AssetLoaderBase.h)
list(APPEND coresrc src/Core/ClipBoard.cpp src/Core/ClipBoard.h)
list(APPEND coresrc src/Core/Editor.cpp src/Core/Editor.h)
list(APPEND coresrc src/Core/Document.cpp src/Core/Document.h)
list(APPEND coresrc src/Core/DocumentViewMode.h)
list(APPEND coresrc src/Core/DocumentViewState.cpp src/Core/DocumentViewState.h)
list(APPEND coresrc src/Core/BinBuffer.h)
list(APPEND coresrc src/Core/ByteStreamReader.cpp src/Core/ByteStreamReader.h)
list(APPEND coresrc src/Core/HexProjection.cpp src/Core/HexProjection.h)
list(APPEND coresrc src/Core/FolderMonitor.cpp src/Core/FolderMonitor.h)
list(APPEND coresrc src/Core/FileSystem/FolderScanner.cpp src/Core/FileSystem/FolderScanner.h)
list(APPEND coresrc src/Core/FileSystem/FsFilter.cpp src/Core/FileSystem/FsFilter.h)
list(APPEND coresrc src/Core/Runloop.cpp src/Core/Runloop.h)
list(APPEND coresrc src/Core/VerticalNavigationViewModel.cpp src/Core/VerticalNavigationViewModel.h)
list(APPEND coresrc src/Core/Workspace.cpp src/Core/Workspace.h)

# Actions, key mapping, plugin entry points
list(APPEND coresrc src/Core/Action.cpp src/Core/Action.h)
list(APPEND coresrc src/Core/ActionHelper.cpp src/Core/ActionHelper.h)
list(APPEND coresrc src/Core/KeyMapping.cpp src/Core/KeyMapping.h)
list(APPEND coresrc src/Core/KeyMappingCache.cpp src/Core/KeyMappingCache.h)
list(APPEND coresrc src/Core/API/DocumentAPI.cpp src/Core/API/DocumentAPI.h)
list(APPEND coresrc src/Core/API/EditorAPI.cpp src/Core/API/EditorAPI.h)
list(APPEND coresrc src/Core/API/NamedColorsAPI.cpp src/Core/API/NamedColorsAPI.h)
list(APPEND coresrc src/Core/API/ThemeAPI.cpp src/Core/API/ThemeAPI.h)
# list(APPEND coresrc src/Core/API/TextBufferAPI.cpp src/Core/API/TextBufferAPI.h)
list(APPEND coresrc src/Core/API/ViewAPI.cpp src/Core/API/ViewAPI.h)
list(APPEND coresrc src/Core/Plugins/PluginCommand.cpp src/Core/Plugins/PluginCommand.h)
list(APPEND coresrc src/Core/Plugins/PluginExecutor.cpp src/Core/Plugins/PluginExecutor.h)
list(APPEND coresrc src/Core/KeypressAndActionHandler.h)

# Utility + primitives
list(APPEND coresrc src/Core/backward.cpp src/Core/backward.hpp)
list(APPEND coresrc src/Core/ConvertUTF.cpp src/Core/ConvertUTF.h)
list(APPEND coresrc src/Core/ColorRGBA.cpp src/Core/ColorRGBA.h)
list(APPEND coresrc src/Core/DurationTimer.cpp src/Core/DurationTimer.h)
list(APPEND coresrc src/Core/HexDump.cpp src/Core/HexDump.h)
list(APPEND coresrc src/Core/Glob.cpp src/Core/Glob.h)
list(APPEND coresrc src/Core/Job.h)
list(APPEND coresrc src/Core/NamedColors.cpp src/Core/NamedColors.h)
list(APPEND coresrc src/Core/PathUtil.cpp src/Core/PathUtil.h)
list(APPEND coresrc src/Core/StrUtil.cpp src/Core/StrUtil.h)
list(APPEND coresrc src/Core/Timer.cpp src/Core/Timer.h)
list(APPEND coresrc src/Core/TypeUtil.h)
list(APPEND coresrc src/Core/Point.h)
list(APPEND coresrc src/Core/Rect.h)
list(APPEND coresrc src/Core/UndoHistory.cpp src/Core/UndoHistory.h)

# Terminal emulation
list(APPEND coresrc src/Core/VTermParser.cpp src/Core/VTermParser.h)
list(APPEND coresrc src/Core/TerminalScreen.cpp src/Core/TerminalScreen.h)
list(APPEND coresrc src/Core/TerminalCmdHistory.cpp src/Core/TerminalCmdHistory.h)
list(APPEND coresrc src/Core/UnicodeHelper.cpp src/Core/UnicodeHelper.h)

# Input handling
list(APPEND coresrc src/Core/KeyPress.cpp src/Core/KeyPress.h)
list(APPEND coresrc src/Core/Keyboard.cpp src/Core/Keyboard.h)

# Config, theme, session
list(APPEND coresrc src/Core/Config/Config.cpp src/Core/Config/Config.h)
list(APPEND coresrc src/Core/Config/ConfigNode.cpp src/Core/Config/ConfigNode.h)
list(APPEND coresrc src/Core/Config/YAMLMerge.cpp src/Core/Config/YAMLMerge.h)
list(APPEND coresrc src/Core/Theme/Theme.cpp src/Core/Theme/Theme.h)
list(APPEND coresrc src/Core/RuntimeConfig.cpp src/Core/RuntimeConfig.h)
list(APPEND coresrc src/Core/XDGEnvironment.cpp src/Core/XDGEnvironment.h)
list(APPEND coresrc src/Core/Session/SessionManager.cpp src/Core/Session/SessionManager.h)
list(APPEND coresrc src/Core/Session/SessionState.h)
list(APPEND coresrc src/Core/Session/SessionSerializer.cpp src/Core/Session/SessionSerializer.h)

# Text buffer + language / syntax highlighting
list(APPEND coresrc src/Core/Line.cpp src/Core/Line.h)
list(APPEND coresrc src/Core/TextBuffer.cpp src/Core/TextBuffer.h)
list(APPEND coresrc src/Core/Tokenizer.cpp src/Core/Tokenizer.h)
list(APPEND coresrc src/Core/Language/LangLineTokenizer.cpp src/Core/Language/LangLineTokenizer.h)
list(APPEND coresrc src/Core/Language/LanguageBase.cpp src/Core/Language/LanguageBase.h)
list(APPEND coresrc src/Core/Language/LangToken.cpp src/Core/Language/LangToken.h)
list(APPEND coresrc src/Core/Language/AutoPairEngine.cpp src/Core/Language/AutoPairEngine.h)
list(APPEND coresrc src/Core/Language/AutoPairTable.h)
list(APPEND coresrc src/Core/Language/AutoPairCache.cpp src/Core/Language/AutoPairCache.h)
list(APPEND coresrc src/Core/Language/IndentEngine.cpp src/Core/Language/IndentEngine.h)
list(APPEND coresrc src/Core/Language/IndentTable.h)
list(APPEND coresrc src/Core/Language/IndentCache.cpp src/Core/Language/IndentCache.h)
list(APPEND coresrc src/Core/Language/LanguageSupport/DefaultLanguage.cpp src/Core/Language/LanguageSupport/DefaultLanguage.h)
list(APPEND coresrc src/Core/Language/LanguageSupport/CPPLanguage.cpp src/Core/Language/LanguageSupport/CPPLanguage.h)
list(APPEND coresrc src/Core/Language/LanguageSupport/CPPNumberMatcher.cpp src/Core/Language/LanguageSupport/CPPNumberMatcher.h)
list(APPEND coresrc src/Core/Language/NumberMatcherBase.h)
list(APPEND coresrc src/Core/Language/LanguageSupport/JSONLanguage.cpp src/Core/Language/LanguageSupport/JSONLanguage.h)
list(APPEND coresrc src/Core/Language/LanguageSupport/MakeBuildLang.cpp src/Core/Language/LanguageSupport/MakeBuildLang.h)
list(APPEND coresrc src/Core/Language/LanguageSupport/MarkdownLanguage.cpp src/Core/Language/LanguageSupport/MarkdownLanguage.h)
list(APPEND coresrc src/Core/Sublime/SublimeConfigScriptEngine.cpp src/Core/Sublime/SublimeConfigScriptEngine.h)
list(APPEND coresrc src/Core/Sublime/SublimeConfigColorScript.cpp src/Core/Sublime/SublimeConfigColorScript.h)

# ---------------------------------------------------------------------------
# uisrc — Generic UI toolkit (src/Core/UI/)
# ---------------------------------------------------------------------------

# Graphics rendering contract (backend interface — see docs/ui-refactor.md)
list(APPEND uisrc src/Core/UI/Graphics/NativeWindow.h)
list(APPEND uisrc src/Core/UI/Graphics/ScreenBase.cpp src/Core/UI/Graphics/ScreenBase.h)
list(APPEND uisrc src/Core/UI/Graphics/WindowBase.h)
list(APPEND uisrc src/Core/UI/Graphics/KeyboardDriverBase.cpp src/Core/UI/Graphics/KeyboardDriverBase.h)
list(APPEND uisrc src/Core/UI/Graphics/DrawContext.cpp src/Core/UI/Graphics/DrawContext.h)
list(APPEND uisrc src/Core/Graphics/Cursor.h)
list(APPEND uisrc src/Core/UI/UIHost.cpp src/Core/UI/UIHost.h)
list(APPEND uisrc src/Core/UI/ILayoutSink.h)

# Base views
list(APPEND uisrc src/Core/UI/Views/HSplitView.h)
list(APPEND uisrc src/Core/UI/Views/HStackView.h)
list(APPEND uisrc src/Core/UI/Views/RootView.h)
list(APPEND uisrc src/Core/UI/Views/ViewBase.cpp src/Core/UI/Views/ViewBase.h)
list(APPEND uisrc src/Core/UI/Views/VSplitView.h)
list(APPEND uisrc src/Core/UI/Views/VStackView.h)
list(APPEND uisrc src/Core/UI/Views/VisibleView.cpp src/Core/UI/Views/VisibleView.h)
list(APPEND uisrc src/Core/UI/Views/StackableView.h)
list(APPEND uisrc src/Core/UI/Views/SingleLineView.h)
list(APPEND uisrc src/Core/UI/Views/TestView.cpp src/Core/UI/Views/TestView.h)

# Modals / dialogs
list(APPEND uisrc src/Core/UI/Views/ModalView.cpp src/Core/UI/Views/ModalView.h)
list(APPEND uisrc src/Core/UI/Views/ListSelectionModal.cpp src/Core/UI/Views/ListSelectionModal.h)
list(APPEND uisrc src/Core/UI/Views/TreeView.h)
list(APPEND uisrc src/Core/UI/Views/TreeSelectionModal.cpp src/Core/UI/Views/TreeSelectionModal.h)

# Base controller
list(APPEND uisrc src/Core/UI/Controllers/BaseController.cpp src/Core/UI/Controllers/BaseController.h)

# ---------------------------------------------------------------------------
# editorsrc — Editor-specific UI (src/Core/Editor/)
# ---------------------------------------------------------------------------

# Views
list(APPEND editorsrc src/Core/Editor/Views/EditorView.cpp src/Core/Editor/Views/EditorView.h)
list(APPEND editorsrc src/Core/Editor/Views/HexView.cpp src/Core/Editor/Views/HexView.h)
list(APPEND editorsrc src/Core/Editor/Views/GutterView.cpp src/Core/Editor/Views/GutterView.h)
#list(APPEND editorsrc src/Core/Editor/Views/CommandView.cpp src/Core/Editor/Views/CommandView.h)
list(APPEND editorsrc src/Core/Editor/Views/EditorHeaderView.h)
list(APPEND editorsrc src/Core/Editor/Views/HSplitViewStatus.cpp src/Core/Editor/Views/HSplitViewStatus.h)
list(APPEND editorsrc src/Core/Editor/Views/TerminalView.cpp src/Core/Editor/Views/TerminalView.h)
list(APPEND editorsrc src/Core/Editor/Views/WorkspaceView.cpp src/Core/Editor/Views/WorkspaceView.h)
list(APPEND editorsrc src/Core/Editor/Views/EditorViewContainer.h)

# Rendering + controllers
list(APPEND editorsrc src/Core/Editor/LineRender.cpp src/Core/Editor/LineRender.h)
list(APPEND editorsrc src/Core/Editor/Controllers/EditController.cpp src/Core/Editor/Controllers/EditController.h)
list(APPEND editorsrc src/Core/Editor/Controllers/TerminalController.cpp src/Core/Editor/Controllers/TerminalController.h)
list(APPEND editorsrc src/Core/Editor/Controllers/QuickCommandController.cpp src/Core/Editor/Controllers/QuickCommandController.h)

# ---------------------------------------------------------------------------
# jsapi — JS engine wrapper layer (src/Core/JSEngine/)
# ---------------------------------------------------------------------------
list(APPEND jsapi src/Core/JSEngine/JSPluginEngine.cpp src/Core/JSEngine/JSPluginEngine.h)
list(APPEND jsapi src/Core/JSEngine/JSPluginCommand.cpp src/Core/JSEngine/JSPluginCommand.h)
list(APPEND jsapi src/Core/JSEngine/Modules/ConsoleAPIWrapper.cpp src/Core/JSEngine/Modules/ConsoleAPIWrapper.h)
list(APPEND jsapi src/Core/JSEngine/Modules/DocumentAPIWrapper.cpp src/Core/JSEngine/Modules/DocumentAPIWrapper.h)
list(APPEND jsapi src/Core/JSEngine/Modules/EditorAPIWrapper.cpp src/Core/JSEngine/Modules/EditorAPIWrapper.h)
list(APPEND jsapi src/Core/JSEngine/Modules/NamedColorsAPIWrapper.cpp src/Core/JSEngine/Modules/NamedColorsAPIWrapper.h)
# list(APPEND jsapi src/Core/JSEngine/Modules/TextBufferAPIWrapper.cpp src/Core/JSEngine/Modules/TextBufferAPIWrapper.h)
list(APPEND jsapi src/Core/JSEngine/Modules/ThemeAPIWrapper.cpp src/Core/JSEngine/Modules/ThemeAPIWrapper.h)
list(APPEND jsapi src/Core/JSEngine/Modules/ViewAPIWrapper.cpp src/Core/JSEngine/Modules/ViewAPIWrapper.h)

# ---------------------------------------------------------------------------
# graphics_sdlsrc — SDL rendering backend (SDL2 XOR SDL3; mutually exclusive)
# ---------------------------------------------------------------------------
if (GEDIT_BUILD_SDL3 AND GEDIT_BUILD_SDL2)
    message(WARNING "Both GEDIT_BUILD_SDL3 and GEDIT_BUILD_SDL2 are ON — SDL3 takes priority, SDL2 will be ignored.")
endif()

if (GEDIT_BUILD_SDL3)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL3/SDLScreen.cpp src/Core/Graphics/SDL3/SDLScreen.h)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL3/SDLDrawContext.cpp src/Core/Graphics/SDL3/SDLDrawContext.h)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL3/SDLWindow.cpp src/Core/Graphics/SDL3/SDLWindow.h)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL3/SDLTranslate.cpp src/Core/Graphics/SDL3/SDLTranslate.h)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL3/SDLFontManager.cpp src/Core/Graphics/SDL3/SDLFontManager.h)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL3/SDLKeyboardDriver.cpp src/Core/Graphics/SDL3/SDLKeyboardDriver.h)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL3/SDLColor.h)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL3/SDLCursor.h)

    # Use the keg-specific prefix so find_package resolves SDL3's own include dir
    # (not the global /opt/homebrew/include symlink tree, which loses to any stale
    # SDL3 install under /usr/local/include in the compiler's default search order).
    if (APPLE AND BREW)
        execute_process(COMMAND ${BREW} --prefix sdl3
            OUTPUT_VARIABLE _sdl3_prefix OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if (_sdl3_prefix)
            list(PREPEND CMAKE_PREFIX_PATH "${_sdl3_prefix}")
        endif()
    endif()
    find_package(SDL3 REQUIRED CONFIG)
    message(STATUS "SDL3 found: ${SDL3_VERSION} (${SDL3_DIR})")
    list(APPEND extlibs SDL3::SDL3)
    add_definitions(-DGEDIT_USE_SDL3)

elseif (GEDIT_BUILD_SDL2)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL2/SDLScreen.cpp src/Core/Graphics/SDL2/SDLScreen.h)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL2/SDLDrawContext.cpp src/Core/Graphics/SDL2/SDLDrawContext.h)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL2/SDLWindow.cpp src/Core/Graphics/SDL2/SDLWindow.h)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL2/SDLTranslate.cpp src/Core/Graphics/SDL2/SDLTranslate.h)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL2/SDLFontManager.cpp src/Core/Graphics/SDL2/SDLFontManager.h)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL2/SDLKeyboardDriver.cpp src/Core/Graphics/SDL2/SDLKeyboardDriver.h)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL2/SDLColor.h)
    list(APPEND graphics_sdlsrc src/Core/Graphics/SDL2/SDLCursor.h)

    if (APPLE AND BREW)
        execute_process(COMMAND ${BREW} --prefix sdl2
            OUTPUT_VARIABLE _sdl2_prefix OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
        if (_sdl2_prefix)
            list(PREPEND CMAKE_PREFIX_PATH "${_sdl2_prefix}")
        endif()
    endif()
    find_package(SDL2 REQUIRED CONFIG)
    message(STATUS "SDL2 found: ${SDL2_VERSION} (${SDL2_DIR})")
    list(APPEND extlibs SDL2::SDL2)
    add_definitions(-DGEDIT_USE_SDL2)
endif()


# append configuration stuff - these are copied with custom command


# include and link directories for project and dependencies
# all deps are imported targets (yaml-cpp::yaml-cpp, SDL3::SDL3, fmt, nlohmann_json, gnklog);
# include dirs are carried transitively — only our own src paths need explicit listing here.
list(APPEND includedirs src/ src/ext/stb)
list(APPEND includedirs ${gnklog_SOURCE_DIR}/src)
list(APPEND includedirs ${DUKTAPE_HOME})
list(APPEND includedirs ${DUKTAPE_EXTRAS}/module-node)


target_sources(${CUR_TARGET} PUBLIC ${coresrc} ${uisrc} ${editorsrc} ${platformsrc} ${graphics_sdlsrc} ${jsapi} ${duktape})
target_include_directories(${CUR_TARGET} PUBLIC ${includedirs})
target_link_libraries(${CUR_TARGET} ${extlibs})

#
# unit testing
#
list(APPEND utestsrc utests/test_main.cpp)

list(APPEND utestsrc utests/test_assetloader.cpp)
list(APPEND utestsrc utests/test_clipboard.cpp)
list(APPEND utestsrc utests/test_cpplang.cpp)
list(APPEND utestsrc utests/test_cppnumbers.cpp)
list(APPEND utestsrc utests/test_jsonlang.cpp)
list(APPEND utestsrc utests/test_markdown.cpp)
list(APPEND utestsrc utests/test_config.cpp)
list(APPEND utestsrc utests/test_dcoverlay.cpp)
list(APPEND utestsrc utests/test_editorapi.cpp)
list(APPEND utestsrc utests/test_document.cpp)
list(APPEND utestsrc utests/test_hexprojection.cpp)
list(APPEND utestsrc utests/test_bytestream.cpp)
list(APPEND utestsrc utests/test_hexview.cpp)
list(APPEND utestsrc utests/test_autopair.cpp)
list(APPEND utestsrc utests/test_indent.cpp)
list(APPEND utestsrc utests/test_jsengine.cpp)
list(APPEND utestsrc utests/test_keymapping.cpp)
list(APPEND utestsrc utests/test_layout.cpp)
list(APPEND utestsrc utests/test_linelayout.cpp)
list(APPEND utestsrc utests/test_logger.cpp)
list(APPEND utestsrc utests/test_strutil.cpp)
list(APPEND utestsrc utests/test_theme.cpp)
list(APPEND utestsrc utests/test_timer.cpp)
list(APPEND utestsrc utests/test_textbuffer.cpp)
list(APPEND utestsrc utests/test_unicode.cpp)
list(APPEND utestsrc utests/test_vnav.cpp)
list(APPEND utestsrc utests/test_terminalscreen.cpp)
list(APPEND utestsrc utests/test_terminalcontroller.cpp)
list(APPEND utestsrc utests/test_vtermparser.cpp)
list(APPEND utestsrc utests/test_workspace.cpp)
list(APPEND utestsrc utests/test_folderscanner.cpp)
list(APPEND utestsrc utests/test_treeview.cpp)
list(APPEND utestsrc utests/test_session.cpp)
list(APPEND utestsrc utests/test_xgdenv.cpp)



add_library(utests SHARED ${coresrc} ${uisrc} ${editorsrc} ${platformsrc} ${graphics_sdlsrc} ${jsapi} ${duktape} ${utestsrc})
target_compile_options(utests PUBLIC -fPIC -D TRUN_USE_V1)
target_include_directories(utests PUBLIC ${includedirs} ${CMAKE_BINARY_DIR}/generated)
target_include_directories(utests SYSTEM PUBLIC ${DUKGLUE_HOME}/include)
# CMake strips /usr/local/include from target_include_directories on macOS (treats it as an
# implicit system path). The raw -I flag bypasses that deduplication and is the only reliable
# way to force it through after the macOS SDK upgrade that dropped it from the default path.
if (TRUN_INCLUDE_DIR)
    target_compile_options(utests PUBLIC -I${TRUN_INCLUDE_DIR})
endif()
target_link_libraries(utests ${extlibs})


add_custom_target(resources ${CMAKE_COMMAND} -E copy_directory_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets/Resources ${EDITOR_ASSET_DIR})

add_custom_target(plugins ${CMAKE_COMMAND} -E copy_directory_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Plugins ${EDITOR_ASSET_DIR}/Plugins)

add_custom_target(testfiles ${CMAKE_COMMAND} -E copy_directory_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/Assets/testfiles ${CMAKE_CURRENT_BINARY_DIR}/testfiles)

#add_dependencies(${CUR_TARGET} config colors font plugins default_theme resources)
add_dependencies(${CUR_TARGET} plugins resources)
add_dependencies(utests plugins resources testfiles)


#set_target_properties(logtest PROPERTIES EXCLUDE_FROM_ALL true)
set_target_properties(gnklog_utest PROPERTIES EXCLUDE_FROM_ALL true)
set_target_properties(utests PROPERTIES EXCLUDE_FROM_ALL true)
