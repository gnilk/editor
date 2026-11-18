//
// StartupNotices - see the header
//

#include <cstdio>

#include "Core/StartupNotices.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

void StartupNotices::Add(const std::string &text) {
    auto line = "goatedit: " + text;
    if (isEchoToStderr) {
        fprintf(stderr, "%s\n", line.c_str());
    }
    IOutputConsole *target = nullptr;
    {
        std::lock_guard<std::mutex> guard(lock);
        if (console == nullptr) {
            held.push_back(line);
            return;
        }
        target = console;
    }
    target->WriteLine(UnicodeHelper::utf8to32(line));
}

void StartupNotices::Attach(IOutputConsole *newConsole) {
    std::vector<std::string> lines;
    {
        std::lock_guard<std::mutex> guard(lock);
        if (newConsole == nullptr) {
            return;
        }
        console = newConsole;
        lines.swap(held);
    }
    for (const auto &line : lines) {
        newConsole->WriteLine(UnicodeHelper::utf8to32(line));
    }
}

std::vector<std::string> StartupNotices::Held() const {
    std::lock_guard<std::mutex> guard(lock);
    return held;
}
