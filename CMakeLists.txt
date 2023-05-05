cmake_minimum_required(VERSION 3.22)
project(editor)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE DEBUG)

option(GEDIT_BUILD_SDL3 "Build SDL3" OFF)
option(GEDIT_BUILD_SDL2 "Build SDL2" ON)

set(SDL_HOME /usr/local)

list(APPEND appsrc main.cpp)

set(CUR_TARGET editor)

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
    set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)

    # Not sure if this is the right way to change the macOS defaults...
    set(GEDIT_BUILD_SDL3 ON)
    set(GEDIT_BUILD_SDL2 OFF)

    add_definitions(-DGEDIT_MACOS)

    include_directories(AFTER SYSTEM /usr/local/include)

    ## THIS IS FOR macOS - homebrew installation...
    set(NCURSES_HOME /opt/homebrew/opt/ncurses)
    set(YAML_CPP_HOME /opt/homebrew/opt/yaml-cpp)


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

    add_executable(${CUR_TARGET} MACOSX_BUNDLE ${appsrc})
    set_target_properties(${CUR_TARGET} PROPERTIES
            BUNDLE True
            MACOSX_BUNDLE_GUI_IDENTIFIER gnilk.com.${CUR_TARGET}
            MACOSX_BUNDLE_BUNDLE_NAME ${CUR_TARGET}
            MACOSX_BUNDLE_BUNDLE_VERSION "0.1"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1"
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_SOURCE_DIR}/Assets/CMake/Info.plist.in
            )


    # list(APPEND extlibs ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${GAMECONTROLLER_FRAMEWORK} ${CORE_FOUNDATION})
    list(APPEND extlibs ${IOKIT_FRAMEWORK} ${CORE_FOUNDATION})
elseif(UNIX)
    message(STATUS "Linux build")
    set(YAML_CPP_HOME /usr)
    set(NCURSES_HOME /usr)
    set(SDL_HOME /usr/local)

    list(APPEND includedirs /home/gnilk/src/ext/json/single_include)

    list(APPEND editorsrc src/Core/unix/Shell.cpp src/Core/unix/Shell.h)

    add_definitions(-DGEDIT_LINUX)

    list(APPEND extlibs util)   ## see: man forkpty
    list(APPEND extlibs ncurses)
    list(APPEND extlibs yaml-cpp)
    add_executable(${CUR_TARGET} ${appsrc})

endif ()

#
# Dependencies
#

set(DUKTAPE_HOME ext/duktape-2.7.0/src)
set(DUKTAPE_EXTRAS ext/duktape-2.7.0/extras)
set(DUKGLUE_HOME ext/dukglue)

list(APPEND duktape ${DUKTAPE_HOME}/duktape.c ${DUKTAPE_HOME}/duk_config.h ${DUKTAPE_HOME}/duktape.h)
list(APPEND duktape ${DUKTAPE_EXTRAS}/module-node/duk_module_node.c)

add_subdirectory(ext/logger ${CMAKE_BINARY_DIR}/logger)
list(APPEND extlibs logger)

#
# Editor
#

# Editor main stuff
list(APPEND editorsrc src/Core/Editor.cpp src/Core/Editor.h)
list(APPEND editorsrc src/Core/EditorModel.cpp src/Core/EditorModel.h)
list(APPEND editorsrc src/Core/VerticalNavigationViewModel.cpp src/Core/VerticalNavigationViewModel.h)
list(APPEND editorsrc src/Core/Runloop.cpp src/Core/Runloop.h)
list(APPEND editorsrc src/Core/AssetLoaderBase.cpp src/Core/AssetLoaderBase.h)

# API and Actions
list(APPEND editorsrc src/Core/Action.cpp src/Core/Action.h)
list(APPEND editorsrc src/Core/KeyMapping.cpp src/Core/KeyMapping.h)
list(APPEND editorsrc src/Core/API/EditorAPI.cpp src/Core/API/EditorAPI.h)
list(APPEND editorsrc src/Core/API/TextBufferAPI.cpp src/Core/API/TextBufferAPI.h)
list(APPEND editorsrc src/Core/Plugins/PluginCommand.cpp src/Core/Plugins/PluginCommand.h)

# utility stuff
list(APPEND editorsrc src/Core/StrUtil.cpp src/Core/StrUtil.h)
list(APPEND editorsrc src/Core/HexDump.cpp src/Core/HexDump.h)
list(APPEND editorsrc src/Core/ColorRGBA.cpp src/Core/ColorRGBA.h)
list(APPEND editorsrc src/Core/TypeUtil.h)

# Keyboard handling
list(APPEND editorsrc src/Core/KeyPress.h)
list(APPEND editorsrc src/Core/KeyboardDriverBase.cpp src/Core/KeyboardDriverBase.h)

# Config
list(APPEND editorsrc src/Core/RuntimeConfig.cpp src/Core/RuntimeConfig.cpp)
list(APPEND editorsrc src/Core/Config/Config.cpp src/Core/Config/Config.h)
list(APPEND editorsrc src/Core/Config/NamedColorConfig.cpp src/Core/Config/NamedColorConfig.h)

# Native/Platform
list(APPEND editorsrc src/Core/NativeWindow.h)
list(APPEND editorsrc src/Core/ScreenBase.h)
list(APPEND editorsrc src/Core/WindowBase.h)
# NCurses
list(APPEND editorsrc src/Core/NCurses/NCursesDrawContext.cpp src/Core/NCurses/NCursesDrawContext.h)
list(APPEND editorsrc src/Core/NCurses/NCursesKeyboardDriver.cpp src/Core/NCurses/NCursesKeyboardDriver.h)
list(APPEND editorsrc src/Core/NCurses/NCursesScreen.cpp src/Core/NCurses/NCursesScreen.h)
list(APPEND editorsrc src/Core/NCurses/NCursesWindow.cpp src/Core/NCurses/NCursesWindow.h)

# base views
list(APPEND editorsrc src/Core/Views/HSplitView.h)
list(APPEND editorsrc src/Core/Views/HStackView.h)
list(APPEND editorsrc src/Core/Views/RootView.h)
list(APPEND editorsrc src/Core/Views/ViewBase.cpp src/Core/Views/ViewBase.h)
list(APPEND editorsrc src/Core/Views/VSplitView.h)
list(APPEND editorsrc src/Core/Views/VStackView.h)
list(APPEND editorsrc src/Core/Views/VisibleView.cpp src/Core/Views/VisibleView.h)
# specific views
list(APPEND editorsrc src/Core/Views/EditorView.cpp src/Core/Views/EditorView.h)
list(APPEND editorsrc src/Core/Views/GutterView.cpp src/Core/Views/GutterView.h)
list(APPEND editorsrc src/Core/Views/CommandView.cpp src/Core/Views/CommandView.h)
list(APPEND editorsrc src/Core/Views/SingleLineView.h)
list(APPEND editorsrc src/Core/Views/EditorHeaderView.h)
list(APPEND editorsrc src/Core/Views/HSplitViewStatus.h)

# modals/dialogs/etc..
list(APPEND editorsrc src/Core/Views/ModalView.cpp src/Core/Views/ModalView.h)
list(APPEND editorsrc src/Core/Views/ListSelectionModal.cpp src/Core/Views/ListSelectionModal.h)
list(APPEND editorsrc src/Core/Views/TreeView.h)
list(APPEND editorsrc src/Core/Views/TreeSelectionModal.cpp src/Core/Views/TreeSelectionModal.h)

# draw helper
list(APPEND editorsrc src/Core/DrawContext.h)
list(APPEND editorsrc src/Core/Point.h)
list(APPEND editorsrc src/Core/Rect.h)
list(APPEND editorsrc src/Core/LineRender.cpp src/Core/LineRender.h)
# controllers
list(APPEND editorsrc src/Core/Controllers/BaseController.cpp src/Core/Controllers/BaseController.h)
list(APPEND editorsrc src/Core/Controllers/EditController.cpp src/Core/Controllers/EditController.h)
list(APPEND editorsrc src/Core/Controllers/CommandController.cpp src/Core/Controllers/CommandController.h)
# text buffer handling
list(APPEND editorsrc src/Core/Line.cpp src/Core/Line.h)
list(APPEND editorsrc src/Core/BufferManager.cpp src/Core/BufferManager.h)
list(APPEND editorsrc src/Core/TextBuffer.cpp src/Core/TextBuffer.h)
# Language analysis
list(APPEND editorsrc src/Core/Tokenizer.cpp src/Core/Tokenizer.h)
list(APPEND editorsrc src/Core/Language/LangLineTokenizer.cpp src/Core/Language/LangLineTokenizer.h)
list(APPEND editorsrc src/Core/Language/LanguageBase.cpp src/Core/Language/LanguageBase.h)
list(APPEND editorsrc src/Core/Language/LangToken.cpp src/Core/Language/LangToken.h)
list(APPEND editorsrc src/Core/Language/LanguageSupport/DefaultLanguage.cpp src/Core/Language/LanguageSupport/DefaultLanguage.h)
list(APPEND editorsrc src/Core/Language/LanguageSupport/CPPLanguage.cpp src/Core/Language/LanguageSupport/CPPLanguage.h)
list(APPEND editorsrc src/Core/Language/LanguageSupport/JSONLanguage.cpp src/Core/Language/LanguageSupport/JSONLanguage.cpp)
# sublime config/scripting support
list(APPEND editorsrc src/Core/Sublime/SublimeConfigScriptEngine.cpp src/Core/Sublime/SublimeConfigScriptEngine.h)
list(APPEND editorsrc src/Core/Sublime/SublimeConfigColorScript.cpp src/Core/Sublime/SublimeConfigColorScript.h)


#
# JSAPI - separate
#
list(APPEND jsapi src/Core/JSEngine/JSPluginEngine.cpp src/Core/JSEngine/JSPluginEngine.h)
list(APPEND jsapi src/Core/JSEngine/JSPluginCommand.cpp src/Core/JSEngine/JSPluginCommand.h)
list(APPEND jsapi src/Core/JSEngine/Modules/TextBufferAPIWrapper.cpp src/Core/JSEngine/Modules/TextBufferAPIWrapper.h)
list(APPEND jsapi src/Core/JSEngine/Modules/EditorAPIWrapper.cpp src/Core/JSEngine/Modules/EditorAPIWrapper.h)
list(APPEND jsapi src/Core/JSEngine/Modules/ConsoleAPIWrapper.cpp src/Core/JSEngine/Modules/ConsoleAPIWrapper.h)



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
    list(APPEND editorsrc src/Core/SDL2/SDLColorRepository.cpp src/Core/SDL2/SDLColorRepository.h)
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

list(APPEND includedirs ${NCURSES_HOME}/include PUBLIC ${YAML_CPP_HOME}/include ext/logger/src src/)
list(APPEND includedirs ${SDL_HOME}/include)

list(APPEND includedirs ${DUKGLUE_HOME}/include)
list(APPEND includedirs ${DUKTAPE_HOME})
list(APPEND includedirs ${DUKTAPE_EXTRAS}/module-node)




target_sources(editor PUBLIC ${editorsrc} ${jsapi} ${duktape})
target_link_directories(editor PUBLIC ${linkdirs})
target_include_directories(editor PUBLIC ${includedirs})
target_link_libraries(editor ${extlibs})

# Ok, we should have enabled this from the beginning...
#target_compile_options(editor PUBLIC -Wall -Wextra -Wpedantic)
#target_compile_options(editor PUBLIC -Wno-unused-parameter)

#add_executable(kbdhook tests/kbdhooking.m)
#target_link_libraries(kbdhook ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${GAMECONTROLLER_FRAMEWORK} ${CORE_FOUNDATION})

#
# Keyboard monitor test
#
#add_executable(kbdmon tests/kbdmon.cpp tests/MacOSKeyMonCGEvents.cpp ${editorsrc})
#target_link_directories(kbdmon PUBLIC ${linkdirs})
#target_include_directories(kbdmon PUBLIC ${includedirs})
#target_link_libraries(kbdmon ${extlibs}  ${CARBON_FRAMEWORK})
## end kbdmon
#
#add_executable(gckeys tests/gckeys.m)
#target_link_libraries(gckeys ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${GAMECONTROLLER_FRAMEWORK} ${CORE_FOUNDATION})
#
#add_executable(myshell tests/myshell.cpp)
#target_link_libraries(myshell ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${GAMECONTROLLER_FRAMEWORK} ${CORE_FOUNDATION})
#
#add_executable(myshell2 tests/myshell2.cpp src/Core/unix/Shell.cpp src/Core/StrUtil.cpp)
#target_include_directories(myshell2 PUBLIC src/)
#
#add_executable(tstlog tests/tstlog.cpp)
#target_include_directories(tstlog PUBLIC ext/logger/src)
#target_link_libraries(tstlog logger)
#
# I include all of the editor stuff in this test app
#
#add_executable(strattr tests/strsttr.cpp ${editorsrc})
#target_include_directories(strattr PUBLIC src)
#target_include_directories(strattr PUBLIC ${NCURSES_HOME}/include)
#target_link_directories(strattr  PUBLIC ${NCURSES_HOME}/lib)
#target_include_directories(strattr PUBLIC ${YAML_CPP_HOME}/include)
#target_link_directories(strattr  PUBLIC ${YAML_CPP_HOME}/lib)
#target_link_libraries(strattr ${extlibs})


#add_executable(focuscheck tests/focusdet.cpp tests/focus_oc_wrapper.mm)
#target_link_libraries(focuscheck ${CORE_FOUNDATION} ${APPKIT})
#
#add_executable(cgsniff tests/cgeventsniff.cpp)
#target_link_libraries(cgsniff ${CORE_FOUNDATION} ${CORE_SERVICES} ${CORE_GRAPHICS} ${APPKIT})
#
#add_executable(nseventmon tests/nseventmon.m)
#target_link_libraries(nseventmon ${CORE_FOUNDATION} ${CORE_SERVICES} ${CORE_GRAPHICS} ${APPKIT})

#
# view handling test renderering
#
#add_executable(renderview tests/viewrender.cpp ${editorsrc})
#target_link_directories(renderview PUBLIC ${linkdirs})
#target_include_directories(renderview PUBLIC ${includedirs})
#target_link_libraries(renderview ${extlibs})
#
#add_executable(testlayout tests/testlayout.cpp ${editorsrc})
#target_link_directories(kbdmon PUBLIC ${linkdirs})
#target_include_directories(kbdmon PUBLIC ${includedirs})
#target_link_libraries(testlayout ${extlibs})
#
#add_executable(testscroll tests/testscroll.cpp ${editorsrc})
#target_link_directories(testscroll PUBLIC ${linkdirs})
#target_include_directories(testscroll PUBLIC ${includedirs})
#target_link_libraries(testscroll ${extlibs})




#add_library(libeditor SHARED ${editorsrc} ${utestsrc})
#target_link_directories(libeditor PUBLIC ${linkdirs})
#target_include_directories(libeditor PUBLIC ${includedirs})
#target_link_libraries(libeditor ${extlibs})

#
# unit testing
#
list(APPEND utestsrc utests/test_main.cpp)
list(APPEND utestsrc utests/test_buffermanager.cpp)
list(APPEND utestsrc utests/test_assetloader.cpp)
list(APPEND utestsrc utests/test_jsengine.cpp)
list(APPEND utestsrc utests/test_editorapi.cpp)
list(APPEND utestsrc src/Core/Tests/test_dcoverlay.cpp)



add_library(utests SHARED ${editorsrc} ${jsapi}  ${duktape} ${utestsrc})
target_link_directories(utests PUBLIC ${linkdirs})
target_include_directories(utests PUBLIC ${includedirs})
target_link_libraries(utests ${extlibs})


add_custom_target(
        Unit_Tests ALL
        DEPENDS utests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

#
# Add configuration files so they get copied when changed...
#
add_custom_target(config ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_LIST_DIR}/config.yml ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(colors ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_LIST_DIR}/colors.json ${CMAKE_CURRENT_BINARY_DIR})

add_custom_target(font ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_LIST_DIR}/Andale\ Mono.ttf ${CMAKE_CURRENT_BINARY_DIR})


# copy_directory_if_different available in cmake 3.26 or better...
if (${CMAKE_VERSION} VERSION_LESS "3.26.0")
    add_custom_target(plugins ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/src/Plugins ${CMAKE_CURRENT_BINARY_DIR}/Plugins)
else()
    add_custom_target(plugins ${CMAKE_COMMAND} -E copy_directory_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/src/Plugins ${CMAKE_CURRENT_BINARY_DIR}/Plugins)
endif()

add_dependencies(editor config colors font plugins)
add_dependencies(Unit_Tests config colors font plugins)
