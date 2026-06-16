#
# GoatEdit - CMakeLists.txt for Linux and macOS
# Note: I throughly dislike CMake but I find it better than plain makefiles so that's it..
#
cmake_minimum_required(VERSION 3.22)
project(goatedit VERSION 0.1)
set(CMAKE_CXX_STANDARD 20)

# When changing the Assets/CMake/Info.plist.in - which will rebuild the bundle Info.plist we must
# reset the cache in macos for this to take effect...
# /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -kill -seed


set(EDITOR_ASSET_DIR ${CMAKE_CURRENT_BINARY_DIR}/resources)

message(STATUS "Assets directory: ${EDITOR_ASSET_DIR}")
if (NOT EXISTS ${EDITOR_ASSET_DIR})
    execute_process(COMMAND mkdir ${EDITOR_ASSET_DIR})
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

#
# Installing dependencies - source dependencies...
#
if (NOT EXISTS ${CMAKE_SOURCE_DIR}/ext)
    message(STATUS "ext directory (for dependencies) missing, creating...")
    execute_process(COMMAND mkdir ${CMAKE_SOURCE_DIR}/ext)
endif()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/ext/json)
    message(WARNING "Installing JSON library")
    execute_process(COMMAND git clone https://github.com/nlohmann/json ${CMAKE_SOURCE_DIR}/ext/json)
endif ()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/ext/gnklog)
    message(WARNING "Installing GnkLog library")
    execute_process(COMMAND git clone https://github.com/gnilk/gnklog ${CMAKE_SOURCE_DIR}/ext/gnklog)
endif ()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/ext/dukglue)
    message(WARNING "Installing dukglue library")
    execute_process(COMMAND git clone https://github.com/gnilk/dukglue ${CMAKE_SOURCE_DIR}/ext/dukglue)
endif ()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/ext/fmt)
    message(WARNING "Installing fmt library")
    execute_process(COMMAND git clone --depth 1 --branch 12.1.0 https://github.com/fmtlib/fmt ${CMAKE_SOURCE_DIR}/ext/fmt)
endif()
#
# Note: This won't fly as we need to pre-process this further...
#
#if (NOT EXISTS ${CMAKE_SOURCE_DIR}/ext/duktape-2.7.0)
#    message(WARNING "Installing DukTape 2.7.0 library")
#    execute_process(COMMAND git clone --depth 1 --branch v2.7.0 https://github.com/svaarala/duktape ${CMAKE_SOURCE_DIR}/ext/duktape-2.7.0)
#endif ()


#
# Dependencies
#

set(DUKTAPE_HOME src/ext/duktape-2.7.0/src)
set(DUKTAPE_EXTRAS src/ext/duktape-2.7.0/extras)
set(DUKGLUE_HOME ext/dukglue)
set(FMT_HOME ext/fmt)

#
# Experimenting - not sure this does any good...
#
file(GLOB_RECURSE DUKGLUE_FILES "${DUKGLUE_HOME}/include/*.h")
set_source_files_properties(
        ${DUKGLUE_FILES}
        PROPERTIES
        COMPILE_FLAGS "-Wno-shadow -Wno-unused-but-set-parameter -Wno-sign-compare -Wno-unused-local-typedefs"
)


list(APPEND duktape ${DUKTAPE_HOME}/duktape.c ${DUKTAPE_HOME}/duk_config.h ${DUKTAPE_HOME}/duktape.h)
list(APPEND duktape ${DUKTAPE_EXTRAS}/module-node/duk_module_node.c)


set(LOG_HAVE_FMT ON)
set(LOG_FMT_DIR ${CMAKE_SOURCE_DIR}/ext/fmt)

add_subdirectory(ext/gnklog ${CMAKE_BINARY_DIR}/gnklog)
target_compile_options(gnklog PUBLIC -fPIC -Wno-unused-parameter)

list(APPEND extlibs gnklog)


add_subdirectory(ext/fmt ${CMAKE_BINARY_DIR}/fmt EXCLUDE_FROM_ALL)
set_property(TARGET fmt PROPERTY POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(fmt PUBLIC FMT_EXCEPTIONS=0)
list(APPEND extlibs fmt)

set(CUR_TARGET goatedit)

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
    set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)

    add_definitions(-DGEDIT_MACOS)

    ## THIS IS FOR macOS - homebrew installation...
    set(NCURSES_HOME /opt/homebrew/opt/ncurses)
    set(YAML_CPP_HOME /opt/homebrew/opt/yaml-cpp)

    list(APPEND editorsrc src/Core/macOS/MacOSKBEmulator.cpp src/Core/macOS/MacOSKBEmulator.h)
    list(APPEND editorsrc src/Core/macOS/MacOSFolderMonitor.cpp src/Core/macOS/MacOSFolderMonitor.h)
    # list(APPEND editorsrc src/Core/macOS/MacOSKeyboardMonitor.cpp src/Core/macOS/MacOSKeyboardMonitor.h)
    list(APPEND editorsrc src/Core/unix/Shell.cpp src/Core/unix/Shell.h)

    find_library(CARBON_FRAMEWORK Carbon)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(GAMECONTROLLER_FRAMEWORK GameController)
    find_library(CORE_FOUNDATION CoreFoundation)
    find_library(CORE_SERVICES CoreServices)
    find_library(CORE_GRAPHICS CoreGraphics)
    find_library(APPKIT AppKit)


    list(APPEND extlibs ncurses)
    list(APPEND extlibs yaml-cpp)
    list(APPEND extlibs ${CORE_SERVICES})


    set(ICON_FILE ${CMAKE_SOURCE_DIR}/Assets/CMake/GoatEdit.icns)
    set(PLIST_FILE_IN ${CMAKE_SOURCE_DIR}/Assets/CMake/Info.plist.in)

    # for MacOS add - mark as 'MACOSX_BUNDLE' and add the ICON as well
    add_executable(${CUR_TARGET} MACOSX_BUNDLE ${appsrc} ${ICON_FILE})
    # Now, set some properties to tell CMake where it should go..
    set_source_files_properties(${ICON_FILE} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")

    list(APPEND includedirs ext/json/single_include)


    # When changing the Assets/CMake/Info.plist.in - which will rebuild the bundle Info.plist we must
    # reset the cache in macos for this to take effect...
    # /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister -kill -seed

    set_target_properties(${CUR_TARGET} PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER gnilk.com.${CUR_TARGET}
            MACOSX_BUNDLE_BUNDLE_NAME ${CUR_TARGET}
            MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
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


    #    install(TARGETS ${CUR_TARGET} BUNDLE DESTINATION GoatEdit.app/Contents/MacOS COMPONENT GoatEdit)
    #    install(DIRECTORY ${EDITOR_ASSET_DIR} DESTINATION GoatEdit.app/Contents/Resources COMPONENT GoatEdit)


    target_compile_options(${CUR_TARGET} PUBLIC -Wall -Wextra -Wpedantic -Wshadow)
    target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-parameter)
    target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-variable)

    # DukGlue has quite a few type-defs causing pain - consider putting duglue into it's own compile target..
    target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-local-typedef)
    target_compile_options(${CUR_TARGET} PUBLIC -Wno-unneeded-internal-declaration)
    # this one is irritating..
    target_compile_options(${CUR_TARGET} PUBLIC -Wno-sign-compare)
    target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-but-set-variable)

    # YAML header library
    target_compile_options(${CUR_TARGET} PUBLIC -Wno-shadow)


#
#    SET(CPACK_GENERATOR "DragNDrop")
##    SET(CPACK_PACKAGE_EXECUTABLES ${CMAKE_CURRENT_BINARY_DIR}/editor.app)
#    SET(CPACK_BUNDLE_NAME "GoatEdit")
#    SET(CPACK_BUNDLE_STARTUP_COMMAND "editor")
#    SET(CPACK_BUNDLE_ICON ${CMAKE_SOURCE_DIR}/Assets/CMake/GoatEdit.icns)
#    SET(CPACK_BUNDLE_PLIST ${CMAKE_SOURCE_DIR}/Assets/CMake/GoatEdit.plist)
#    SET(CPACK_PACKAGE_DESCRIPTION "A text editor for my own personal use..")
#    SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "not a very fancy text/code editor..")
#    SET(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/gnilk/editor")
#    #configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Assets/CMake/Info.plist.in ${CMAKE_CURRENT_BINARY_DIR}/GoatEdit2.plist @ONLY)
#    #SET(CPACK_BUNDLE_PLIST ${CMAKE_CURRENT_BINARY_DIR}/GoatEdit2.plist)
##    SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "gnilk") #required
##    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
#    INCLUDE(CPack)




    # list(APPEND extlibs ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${GAMECONTROLLER_FRAMEWORK} ${CORE_FOUNDATION})
    list(APPEND extlibs ${IOKIT_FRAMEWORK} ${CORE_FOUNDATION})
elseif(UNIX)
    message(STATUS "Linux build")
    set(YAML_CPP_HOME /usr)
    set(NCURSES_HOME /usr)
    set(SDL_HOME /usr/local)


    list(APPEND includedirs ext/json/single_include)
    list(APPEND editorsrc src/Core/unix/Shell.cpp src/Core/unix/Shell.h)
    list(APPEND editorsrc src/Core/Linux/LinuxFolderMonitor.cpp src/Core/Linux/LinuxFolderMonitor.h)


    add_definitions(-DGEDIT_LINUX)

    list(APPEND extlibs util)   ## see: man forkpty
    list(APPEND extlibs ncurses)
    list(APPEND extlibs yaml-cpp)
    add_executable(${CUR_TARGET} ${appsrc})

    add_definitions(-DGEDIT_LINUX)

    target_compile_options(${CUR_TARGET} PUBLIC -fPIC)
    #target_compile_options(${CUR_TARGET} PUBLIC -Wall -Wextra -Wpedantic -Wshadow)
    target_compile_options(${CUR_TARGET} PUBLIC -Wall -Wextra -Wshadow)

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message("Compiler is Clang")
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-parameter)
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-variable)

        # DukGlue has quite a few type-defs causing pain - consider putting duglue into it's own compile target..
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-local-typedef)
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-unneeded-internal-declaration)
        # this one is irritating..
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-sign-compare)
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-but-set-variable)

        # YAML header library
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-shadow)
    else()
        # Assume...
        message("Compiler is GCC")
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-parameter)
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-variable)

        # DukGlue has quite a few type-defs causing pain - consider putting duglue into it's own compile target..
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-local-typedefs)
        # target_compile_options(${CUR_TARGET} PUBLIC -Wno-unneeded-internal-declaration)
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-but-set-parameter)
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-value)

        target_compile_options(${CUR_TARGET} PUBLIC -Wno-implicit-fallthrough)

        # only for C sources..
        # fix this...
        # target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-local-typedefs)

        # this one is irritating..
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-sign-compare)
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-unused-but-set-variable)

        # YAML header library
        target_compile_options(${CUR_TARGET} PUBLIC -Wno-shadow)

    endif()


    #
    # Create a .deb install package..
    # Note: The binary will currently be named 'editor' <- should fix this at some point...
    #
    include(GNUInstallDirs)
    install(TARGETS ${CUR_TARGET} RUNTIME DESTINATION bin)
    install(DIRECTORY ${EDITOR_ASSET_DIR}/. DESTINATION ${CMAKE_INSTALL_DATADIR}/goatedit)
    install(FILES ${CMAKE_SOURCE_DIR}/Assets/CMake/com.gnilk.goatedit.desktop DESTINATION DESTINATION share/applications)
    install(FILES ${CMAKE_SOURCE_DIR}/Assets/Graphics/com.gnilk.goatedit.png DESTINATION DESTINATION share/icons/hicolor/scalable/apps)

    SET(CPACK_GENERATOR "DEB")
    SET(CPACK_PACKAGE_EXECUTEABLES goatedit;GoatEdit)
    SET(CPACK_PACKAGE_ICON ${CMAKE_SOURCE_DIR}/Assets/Graphics/goatedit_icon.png)
    SET(CPACK_PACKAGE_NAME "goatedit")
    SET(CPACK_PACKAGE_VENDOR "gnilk")
    SET(CPACK_PACKAGE_DESCRIPTION "A text editor for my own personal use..")
    SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "not a very fancy text/code editor..")
    SET(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/gnilk/editor")
    SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "gnilk") #required
    SET(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
    INCLUDE(CPack)

endif ()

#
#
#
target_compile_definitions(${CUR_TARGET} PUBLIC GEDIT_APP_NAME=GoatEdit)
target_compile_definitions(${CUR_TARGET} PUBLIC GEDIT_VERSION_MAJOR=0)
target_compile_definitions(${CUR_TARGET} PUBLIC GEDIT_VERSION_MINOR=1)
target_compile_definitions(${CUR_TARGET} PUBLIC GEDIT_VERSION_PATCH=0)


#
# Editor
#

# Shared third-party single-header implementations (one TU for the whole binary)
list(APPEND editorsrc src/ext/stb/stb_impl.cpp)

# Editor main stuff
list(APPEND editorsrc src/Core/AssetLoaderBase.cpp src/Core/AssetLoaderBase.h)
list(APPEND editorsrc src/Core/ClipBoard.cpp src/Core/ClipBoard.h)
list(APPEND editorsrc src/Core/Editor.cpp src/Core/Editor.h)
list(APPEND editorsrc src/Core/Document.cpp src/Core/Document.h)
list(APPEND editorsrc src/Core/DocumentViewMode.h)
list(APPEND editorsrc src/Core/DocumentViewState.cpp src/Core/DocumentViewState.h)
list(APPEND editorsrc src/Core/BinBuffer.h)
list(APPEND editorsrc src/Core/ByteStreamReader.cpp src/Core/ByteStreamReader.h)
list(APPEND editorsrc src/Core/HexProjection.cpp src/Core/HexProjection.h)
list(APPEND editorsrc src/Core/FolderMonitor.cpp src/Core/FolderMonitor.h)
list(APPEND editorsrc src/Core/Runloop.cpp src/Core/Runloop.h)
list(APPEND editorsrc src/Core/VerticalNavigationViewModel.cpp src/Core/VerticalNavigationViewModel.h)
list(APPEND editorsrc src/Core/Workspace.cpp src/Core/Workspace.h)

# API and Actions
list(APPEND editorsrc src/Core/Action.cpp src/Core/Action.h)
list(APPEND editorsrc src/Core/ActionHelper.cpp src/Core/ActionHelper.h)
list(APPEND editorsrc src/Core/KeyMapping.cpp src/Core/KeyMapping.h)
list(APPEND editorsrc src/Core/KeyMappingCache.cpp src/Core/KeyMappingCache.h)
list(APPEND editorsrc src/Core/API/DocumentAPI.cpp src/Core/API/DocumentAPI.h)
list(APPEND editorsrc src/Core/API/EditorAPI.cpp src/Core/API/EditorAPI.h)
list(APPEND editorsrc src/Core/API/NamedColorsAPI.cpp src/Core/API/NamedColorsAPI.h)
list(APPEND editorsrc src/Core/API/ThemeAPI.cpp src/Core/API/ThemeAPI.cpp)
# list(APPEND editorsrc src/Core/API/TextBufferAPI.cpp src/Core/API/TextBufferAPI.h)
list(APPEND editorsrc src/Core/API/ViewAPI.cpp src/Core/API/ViewAPI.h)
list(APPEND editorsrc src/Core/Plugins/PluginCommand.cpp src/Core/Plugins/PluginCommand.h)
list(APPEND editorsrc src/Core/Plugins/PluginExecutor.cpp src/Core/Plugins/PluginExecutor.h)
list(APPEND editorsrc src/Core/KeypressAndActionHandler.h)

# utility stuff
list(APPEND editorsrc src/Core/backward.cpp src/Core/backward.hpp)
list(APPEND editorsrc src/Core/ConvertUTF.cpp src/Core/ConvertUTF.h)
list(APPEND editorsrc src/Core/ColorRGBA.cpp src/Core/ColorRGBA.h)
list(APPEND editorsrc src/Core/DurationTimer.cpp src/Core/DurationTimer.h)
list(APPEND editorsrc src/Core/HexDump.cpp src/Core/HexDump.h)
list(APPEND editorsrc src/Core/Glob.cpp src/Core/Glob.h)
list(APPEND editorsrc src/Core/Job.h)
list(APPEND editorsrc src/Core/NamedColors.cpp src/Core/NamedColors.h)
list(APPEND editorsrc src/Core/PathUtil.cpp src/Core/PathUtil.h)
list(APPEND editorsrc src/Core/StrUtil.cpp src/Core/StrUtil.h)
list(APPEND editorsrc src/Core/Timer.cpp src/Core/Timer.h)
list(APPEND editorsrc src/Core/TypeUtil.h)
list(APPEND editorsrc src/Core/VTermParser.cpp src/Core/VTermParser.h)
list(APPEND editorsrc src/Core/TerminalScreen.cpp src/Core/TerminalScreen.h)
list(APPEND editorsrc src/Core/TerminalHistory.cpp src/Core/TerminalHistory.h)
list(APPEND editorsrc src/Core/UnicodeHelper.cpp src/Core/UnicodeHelper.h)

# Keyboard handling
list(APPEND editorsrc src/Core/KeyPress.cpp src/Core/KeyPress.h)
list(APPEND editorsrc src/Core/UI/Graphics/KeyboardDriverBase.cpp src/Core/UI/Graphics/KeyboardDriverBase.h)
list(APPEND editorsrc src/Core/Keyboard.cpp src/Core/Keyboard.cpp)

# Config
list(APPEND editorsrc src/Core/Config/Config.cpp src/Core/Config/Config.h)
list(APPEND editorsrc src/Core/Config/ConfigNode.cpp src/Core/Config/ConfigNode.h)
list(APPEND editorsrc src/Core/Config/YAMLMerge.cpp src/Core/Config/YAMLMerge.h)
list(APPEND editorsrc src/Core/Theme/Theme.cpp src/Core/Theme/Theme.h)
list(APPEND editorsrc src/Core/RuntimeConfig.cpp src/Core/RuntimeConfig.cpp)
list(APPEND editorsrc src/Core/XDGEnvironment.cpp src/Core/XDGEnvironment.h)

# Session cache (per-root persistence — docs/session-cache.md)
list(APPEND editorsrc src/Core/Session/SessionManager.cpp src/Core/Session/SessionManager.h)
list(APPEND editorsrc src/Core/Session/SessionState.h)
list(APPEND editorsrc src/Core/Session/SessionSerializer.cpp src/Core/Session/SessionSerializer.h)


# Graphics base classes (UI rendering CONTRACT — generic toolkit, see docs/ui-refactor.md AI-1)
list(APPEND editorsrc src/Core/UI/Graphics/NativeWindow.h)
list(APPEND editorsrc src/Core/UI/Graphics/ScreenBase.cpp src/Core/UI/Graphics/ScreenBase.h)
list(APPEND editorsrc src/Core/UI/Graphics/WindowBase.h)
list(APPEND editorsrc src/Core/Graphics/Cursor.h)
# NCurses
#list(APPEND editorsrc src/Core/Graphics/NCurses/NCursesDrawContext.cpp src/Core/Graphics/NCurses/NCursesDrawContext.h)
#list(APPEND editorsrc src/Core/Graphics/NCurses/NCursesKeyboardDriver.cpp src/Core/Graphics/NCurses/NCursesKeyboardDriver.h)
#list(APPEND editorsrc src/Core/Graphics/NCurses/NCursesScreen.cpp src/Core/Graphics/NCurses/NCursesScreen.h)
#list(APPEND editorsrc src/Core/Graphics/NCurses/NCursesWindow.cpp src/Core/Graphics/NCurses/NCursesWindow.h)
#list(APPEND editorsrc src/Core/Graphics/NCurses/NCursesColorRepository.cpp src/Core/Graphics/NCurses/NCursesColorRepository.h)
#list(APPEND editorsrc src/Core/Graphics/NCurses/NCursesTranslate.cpp src/Core/Graphics/NCurses/NCursesTranslate.h)
list(APPEND editorsrc src/Core/UI/UIHost.cpp src/Core/UI/UIHost.h)
list(APPEND editorsrc src/Core/UI/ILayoutSink.h)
# base views (generic UI toolkit)
list(APPEND editorsrc src/Core/UI/Views/HSplitView.h)
list(APPEND editorsrc src/Core/UI/Views/HStackView.h)
list(APPEND editorsrc src/Core/UI/Views/RootView.h)
list(APPEND editorsrc src/Core/UI/Views/ViewBase.cpp src/Core/UI/Views/ViewBase.h)
list(APPEND editorsrc src/Core/UI/Views/VSplitView.h)
list(APPEND editorsrc src/Core/UI/Views/VStackView.h)
list(APPEND editorsrc src/Core/UI/Views/VisibleView.cpp src/Core/UI/Views/VisibleView.h)
list(APPEND editorsrc src/Core/UI/Views/StackableView.h)
list(APPEND editorsrc src/Core/UI/Views/TestView.cpp src/Core/UI/Views/TestView.h)
# specific views (editor-specific UI)
list(APPEND editorsrc src/Core/Editor/Views/EditorView.cpp src/Core/Editor/Views/EditorView.h)
list(APPEND editorsrc src/Core/Editor/Views/HexView.cpp src/Core/Editor/Views/HexView.h)
list(APPEND editorsrc src/Core/Editor/Views/GutterView.cpp src/Core/Editor/Views/GutterView.h)
#list(APPEND editorsrc src/Core/Editor/Views/CommandView.cpp src/Core/Editor/Views/CommandView.h)
list(APPEND editorsrc src/Core/UI/Views/SingleLineView.h)
list(APPEND editorsrc src/Core/Editor/Views/EditorHeaderView.h)
list(APPEND editorsrc src/Core/Editor/Views/HSplitViewStatus.cpp src/Core/Editor/Views/HSplitViewStatus.h)
list(APPEND editorsrc src/Core/Editor/Views/TerminalView.cpp src/Core/Editor/Views/TerminalView.h)
list(APPEND editorsrc src/Core/Editor/Views/WorkspaceView.cpp src/Core/Editor/Views/WorkspaceView.h)
list(APPEND editorsrc src/Core/Editor/Views/EditorViewContainer.h)

# modals/dialogs/etc.. (generic UI toolkit)
list(APPEND editorsrc src/Core/UI/Views/ModalView.cpp src/Core/UI/Views/ModalView.h)
list(APPEND editorsrc src/Core/UI/Views/ListSelectionModal.cpp src/Core/UI/Views/ListSelectionModal.h)
list(APPEND editorsrc src/Core/UI/Views/TreeView.h)
list(APPEND editorsrc src/Core/UI/Views/TreeSelectionModal.cpp src/Core/UI/Views/TreeSelectionModal.h)

# draw helper
list(APPEND editorsrc src/Core/UI/Graphics/DrawContext.cpp src/Core/UI/Graphics/DrawContext.h)
list(APPEND editorsrc src/Core/Point.h)
list(APPEND editorsrc src/Core/Rect.h)
list(APPEND editorsrc src/Core/Editor/LineRender.cpp src/Core/Editor/LineRender.h)
# controllers
list(APPEND editorsrc src/Core/UI/Controllers/BaseController.cpp src/Core/UI/Controllers/BaseController.h)
list(APPEND editorsrc src/Core/Editor/Controllers/EditController.cpp src/Core/Editor/Controllers/EditController.h)
list(APPEND editorsrc src/Core/Editor/Controllers/TerminalController.cpp src/Core/Editor/Controllers/TerminalController.h)
list(APPEND editorsrc src/Core/Editor/Controllers/QuickCommandController.cpp src/Core/Editor/Controllers/QuickCommandController.h)
list(APPEND editorsrc src/Core/UndoHistory.cpp src/Core/UndoHistory.h)

# text buffer handling
list(APPEND editorsrc src/Core/Line.cpp src/Core/Line.h)
list(APPEND editorsrc src/Core/TextBuffer.cpp src/Core/TextBuffer.h)
# Language analysis
list(APPEND editorsrc src/Core/Tokenizer.cpp src/Core/Tokenizer.h)
list(APPEND editorsrc src/Core/Language/LangLineTokenizer.cpp src/Core/Language/LangLineTokenizer.h)
list(APPEND editorsrc src/Core/Language/LanguageBase.cpp src/Core/Language/LanguageBase.h)
list(APPEND editorsrc src/Core/Language/LangToken.cpp src/Core/Language/LangToken.h)
list(APPEND editorsrc src/Core/Language/AutoPairEngine.cpp src/Core/Language/AutoPairEngine.h)
list(APPEND editorsrc src/Core/Language/AutoPairTable.h)
list(APPEND editorsrc src/Core/Language/AutoPairCache.cpp src/Core/Language/AutoPairCache.h)
list(APPEND editorsrc src/Core/Language/IndentEngine.cpp src/Core/Language/IndentEngine.h)
list(APPEND editorsrc src/Core/Language/IndentTable.h)
list(APPEND editorsrc src/Core/Language/IndentCache.cpp src/Core/Language/IndentCache.h)
list(APPEND editorsrc src/Core/Language/LanguageSupport/DefaultLanguage.cpp src/Core/Language/LanguageSupport/DefaultLanguage.h)
list(APPEND editorsrc src/Core/Language/LanguageSupport/CPPLanguage.cpp src/Core/Language/LanguageSupport/CPPLanguage.h)
list(APPEND editorsrc src/Core/Language/LanguageSupport/CPPNumberMatcher.cpp src/Core/Language/LanguageSupport/CPPNumberMatcher.h)
list(APPEND editorsrc src/Core/Language/NumberMatcherBase.h)
list(APPEND editorsrc src/Core/Language/LanguageSupport/JSONLanguage.cpp src/Core/Language/LanguageSupport/JSONLanguage.cpp)
list(APPEND editorsrc src/Core/Language/LanguageSupport/MakeBuildLang.cpp src/Core/Language/LanguageSupport/MakeBuildLang.h)
list(APPEND editorsrc src/Core/Language/LanguageSupport/MarkdownLanguage.cpp src/Core/Language/LanguageSupport/MarkdownLanguage.h)
# sublime config/scripting support
list(APPEND editorsrc src/Core/Sublime/SublimeConfigScriptEngine.cpp src/Core/Sublime/SublimeConfigScriptEngine.h)
list(APPEND editorsrc src/Core/Sublime/SublimeConfigColorScript.cpp src/Core/Sublime/SublimeConfigColorScript.h)


#
# JSAPI - separate
#
list(APPEND jsapi src/Core/JSEngine/JSPluginEngine.cpp src/Core/JSEngine/JSPluginEngine.h)
list(APPEND jsapi src/Core/JSEngine/JSPluginCommand.cpp src/Core/JSEngine/JSPluginCommand.h)
list(APPEND jsapi src/Core/JSEngine/Modules/ConsoleAPIWrapper.cpp src/Core/JSEngine/Modules/ConsoleAPIWrapper.h)
list(APPEND jsapi src/Core/JSEngine/Modules/DocumentAPIWrapper.cpp src/Core/JSEngine/Modules/DocumentAPIWrapper.h)
list(APPEND jsapi src/Core/JSEngine/Modules/EditorAPIWrapper.cpp src/Core/JSEngine/Modules/EditorAPIWrapper.h)
list(APPEND jsapi src/Core/JSEngine/Modules/NamedColorsAPIWrapper.cpp src/Core/JSEngine/Modules/NamedColorsAPIWrapper.h)
# list(APPEND jsapi src/Core/JSEngine/Modules/TextBufferAPIWrapper.cpp src/Core/JSEngine/Modules/TextBufferAPIWrapper.h)
list(APPEND jsapi src/Core/JSEngine/Modules/ThemeAPIWrapper.cpp src/Core/JSEngine/Modules/ThemeAPIWrapper.h)
list(APPEND jsapi src/Core/JSEngine/Modules/ViewAPIWrapper.cpp src/Core/JSEngine/Modules/ViewAPIWrapper.h)



# SDL backends are mutually exclusive — only one can be linked at a time.
if (GEDIT_BUILD_SDL3 AND GEDIT_BUILD_SDL2)
    message(WARNING "Both GEDIT_BUILD_SDL3 and GEDIT_BUILD_SDL2 are ON — SDL3 takes priority, SDL2 will be ignored.")
endif()

if (GEDIT_BUILD_SDL3)
    list(APPEND editorsrc src/Core/Graphics/SDL3/SDLScreen.cpp src/Core/Graphics/SDL3/SDLScreen.h)
    list(APPEND editorsrc src/Core/Graphics/SDL3/SDLDrawContext.cpp src/Core/Graphics/SDL3/SDLDrawContext.h)
    list(APPEND editorsrc src/Core/Graphics/SDL3/SDLWindow.cpp src/Core/Graphics/SDL3/SDLWindow.h)
    list(APPEND editorsrc src/Core/Graphics/SDL3/SDLTranslate.cpp src/Core/Graphics/SDL3/SDLTranslate.h)
    list(APPEND editorsrc src/Core/Graphics/SDL3/SDLFontManager.cpp src/Core/Graphics/SDL3/SDLFontManager.h)
    list(APPEND editorsrc src/Core/Graphics/SDL3/SDLKeyboardDriver.cpp src/Core/Graphics/SDL3/SDLKeyboardDriver.h)
    list(APPEND editorsrc src/Core/Graphics/SDL3/SDLColor.h)
    list(APPEND editorsrc src/Core/Graphics/SDL3/SDLCursor.h)

    # Locate SDL3 via its CMake config package so we pick up the right installation.
    # - macOS/Homebrew: SDL3 is keg-only, not in the default search path
    # - Linux: manual builds typically install to /usr/local
    if (APPLE)
        list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/sdl3)
    elseif (UNIX)
        list(APPEND CMAKE_PREFIX_PATH /usr/local)
    endif()
    find_package(SDL3 REQUIRED CONFIG)
    message(STATUS "SDL3 found: ${SDL3_VERSION} (${SDL3_DIR})")
    # Force the correct SDL3 headers to the front of the search path so they
    # shadow any stale manual install that may live in /usr/local/include.
    # include_directories(BEFORE SYSTEM SDL3::include)
    include_directories(SDL3::include)
    list(APPEND extlibs SDL3::SDL3)
    add_definitions(-DGEDIT_USE_SDL3)

elseif (GEDIT_BUILD_SDL2)
    list(APPEND editorsrc src/Core/Graphics/SDL2/SDLScreen.cpp src/Core/Graphics/SDL2/SDLScreen.h)
    list(APPEND editorsrc src/Core/Graphics/SDL2/SDLDrawContext.cpp src/Core/Graphics/SDL2/SDLDrawContext.h)
    list(APPEND editorsrc src/Core/Graphics/SDL2/SDLWindow.cpp src/Core/Graphics/SDL2/SDLWindow.h)
    list(APPEND editorsrc src/Core/Graphics/SDL2/SDLTranslate.cpp src/Core/Graphics/SDL2/SDLTranslate.h)
    list(APPEND editorsrc src/Core/Graphics/SDL2/SDLFontManager.cpp src/Core/Graphics/SDL2/SDLFontManager.h)
    list(APPEND editorsrc src/Core/Graphics/SDL2/SDLKeyboardDriver.cpp src/Core/Graphics/SDL2/SDLKeyboardDriver.h)
    list(APPEND editorsrc src/Core/Graphics/SDL2/SDLColor.h)
    list(APPEND editorsrc src/Core/Graphics/SDL2/SDLCursor.h)
    # make this optional
    #list(APPEND extlibs SDL2)
    if (APPLE)
        list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/sdl2)
    elseif (UNIX)
        list(APPEND CMAKE_PREFIX_PATH /usr/local)
    endif()
    find_package(SDL2 REQUIRED CONFIG)
    message(STATUS "SDL2 found: ${SDL2_VERSION} (${SDL2_DIR})")
    message(STATUS "SDL2 include: (${SDL2_INCLUDE_DIRS})")
    # Force the correct SDL3 headers to the front of the search path so they
    # shadow any stale manual install that may live in /usr/local/include.
    # include_directories(BEFORE SYSTEM SDL3::include)
    #include_directories(SDL2::include)
    include_directories(${SDL2_INCLUDE_DIRS})
    list(APPEND extlibs SDL2::SDL2)

    add_definitions(-DGEDIT_USE_SDL2)
endif()


# append configuration stuff - these are copied with custom command


# include and link directories for project and dependencies
list(APPEND linkdirs ${NCURSES_HOME}/lib ${YAML_CPP_HOME}/lib)
#list(APPEND linkdirs ${SDL_HOME}/lib)

list(APPEND includedirs ${NCURSES_HOME}/include PUBLIC ${YAML_CPP_HOME}/include ext/gnklog/src src/ src/ext/stb)
#list(APPEND includedirs ${SDL_HOME}/include)

list(APPEND includedirs ${DUKGLUE_HOME}/include)
list(APPEND includedirs ${DUKTAPE_HOME})
list(APPEND includedirs ${DUKTAPE_EXTRAS}/module-node)
list(APPEND includedirs ${FMT_HOME}/include)


target_sources(${CUR_TARGET} PUBLIC ${editorsrc} ${jsapi} ${duktape})
target_link_directories(${CUR_TARGET} PUBLIC ${linkdirs})
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
list(APPEND utestsrc utests/test_vtermparser.cpp)
list(APPEND utestsrc utests/test_workspace.cpp)
list(APPEND utestsrc utests/test_session.cpp)
list(APPEND utestsrc utests/test_xgdenv.cpp)



add_library(utests SHARED ${editorsrc} ${jsapi}  ${duktape} ${utestsrc})
#target_compile_options(logger PUBLIC -fPIC)
target_compile_options(utests PUBLIC -fPIC -D TRUN_USE_V1)
target_link_directories(utests PUBLIC ${linkdirs})
target_include_directories(utests PUBLIC ${includedirs})
target_compile_options(utests PUBLIC -I/usr/local/include)
target_link_libraries(utests ${extlibs})


#add_custom_target(
#        Unit_Tests ALL
#        DEPENDS utests
#        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
#)

##
## Add configuration files so they get copied when changed...
##
#
#add_custom_target(config ${CMAKE_COMMAND} -E copy_if_different
#        ${CMAKE_CURRENT_LIST_DIR}/config.yml ${EDITOR_ASSET_DIR})
#
#add_custom_target(colors ${CMAKE_COMMAND} -E copy_if_different
#        ${CMAKE_CURRENT_LIST_DIR}/colors.json ${EDITOR_ASSET_DIR})
#
#add_custom_target(default_theme ${CMAKE_COMMAND} -E copy_if_different
#        ${CMAKE_CURRENT_LIST_DIR}/default.theme.yml ${EDITOR_ASSET_DIR})
#add_custom_target(font ${CMAKE_COMMAND} -E copy_if_different
#        ${CMAKE_CURRENT_LIST_DIR}/Andale\ Mono.ttf ${EDITOR_ASSET_DIR})

# copy_directory_if_different available in cmake 3.26 or better...
if (${CMAKE_VERSION} VERSION_LESS "3.26.0")
    add_custom_target(resources ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/Assets/Resources ${EDITOR_ASSET_DIR})

    add_custom_target(plugins ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/src/Plugins ${EDITOR_ASSET_DIR}/Plugins)

    add_custom_target(testfiles ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/Assets/testfiles ${CMAKE_CURRENT_BINARY_DIR}/testfiles)
else()
    add_custom_target(resources ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/Assets/Resources ${EDITOR_ASSET_DIR})

    add_custom_target(plugins ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/src/Plugins ${EDITOR_ASSET_DIR}/Plugins)

    add_custom_target(testfiles ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/Assets/testfiles ${CMAKE_CURRENT_BINARY_DIR}/testfiles)
endif()

#add_dependencies(${CUR_TARGET} config colors font plugins default_theme resources)
add_dependencies(${CUR_TARGET} plugins resources)
add_dependencies(utests plugins resources testfiles)


#set_target_properties(logtest PROPERTIES EXCLUDE_FROM_ALL true)
set_target_properties(gnklog_utest PROPERTIES EXCLUDE_FROM_ALL true)
set_target_properties(utests PROPERTIES EXCLUDE_FROM_ALL true)
