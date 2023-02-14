//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_MODEBASE_H
#define EDITOR_MODEBASE_H

#include <vector>
#include "Core/Line.h"
#include "Core/Cursor.h"
#include "Core/KeyboardDriverBase.h"

// TEMP
#include "NCurses/NCursesKeyboardDriver.h"

// This defines a general purpose output console
class IOutputConsole {
public:
    virtual void WriteLine(const std::string &str) = 0;
};

class ModeBase {
public:
    using OnExitMode = std::function<void()>;
    using OnExitApp = std::function<void()>;
public:
    ModeBase() = default;
    void SetColumnOffset(int newColumnOffset) { columnOffset = newColumnOffset; }
    void SetOnExitMode(OnExitMode newOnExitMode) { onExitMode = newOnExitMode; }
    void SetOnExitApp(OnExitApp newOnExitApp) { onExitApp = newOnExitApp; }
    // Called before update-loop, one time initialization goes here
    virtual bool Begin() { return true; }
    // These are called in an update loop - DO NOT BLOCK!
    virtual void DrawLines() {}
    virtual void Update() {}
    virtual const std::vector<Line *> &Lines() const = 0;
    // Called when user switches mode
    virtual void OnSwitchMode(bool enter) {};
    // Holds default editing logic..
    bool DefaultEditLine(Line *line, KeyPress &ch);

    bool DefaultEditLine(Line *line, gedit::NCursesKeyboardDriverNew::KeyPress &keyPress);

protected:
    OnExitMode onExitMode = nullptr;
    OnExitApp  onExitApp = nullptr;
    // not sure...
    Cursor cursor = {0,0};
    int columnOffset = 0;
};

#endif //EDITOR_MODEBASE_H
