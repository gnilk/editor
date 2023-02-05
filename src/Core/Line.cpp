//
// Created by gnilk on 14.01.23.
//

#include <string_view>
#include <string>

#include "Core/EditorConfig.h"
#include "Core/StrUtil.h"
#include "Core/Line.h"


Line::Line() {

}

Line::Line(const char *data) {
    buffer = data;
    strutil::rtrim(buffer);
}

void Line::Clear() {
    buffer = "";
}

void Line::Append(int ch) {
    std::lock_guard<std::mutex> guard(lock);
    buffer += ch;
}

void Line::Append(std::string_view &srcdata) {
    std::lock_guard<std::mutex> guard(lock);
    buffer += srcdata;
}

void Line::Append(std::string &srcdata) {
    std::lock_guard<std::mutex> guard(lock);
    buffer += srcdata;
}

void Line::Append(const std::string &srcdata) {
    Append(srcdata.c_str());
}

void Line::Append(const char *srcdata) {
    std::lock_guard<std::mutex> guard(lock);
    buffer += srcdata;
}

void Line::Insert(int at, int ch) {
    std::lock_guard<std::mutex> guard(lock);
    buffer.insert(at, 1, ch);
}
int Line::Insert(int at, int n, int ch) {
    std::lock_guard<std::mutex> guard(lock);
    buffer.insert(at, n, ch);
    return n;
}
void Line::Move(Line *dst, int dstOfs, int srcOfs, int nChar) {
    std::lock_guard<std::mutex> guard(lock);

    if (nChar == -1) {
        nChar = buffer.size() - srcOfs;
        if (nChar < 0) {
            return;
        }
    }
    // nChar now holds the length from srcOfs
    std::string str(buffer, srcOfs, nChar);
    dst->Append(str.c_str());
    buffer.erase(srcOfs, nChar);
}

void Line::Delete(int at) {
    std::lock_guard<std::mutex> guard(lock);
    buffer.erase(at, 1);
}
void Line::Delete(int at, int n) {
    std::lock_guard<std::mutex> guard(lock);
    buffer.erase(at, n);
}
int Line::Unindent() {
    std::lock_guard<std::mutex> guard(lock);
    if (buffer.size() == 0) {
        return 0;
    }
    static const std::string& chars = "\t\n\v\f\r ";
    auto maxLen = buffer.find_first_not_of(chars);
    if (maxLen == std::string::npos) {
        return 0;
    }
    if (maxLen > EditorConfig::Instance().tabSize) {
        maxLen = EditorConfig::Instance().tabSize;
    }
    buffer.erase(0, maxLen);
    return maxLen;
}

//
// TODO: here we should have proper language support!!
//
int Line::ComputeIndent() {
    std::lock_guard<std::mutex> guard(lock);
    static const std::string& chars = "\t\n\v\f\r ";
    if (buffer.size() == 0) {
        return 0;
    }
    auto lastPos = buffer.find_last_not_of(chars);
    // Next line should be indented
    if ((lastPos != std::string::npos) && ('{' == buffer[lastPos])) {
        // Is this 'good enough'
        return EditorConfig::Instance().tabSize + indent;
    }

    auto pos = buffer.find_first_not_of(chars);
    // String is empty - no need
    if (pos == std::string::npos) {
        return 0;
    }
    return pos;
}
