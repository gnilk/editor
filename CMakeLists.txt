cmake_minimum_required(VERSION 3.22)
project(editor)

set(CMAKE_CXX_STANDARD 17)

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
    set(CMAKE_OSX_SYSROOT /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk)

    include_directories(AFTER SYSTEM /usr/local/include)

    list(APPEND editorsrc src/Core/macOS/MacOSKeyboardMonitor.cpp src/Core/macOS/MacOSKeyboardMonitor.h)
    list(APPEND editorsrc src/Core/unix/Shell.cpp src/Core/unix/Shell.h)

    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)
    find_library(GAMECONTROLLER_FRAMEWORK GameController)
    find_library(CORE_FOUNDATION CoreFoundation)

    list(APPEND extlibs ncurses)
    list(APPEND extlibs yaml-cpp)

    # list(APPEND extlibs ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${GAMECONTROLLER_FRAMEWORK} ${CORE_FOUNDATION})
    list(APPEND extlibs ${IOKIT_FRAMEWORK} ${CORE_FOUNDATION})
endif()


list(APPEND editorsrc src/Core/Line.cpp src/Core/Line.h)
list(APPEND editorsrc src/Core/StrUtil.cpp src/Core/StrUtil.h)
list(APPEND editorsrc src/Core/ModeBase.cpp src/Core/ModeBase.h)
list(APPEND editorsrc src/Core/CommandMode.cpp src/Core/CommandMode.cpp)
list(APPEND editorsrc src/Core/EditorMode.cpp src/Core/EditorMode.cpp)
list(APPEND editorsrc src/Core/KeyboardDriverBase.cpp src/Core/KeyboardDriverBase.h)
list(APPEND editorsrc src/Core/RuntimeConfig.cpp src/Core/RuntimeConfig.cpp)
list(APPEND editorsrc src/Core/KeyPress.cpp src/Core/KeyPress.h)
list(APPEND editorsrc src/Core/Config/Config.cpp src/Core/Config/Config.h)

# TODO: Revisit this - perhaps better somewhere else
list(APPEND editorsrc src/Core/NCurses/NCursesScreen.cpp src/Core/NCurses/NCursesScreen.h)
list(APPEND editorsrc src/Core/NCurses/NCursesKeyboardDriver.cpp src/Core/NCurses/NCursesKeyboardDriver.h)

list(APPEND appsrc main.cpp)

## THIS IS FOR macOS - homebrew installation...
set(NCURSES_HOME /opt/homebrew/opt/ncurses)
set(YAML_CPP_HOME /opt/homebrew/opt/yaml-cpp)



add_executable(editor ${appsrc} ${editorsrc})
target_include_directories(editor PUBLIC src)


target_include_directories(editor PUBLIC ${NCURSES_HOME}/include)
target_link_directories(editor  PUBLIC ${NCURSES_HOME}/lib)

target_include_directories(editor PUBLIC ${YAML_CPP_HOME}/include)
target_link_directories(editor  PUBLIC ${YAML_CPP_HOME}/lib)


target_link_libraries(editor ${extlibs})

add_executable(kbdhook tests/kbdhooking.m)
target_link_libraries(kbdhook ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${GAMECONTROLLER_FRAMEWORK} ${CORE_FOUNDATION})

add_executable(gckeys tests/gckeys.m)
target_link_libraries(gckeys ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${GAMECONTROLLER_FRAMEWORK} ${CORE_FOUNDATION})

add_executable(myshell tests/myshell.cpp)
target_link_libraries(myshell ${COCOA_FRAMEWORK} ${IOKIT_FRAMEWORK} ${GAMECONTROLLER_FRAMEWORK} ${CORE_FOUNDATION})

add_executable(myshell2 tests/myshell2.cpp src/Core/unix/Shell.cpp src/Core/StrUtil.cpp)
target_include_directories(myshell2 PUBLIC src/)

add_executable(strattr tests/strsttr.cpp src/Core/Line.cpp src/Core/Line.h src/Core/StrUtil.cpp tests/Tokenizer.cpp tests/Tokenizer.h)
target_include_directories(strattr PUBLIC src)
target_include_directories(strattr PUBLIC ${NCURSES_HOME}/include)
target_link_directories(strattr  PUBLIC ${NCURSES_HOME}/lib)

target_link_libraries(strattr ncurses)

