cmake_minimum_required(VERSION 3.22)
project(editor)
set(CMAKE_CXX_STANDARD 17)


set(CMAKE_BUILD_TYPE DEBUG)

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
    set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)

    include_directories(AFTER SYSTEM /usr/local/include)

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

    # list(APPEND extlibs ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${GAMECONTROLLER_FRAMEWORK} ${CORE_FOUNDATION})
    list(APPEND extlibs ${IOKIT_FRAMEWORK} ${CORE_FOUNDATION})
endif()

add_subdirectory(ext/logger ${CMAKE_BINARY_DIR}/logger)
list(APPEND extlibs logger)


list(APPEND editorsrc src/Core/Buffer.cpp src/Core/Buffer.h)
list(APPEND editorsrc src/Core/Line.cpp src/Core/Line.h)
list(APPEND editorsrc src/Core/StrUtil.cpp src/Core/StrUtil.h)
list(APPEND editorsrc src/Core/ModeBase.cpp src/Core/ModeBase.h)
list(APPEND editorsrc src/Core/CommandMode.cpp src/Core/CommandMode.cpp)
list(APPEND editorsrc src/Core/EditorMode.cpp src/Core/EditorMode.cpp)
list(APPEND editorsrc src/Core/KeyboardDriverBase.cpp src/Core/KeyboardDriverBase.h)
list(APPEND editorsrc src/Core/RuntimeConfig.cpp src/Core/RuntimeConfig.cpp)
list(APPEND editorsrc src/Core/KeyPress.cpp src/Core/KeyPress.h)
list(APPEND editorsrc src/Core/Tokenizer.cpp src/Core/Tokenizer.h)
list(APPEND editorsrc src/Core/ColorRGBA.cpp src/Core/ColorRGBA.h)
list(APPEND editorsrc src/Core/Language/LangLineTokenizer.cpp src/Core/Language/LangLineTokenizer.h)
list(APPEND editorsrc src/Core/HexDump.cpp src/Core/HexDump.h)
list(APPEND editorsrc src/Core/Config/Config.cpp src/Core/Config/Config.h)
list(APPEND editorsrc src/Core/Config/ColorConfig.cpp src/Core/Config/ColorConfig.h)
list(APPEND editorsrc src/Core/ViewBase.cpp src/Core/ViewBase.h)
list(APPEND editorsrc src/Core/EditorView.cpp src/Core/EditorView.h)
list(APPEND editorsrc src/Core/Point.h)
list(APPEND editorsrc src/Core/Rect.h)

#
# Language analysis
#
list(APPEND editorsrc src/Core/Language/LanguageBase.cpp src/Core/Language/LanguageBase.h)
list(APPEND editorsrc src/Core/Language/CPP/CPPLanguage.cpp src/Core/Language/CPP/CPPLanguage.h)
list(APPEND editorsrc src/Core/Language/LangToken.cpp src/Core/Language/LangToken.h)


# sublime config/scripting support
list(APPEND editorsrc src/Core/Sublime/SublimeConfigScriptEngine.cpp src/Core/Sublime/SublimeConfigScriptEngine.h)
list(APPEND editorsrc src/Core/Sublime/SublimeConfigColorScript.cpp src/Core/Sublime/SublimeConfigColorScript.h)

# TODO: Revisit this - perhaps better somewhere else
list(APPEND editorsrc src/Core/NCurses/NCursesScreen.cpp src/Core/NCurses/NCursesScreen.h)
list(APPEND editorsrc src/Core/NCurses/NCursesKeyboardDriver.cpp src/Core/NCurses/NCursesKeyboardDriver.h)

list(APPEND appsrc main.cpp)

## THIS IS FOR macOS - homebrew installation...
set(NCURSES_HOME /opt/homebrew/opt/ncurses)
set(YAML_CPP_HOME /opt/homebrew/opt/yaml-cpp)



add_executable(editor ${appsrc} ${editorsrc})
target_include_directories(editor PUBLIC ext/logger/src)

target_include_directories(editor PUBLIC src)


target_include_directories(editor PUBLIC ${NCURSES_HOME}/include)
target_link_directories(editor  PUBLIC ${NCURSES_HOME}/lib)

target_include_directories(editor PUBLIC ${YAML_CPP_HOME}/include)
target_link_directories(editor  PUBLIC ${YAML_CPP_HOME}/lib)



target_link_libraries(editor ${extlibs})

add_executable(kbdhook tests/kbdhooking.m)
target_link_libraries(kbdhook ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${GAMECONTROLLER_FRAMEWORK} ${CORE_FOUNDATION})

#
# Keyboard monitor test
#
add_executable(kbdmon tests/kbdmon.cpp tests/MacOSKeyMonCGEvents.cpp ${editorsrc})
target_include_directories(kbdmon PUBLIC ${NCURSES_HOME}/include)
target_link_directories(kbdmon  PUBLIC ${NCURSES_HOME}/lib)
target_include_directories(kbdmon PUBLIC ${YAML_CPP_HOME}/include)
target_link_directories(kbdmon  PUBLIC ${YAML_CPP_HOME}/lib)
target_include_directories(kbdmon PUBLIC ext/logger/src)
target_include_directories(kbdmon PUBLIC src)
target_link_libraries(kbdmon ${extlibs}  ${CARBON_FRAMEWORK})
# end kbdmon

add_executable(gckeys tests/gckeys.m)
target_link_libraries(gckeys ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${GAMECONTROLLER_FRAMEWORK} ${CORE_FOUNDATION})

add_executable(myshell tests/myshell.cpp)
target_link_libraries(myshell ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${GAMECONTROLLER_FRAMEWORK} ${CORE_FOUNDATION})

add_executable(myshell2 tests/myshell2.cpp src/Core/unix/Shell.cpp src/Core/StrUtil.cpp)
target_include_directories(myshell2 PUBLIC src/)

add_executable(tstlog tests/tstlog.cpp)
target_include_directories(tstlog PUBLIC ext/logger/src)
target_link_libraries(tstlog logger)


#
# I include all of the editor stuff in this test app
#
add_executable(strattr tests/strsttr.cpp ${editorsrc})
target_include_directories(strattr PUBLIC src)
target_include_directories(strattr PUBLIC ${NCURSES_HOME}/include)
target_link_directories(strattr  PUBLIC ${NCURSES_HOME}/lib)
target_include_directories(strattr PUBLIC ${YAML_CPP_HOME}/include)
target_link_directories(strattr  PUBLIC ${YAML_CPP_HOME}/lib)

target_link_libraries(strattr ${extlibs})


add_executable(focuscheck tests/focusdet.cpp tests/focus_oc_wrapper.mm)
target_link_libraries(focuscheck ${CORE_FOUNDATION} ${APPKIT})

add_executable(cgsniff tests/cgeventsniff.cpp)
target_link_libraries(cgsniff ${CORE_FOUNDATION} ${CORE_SERVICES} ${CORE_GRAPHICS} ${APPKIT})

add_executable(nseventmon tests/nseventmon.m)
target_link_libraries(nseventmon ${CORE_FOUNDATION} ${CORE_SERVICES} ${CORE_GRAPHICS} ${APPKIT})

#
# view handling test renderering
#
add_executable(renderview tests/viewrender.cpp ${editorsrc})
target_include_directories(renderview PUBLIC ext/logger/src)
target_include_directories(renderview PUBLIC src)
target_include_directories(renderview PUBLIC ${NCURSES_HOME}/include)
target_link_directories(renderview  PUBLIC ${NCURSES_HOME}/lib)
target_include_directories(renderview PUBLIC ${YAML_CPP_HOME}/include)
target_link_directories(renderview  PUBLIC ${YAML_CPP_HOME}/lib)
target_link_libraries(renderview ${extlibs})
