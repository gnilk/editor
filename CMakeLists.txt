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

option(GEDIT_BUILD_SDL3 "Build SDL3" OFF)
option(GEDIT_BUILD_SDL2 "Build SDL2" ON)

set(SDL_HOME /usr/local)


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

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/ext/logger)
    message(WARNING "Installing Logger library")
    execute_process(COMMAND git clone https://github.com/gnilk/logger ${CMAKE_SOURCE_DIR}/ext/logger)
endif ()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/ext/gnklog)
    message(WARNING "Installing GnkLog library")
    execute_process(COMMAND git clone https://github.com/gnilk/gnklog ${CMAKE_SOURCE_DIR}/ext/logger)
endif ()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/ext/dukglue)
    message(WARNING "Installing dukglue library")
    execute_process(COMMAND git clone https://github.com/gnilk/dukglue ${CMAKE_SOURCE_DIR}/ext/dukglue)
endif ()

if (NOT EXISTS ${CMAKE_SOURCE_DIR}/ext/fmt)
    message(WARNING "Installing fmt library")
    execute_process(COMMAND git clone --depth 1 --branch 10.1.1 https://github.com/fmtlib/fmt ${CMAKE_SOURCE_DIR}/ext/fmt)
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

#add_subdirectory(ext/logger ${CMAKE_BINARY_DIR}/logger)
#list(APPEND extlibs logger)

add_subdirectory(ext/gnklog ${CMAKE_BINARY_DIR}/gnklog)
target_compile_options(gnklog PUBLIC -fPIC -Wno-unused-parameter)

list(APPEND extlibs gnklog)

add_subdirectory(ext/fmt ${CMAKE_BINARY_DIR}/fmt EXCLUDE_FROM_ALL)
set_property(TARGET fmt PROPERTY POSITION_INDEPENDENT_CODE ON)
list(APPEND extlibs fmt)



set(CUR_TARGET goatedit)

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
    set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)

    # Not sure if this is the right way to change the macOS defaults...
    set(GEDIT_BUILD_SDL3 OFF)
    set(GEDIT_BUILD_SDL2 ON)

    add_definitions(-DGEDIT_MACOS)

    include_directories(AFTER SYSTEM /usr/local/include)

    ## THIS IS FOR macOS - homebrew installation...
    set(NCURSES_HOME /opt/homebrew/opt/ncurses)
    set(YAML_CPP_HOME /opt/homebrew/opt/yaml-cpp)


    list(APPEND editorsrc src/Core/macOS/MacOSFolderMonitor.cpp src/Core/macOS/MacOSFolderMonitor.h)
    list(APPEND editorsrc src/Core/macOS/MacOSKeyboardMonitor.cpp src/Core/macOS/MacOSKeyboardMonitor.h)
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
            DEPENDS plugins
            DEPENDS resources
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

# Editor main stuff
list(APPEND editorsrc src/Core/AssetLoaderBase.cpp src/Core/AssetLoaderBase.h)
list(APPEND editorsrc src/Core/ClipBoard.cpp src/Core/ClipBoard.h)
list(APPEND editorsrc src/Core/Editor.cpp src/Core/Editor.h)
list(APPEND editorsrc src/Core/EditorModel.cpp src/Core/EditorModel.h)
list(APPEND editorsrc src/Core/FolderMonitor.cpp src/Core/FolderMonitor.h)
list(APPEND editorsrc src/Core/Runloop.cpp src/Core/Runloop.h)
list(APPEND editorsrc src/Core/VerticalNavigationViewModel.cpp src/Core/VerticalNavigationViewModel.h)
list(APPEND editorsrc src/Core/Workspace.cpp src/Core/Workspace.h)

# API and Actions
list(APPEND editorsrc src/Core/Action.cpp src/Core/Action.h)
list(APPEND editorsrc src/Core/ActionHelper.cpp src/Core/ActionHelper.h)
list(APPEND editorsrc src/Core/KeyMapping.cpp src/Core/KeyMapping.h)
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
list(APPEND editorsrc src/Core/AnsiParser.cpp src/Core/AnsiParser.h)
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
list(APPEND editorsrc src/Core/UnicodeHelper.cpp src/Core/UnicodeHelper.h)

# Keyboard handling
list(APPEND editorsrc src/Core/KeyPress.cpp src/Core/KeyPress.h)
list(APPEND editorsrc src/Core/KeyboardDriverBase.cpp src/Core/KeyboardDriverBase.h)
list(APPEND editorsrc src/Core/Keyboard.cpp src/Core/Keyboard.cpp)

# Config
list(APPEND editorsrc src/Core/Config/Config.cpp src/Core/Config/Config.h)
list(APPEND editorsrc src/Core/Config/ConfigNode.cpp src/Core/Config/ConfigNode.h)
list(APPEND editorsrc src/Core/Config/YAMLMerge.cpp src/Core/Config/YAMLMerge.h)
list(APPEND editorsrc src/Core/Theme/Theme.cpp src/Core/Theme/Theme.h)
list(APPEND editorsrc src/Core/RuntimeConfig.cpp src/Core/RuntimeConfig.cpp)
list(APPEND editorsrc src/Core/WindowLocation.cpp src/Core/WindowLocation.h)
list(APPEND editorsrc src/Core/XDGEnvironment.cpp src/Core/XDGEnvironment.h)


# Native/Platform
list(APPEND editorsrc src/Core/NativeWindow.h)
list(APPEND editorsrc src/Core/ScreenBase.h)
list(APPEND editorsrc src/Core/WindowBase.h)
# NCurses
list(APPEND editorsrc src/Core/NCurses/NCursesDrawContext.cpp src/Core/NCurses/NCursesDrawContext.h)
list(APPEND editorsrc src/Core/NCurses/NCursesKeyboardDriver.cpp src/Core/NCurses/NCursesKeyboardDriver.h)
list(APPEND editorsrc src/Core/NCurses/NCursesScreen.cpp src/Core/NCurses/NCursesScreen.h)
list(APPEND editorsrc src/Core/NCurses/NCursesWindow.cpp src/Core/NCurses/NCursesWindow.h)
list(APPEND editorsrc src/Core/NCurses/NCursesColorRepository.cpp src/Core/NCurses/NCursesColorRepository.h)
list(APPEND editorsrc src/Core/NCurses/NCursesTranslate.cpp src/Core/NCurses/NCursesTranslate.h)
# base views
list(APPEND editorsrc src/Core/Views/HSplitView.h)
list(APPEND editorsrc src/Core/Views/HStackView.h)
list(APPEND editorsrc src/Core/Views/RootView.h)
list(APPEND editorsrc src/Core/Views/ViewBase.cpp src/Core/Views/ViewBase.h)
list(APPEND editorsrc src/Core/Views/VSplitView.h)
list(APPEND editorsrc src/Core/Views/VStackView.h)
list(APPEND editorsrc src/Core/Views/VisibleView.cpp src/Core/Views/VisibleView.h)
list(APPEND editorsrc src/Core/Views/TestView.cpp src/Core/Views/TestView.h)
# specific views
list(APPEND editorsrc src/Core/Views/EditorView.cpp src/Core/Views/EditorView.h)
list(APPEND editorsrc src/Core/Views/GutterView.cpp src/Core/Views/GutterView.h)
list(APPEND editorsrc src/Core/Views/CommandView.cpp src/Core/Views/CommandView.h)
list(APPEND editorsrc src/Core/Views/SingleLineView.h)
list(APPEND editorsrc src/Core/Views/EditorHeaderView.h)
list(APPEND editorsrc src/Core/Views/HSplitViewStatus.cpp src/Core/Views/HSplitViewStatus.h)
list(APPEND editorsrc src/Core/Views/TerminalView.cpp src/Core/Views/TerminalView.h)
list(APPEND editorsrc src/Core/Views/WorkspaceView.cpp src/Core/Views/WorkspaceView.h)

# modals/dialogs/etc..
list(APPEND editorsrc src/Core/Views/ModalView.cpp src/Core/Views/ModalView.h)
list(APPEND editorsrc src/Core/Views/ListSelectionModal.cpp src/Core/Views/ListSelectionModal.h)
list(APPEND editorsrc src/Core/Views/TreeView.h)
list(APPEND editorsrc src/Core/Views/TreeSelectionModal.cpp src/Core/Views/TreeSelectionModal.h)

# draw helper
list(APPEND editorsrc src/Core/DrawContext.cpp src/Core/DrawContext.h)
list(APPEND editorsrc src/Core/Point.h)
list(APPEND editorsrc src/Core/Rect.h)
list(APPEND editorsrc src/Core/LineRender.cpp src/Core/LineRender.h)
# controllers
list(APPEND editorsrc src/Core/Controllers/BaseController.cpp src/Core/Controllers/BaseController.h)
list(APPEND editorsrc src/Core/Controllers/EditController.cpp src/Core/Controllers/EditController.h)
list(APPEND editorsrc src/Core/Controllers/CommandController.cpp src/Core/Controllers/CommandController.h)
list(APPEND editorsrc src/Core/Controllers/TerminalController.cpp src/Core/Controllers/TerminalController.h)
list(APPEND editorsrc src/Core/Controllers/QuickCommandController.cpp src/Core/Controllers/QuickCommandController.h)
list(APPEND editorsrc src/Core/UndoHistory.cpp src/Core/UndoHistory.h)

# text buffer handling
list(APPEND editorsrc src/Core/Line.cpp src/Core/Line.h)
list(APPEND editorsrc src/Core/TextBuffer.cpp src/Core/TextBuffer.h)
# Language analysis
list(APPEND editorsrc src/Core/Tokenizer.cpp src/Core/Tokenizer.h)
list(APPEND editorsrc src/Core/Language/LangLineTokenizer.cpp src/Core/Language/LangLineTokenizer.h)
list(APPEND editorsrc src/Core/Language/LanguageBase.cpp src/Core/Language/LanguageBase.h)
list(APPEND editorsrc src/Core/Language/LangToken.cpp src/Core/Language/LangToken.h)
list(APPEND editorsrc src/Core/Language/LanguageSupport/DefaultLanguage.cpp src/Core/Language/LanguageSupport/DefaultLanguage.h)
list(APPEND editorsrc src/Core/Language/LanguageSupport/CPPLanguage.cpp src/Core/Language/LanguageSupport/CPPLanguage.h)
list(APPEND editorsrc src/Core/Language/LanguageSupport/JSONLanguage.cpp src/Core/Language/LanguageSupport/JSONLanguage.cpp)
list(APPEND editorsrc src/Core/Language/LanguageSupport/MakeBuildLang.cpp src/Core/Language/LanguageSupport/MakeBuildLang.h)
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



# SDL3 integration, make this optional!
# test integration files for editor
if (GEDIT_BUILD_SDL3)
    list(APPEND editorsrc src/Core/SDL3/SDLScreen.cpp src/Core/SDL3/SDLScreen.h)
    list(APPEND editorsrc src/Core/SDL3/SDLDrawContext.cpp src/Core/SDL3/SDLDrawContext.h)
    list(APPEND editorsrc src/Core/SDL3/SDLWindow.cpp src/Core/SDL3/SDLWindow.h)
    list(APPEND editorsrc src/Core/SDL3/SDLTranslate.cpp src/Core/SDL3/SDLTranslate.h)
    list(APPEND editorsrc src/Core/SDL3/SDLFontManager.cpp src/Core/SDL3/SDLFontManager.h)
    list(APPEND editorsrc src/Core/SDL3/SDLKeyboardDriver.cpp src/Core/SDL3/SDLKeyboardDriver.h)
    list(APPEND editorsrc src/Core/SDL3/SDLColor.h)
    list(APPEND editorsrc src/Core/SDL3/SDLCursor.h)
    # make this optional
    list(APPEND extlibs SDL3)
    add_definitions(-DGEDIT_USE_SDL3)
endif()

if (GEDIT_BUILD_SDL2)
    list(APPEND editorsrc src/Core/SDL2/SDLScreen.cpp src/Core/SDL2/SDLScreen.h)
    list(APPEND editorsrc src/Core/SDL2/SDLDrawContext.cpp src/Core/SDL2/SDLDrawContext.h)
    list(APPEND editorsrc src/Core/SDL2/SDLWindow.cpp src/Core/SDL2/SDLWindow.h)
    list(APPEND editorsrc src/Core/SDL2/SDLTranslate.cpp src/Core/SDL2/SDLTranslate.h)
    list(APPEND editorsrc src/Core/SDL2/SDLFontManager.cpp src/Core/SDL2/SDLFontManager.h)
    list(APPEND editorsrc src/Core/SDL2/SDLKeyboardDriver.cpp src/Core/SDL2/SDLKeyboardDriver.h)
    list(APPEND editorsrc src/Core/SDL2/SDLColor.h)
    list(APPEND editorsrc src/Core/SDL2/SDLCursor.h)
    # make this optional
    list(APPEND extlibs SDL2)
    add_definitions(-DGEDIT_USE_SDL2)
endif()


# append configuration stuff - these are copied with custom command


# include and link directories for project and dependencies
list(APPEND linkdirs ${NCURSES_HOME}/lib ${YAML_CPP_HOME}/lib)
list(APPEND linkdirs ${SDL_HOME}/lib)

#list(APPEND includedirs ${NCURSES_HOME}/include PUBLIC ${YAML_CPP_HOME}/include ext/logger/src src/)
list(APPEND includedirs ${NCURSES_HOME}/include PUBLIC ${YAML_CPP_HOME}/include ext/gnklog/src src/)
list(APPEND includedirs ${SDL_HOME}/include)

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

list(APPEND utestsrc utests/test_ansiparser.cpp)
list(APPEND utestsrc utests/test_assetloader.cpp)
list(APPEND utestsrc utests/test_clipboard.cpp)
list(APPEND utestsrc utests/test_cpplang.cpp)
list(APPEND utestsrc utests/test_config.cpp)
list(APPEND utestsrc utests/test_editorapi.cpp)
list(APPEND utestsrc utests/test_edtmodel.cpp)
list(APPEND utestsrc utests/test_jsengine.cpp)
list(APPEND utestsrc utests/test_keymapping.cpp)
list(APPEND utestsrc utests/test_logger.cpp)
list(APPEND utestsrc utests/test_strutil.cpp)
list(APPEND utestsrc utests/test_theme.cpp)
list(APPEND utestsrc utests/test_timer.cpp)
list(APPEND utestsrc utests/test_textbuffer.cpp)
list(APPEND utestsrc utests/test_vnav.cpp)
list(APPEND utestsrc utests/test_winlocation.cpp)
list(APPEND utestsrc utests/test_workspace.cpp)
list(APPEND utestsrc utests/test_xgdenv.cpp)

list(APPEND utestsrc src/Core/Tests/test_dcoverlay.cpp)



add_library(utests SHARED ${editorsrc} ${jsapi}  ${duktape} ${utestsrc})
#target_compile_options(logger PUBLIC -fPIC)
target_compile_options(utests PUBLIC -fPIC)
target_link_directories(utests PUBLIC ${linkdirs})
target_include_directories(utests PUBLIC ${includedirs})
target_link_libraries(utests ${extlibs})


add_custom_target(
        Unit_Tests ALL
        DEPENDS utests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

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
else()
    add_custom_target(resources ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/Assets/Resources ${EDITOR_ASSET_DIR})

    add_custom_target(plugins ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/src/Plugins ${EDITOR_ASSET_DIR}/Plugins)
endif()

#add_dependencies(${CUR_TARGET} config colors font plugins default_theme resources)
add_dependencies(${CUR_TARGET} plugins resources)
add_dependencies(Unit_Tests plugins resources)


#set_target_properties(logtest PROPERTIES EXCLUDE_FROM_ALL true)
set_target_properties(gnklog_utest PROPERTIES EXCLUDE_FROM_ALL true)
set_target_properties(utests PROPERTIES EXCLUDE_FROM_ALL true)
