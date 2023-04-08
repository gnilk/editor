cmake_minimum_required(VERSION 3.22)
project(editor)
set(CMAKE_CXX_STANDARD 17)

set(CMAKE_BUILD_TYPE DEBUG)

set(SDL_HOME /usr/local)

list(APPEND appsrc main.cpp)

set(CUR_TARGET editor)

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
    set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)

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

add_subdirectory(ext/logger ${CMAKE_BINARY_DIR}/logger)
list(APPEND extlibs logger)

# Editor main stuff
list(APPEND editorsrc src/Core/Editor.cpp src/Core/Editor.h)
list(APPEND editorsrc src/Core/EditorModel.h)

# API and Actions
list(APPEND editorsrc src/Core/Action.cpp src/Core/Action.h)
list(APPEND editorsrc src/Core/KeyMapping.cpp src/Core/KeyMapping.h)
list(APPEND editorsrc src/Core/API/EditorAPI.cpp src/Core/API/EditorAPI.h)

# utility stuff
list(APPEND editorsrc src/Core/StrUtil.cpp src/Core/StrUtil.h)
list(APPEND editorsrc src/Core/HexDump.cpp src/Core/HexDump.h)
list(APPEND editorsrc src/Core/ColorRGBA.cpp src/Core/ColorRGBA.h)

# Keyboard handling
list(APPEND editorsrc src/Core/KeyPress.h)
list(APPEND editorsrc src/Core/KeyboardDriverBase.cpp src/Core/KeyboardDriverBase.h)

# Config
list(APPEND editorsrc src/Core/RuntimeConfig.cpp src/Core/RuntimeConfig.cpp)
list(APPEND editorsrc src/Core/Config/Config.cpp src/Core/Config/Config.h)
list(APPEND editorsrc src/Core/Config/ColorConfig.cpp src/Core/Config/ColorConfig.h)

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
# specific views
list(APPEND editorsrc src/Core/Views/EditorView.cpp src/Core/Views/EditorView.h)
list(APPEND editorsrc src/Core/Views/GutterView.cpp src/Core/Views/GutterView.h)
list(APPEND editorsrc src/Core/Views/CommandView.cpp src/Core/Views/CommandView.h)
list(APPEND editorsrc src/Core/Views/SingleLineView.h)
list(APPEND editorsrc src/Core/Views/HeaderView.cpp src/Core/Views/HeaderView.h)
list(APPEND editorsrc src/Core/Views/HSplitViewStatus.h)

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
list(APPEND editorsrc src/Core/Buffer.cpp src/Core/Buffer.h)
list(APPEND editorsrc src/Core/Line.cpp src/Core/Line.h)
list(APPEND editorsrc src/Core/BufferManager.cpp src/Core/BufferManager.h)
list(APPEND editorsrc src/Core/TextBuffer.cpp src/Core/TextBuffer.h)
# Language analysis
list(APPEND editorsrc src/Core/Tokenizer.cpp src/Core/Tokenizer.h)
list(APPEND editorsrc src/Core/Language/LangLineTokenizer.cpp src/Core/Language/LangLineTokenizer.h)
list(APPEND editorsrc src/Core/Language/LanguageBase.cpp src/Core/Language/LanguageBase.h)
list(APPEND editorsrc src/Core/Language/CPP/CPPLanguage.cpp src/Core/Language/CPP/CPPLanguage.h)
list(APPEND editorsrc src/Core/Language/LangToken.cpp src/Core/Language/LangToken.h)
# sublime config/scripting support
list(APPEND editorsrc src/Core/Sublime/SublimeConfigScriptEngine.cpp src/Core/Sublime/SublimeConfigScriptEngine.h)
list(APPEND editorsrc src/Core/Sublime/SublimeConfigColorScript.cpp src/Core/Sublime/SublimeConfigColorScript.h)


# SDL3 integration, make this optional!
# test integration files for editor
list(APPEND editorsrc src/Core/SDL3/SDLScreen.cpp src/Core/SDL3/SDLScreen.h)
list(APPEND editorsrc src/Core/SDL3/SDLDrawContext.cpp src/Core/SDL3/SDLDrawContext.h)
list(APPEND editorsrc src/Core/SDL3/SDLWindow.cpp src/Core/SDL3/SDLWindow.h)
list(APPEND editorsrc src/Core/SDL3/SDLTranslate.cpp src/Core/SDL3/SDLTranslate.h)
list(APPEND editorsrc src/Core/SDL3/SDLFontManager.cpp src/Core/SDL3/SDLFontManager.h)
list(APPEND editorsrc src/Core/SDL3/SDLKeyboardDriver.cpp src/Core/SDL3/SDLKeyboardDriver.h)
list(APPEND editorsrc src/Core/SDL3/SDLColorRepository.cpp src/Core/SDL3/SDLColorRepository.h)
list(APPEND editorsrc src/Core/SDL3/SDLColor.h)
list(APPEND editorsrc src/Core/SDL3/SDLCursor.h)

# append configuration stuff - these are copied with custom command


# include and link directories for project and dependencies
list(APPEND linkdirs ${NCURSES_HOME}/lib ${YAML_CPP_HOME}/lib)
list(APPEND linkdirs ${SDL_HOME}/lib)

list(APPEND includedirs ${NCURSES_HOME}/include PUBLIC ${YAML_CPP_HOME}/include ext/logger/src src/)
list(APPEND includedirs ${SDL_HOME}/include)

# make this optional
list(APPEND extlibs SDL3)



target_sources(editor PUBLIC ${editorsrc})
target_link_directories(editor PUBLIC ${linkdirs})
target_include_directories(editor PUBLIC ${includedirs})
target_link_libraries(editor ${extlibs})

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
# Add configuration files so they get copied when changed...
#
ADD_CUSTOM_TARGET(config ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_LIST_DIR}/config.yml ${CMAKE_CURRENT_BINARY_DIR})

ADD_CUSTOM_TARGET(colors ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_LIST_DIR}/colors.sublime.json ${CMAKE_CURRENT_BINARY_DIR})

ADD_CUSTOM_TARGET(font ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_LIST_DIR}/Andale\ Mono.ttf ${CMAKE_CURRENT_BINARY_DIR})

ADD_DEPENDENCIES(editor config colors font)

#
# unit testing
#
list(APPEND utestsrc utests/test_buffermanager.cpp)
list(APPEND utestsrc src/Core/Tests/test_dcoverlay.cpp)



add_library(utests SHARED ${editorsrc} ${utestsrc})
target_link_directories(utests PUBLIC ${linkdirs})
target_include_directories(utests PUBLIC ${includedirs})
target_link_libraries(utests ${extlibs})


add_custom_target(
        Unit_Tests ALL
        DEPENDS utests
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
