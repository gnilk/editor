//
// A place written in text - see TextPlace.h
//

#include <regex>

#include "TextPlace.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

std::optional<TextPlace> TextPlace::At(const std::u32string &text, size_t column) {
    if (column >= text.size()) {
        return std::nullopt;
    }
    auto place = PythonFrameAt(text, column);
    if (place.has_value()) {
        return place;
    }
    return PathAt(text, column);
}

// '  File "/work/main.py", line 5, in <module>' - anywhere from 'File' to the line's number
std::optional<TextPlace> TextPlace::PythonFrameAt(const std::u32string &text, size_t column) {
    static const std::regex frame(R"re(File "([^"]+)", line (\d{1,9}))re");
    auto text8 = UnicodeHelper::utf32to8(text);
    std::smatch match;
    if (!std::regex_search(text8, match, frame)) {
        return std::nullopt;
    }
    // The regex counts bytes, the column counts characters
    auto start = UnicodeHelper::utf8to32(text8.substr(0, (size_t)match.position(0))).size();
    auto end = start + UnicodeHelper::utf8to32(match.str(0)).size();
    if ((column < start) || (column >= end)) {
        return std::nullopt;
    }
    TextPlace place;
    place.path = match.str(1);
    place.line = (uint32_t)std::stoul(match.str(2));
    return place;
}

// The word under the column - up to a blank, a quote or a bracket - read as 'path[:line[:column]]', with what the tools
// put after it (':', ',', '.') left off
std::optional<TextPlace> TextPlace::PathAt(const std::u32string &text, size_t column) {
    static const std::u32string stops = U" \t\"'`()[]{}<>|";
    if (stops.find(text[column]) != std::u32string::npos) {
        return std::nullopt;
    }
    auto start = text.find_last_of(stops, column);
    start = (start == std::u32string::npos) ? 0 : start + 1;
    auto end = text.find_first_of(stops, column);
    end = (end == std::u32string::npos) ? text.size() : end;
    auto word = UnicodeHelper::utf32to8(text.substr(start, end - start));
    while (!word.empty() && ((word.back() == ':') || (word.back() == ',') || (word.back() == '.') || (word.back() == ';'))) {
        word.pop_back();
    }
    // 'a.cpp:3:16: error' was cut at the blank - what is left after the numbers is the tool's ('a.cpp:3:16:warning')
    static const std::regex withLine(R"(^(.+?):(\d{1,9})(?::(\d{1,9}))?(?::.*)?$)");
    std::smatch match;
    TextPlace place;
    if (std::regex_match(word, match, withLine)) {
        place.path = match.str(1);
        place.line = (uint32_t)std::stoul(match.str(2));
        place.column = match[3].matched ? (uint32_t)std::stoul(match.str(3)) : 0;
    } else {
        place.path = word;
    }
    if (place.path.empty()) {
        return std::nullopt;
    }
    return place;
}
