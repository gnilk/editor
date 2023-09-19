//
// Created by gnilk on 14.01.23.
//

#include <string_view>
#include <string>

#include "Core/StrUtil.h"
#include "Core/Line.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

Line::Line() {

}

Line::Line(const std::u32string &data) {
    buffer = data;
    strutil::rtrim(buffer);
}


Line::Ref Line::Create() {
    return std::make_shared<Line>();
}

Line::Ref Line::Create(const std::u32string &data) {
    return std::make_shared<Line>(data);
}


void Line::Lock() {
    lock.lock();
    isLocked = true;
}
void Line::Release() {
    lock.unlock();
    isLocked = false;
}

void Line::NotifyChangeHandler() {
    if (cbChanged == nullptr) {
        return;
    }
   cbChanged(*this);
}

void Line::Clear() {
    buffer = U"";
}

void Line::Append(char32_t ch) {
    {
        std::lock_guard<std::mutex> guard(lock);
        buffer += ch;
    }
    NotifyChangeHandler();
}

void Line::Append(const std::u32string &srcdata) {
    {
        std::lock_guard<std::mutex> guard(lock);
        buffer += srcdata;
    }
    NotifyChangeHandler();
}

void Line::Append(const std::u32string_view &srcdata) {
    {
        std::lock_guard<std::mutex> guard(lock);
        buffer += srcdata;

    }
    NotifyChangeHandler();
}


void Line::Append(Line::Ref other) {
    Append(other->Buffer());
}


void Line::Insert(int at, char32_t ch) {
    {
        std::lock_guard<std::mutex> guard(lock);
        buffer.insert(at, 1, ch);
    }
    NotifyChangeHandler();
}

int Line::Insert(int at, int n, char32_t ch) {
    {
        std::lock_guard<std::mutex> guard(lock);
        buffer.insert(at, n, ch);
    }
    NotifyChangeHandler();
    return n;
}

void Line::Insert(int at, const std::u32string_view &srcdata) {
    {
        std::lock_guard<std::mutex> guard(lock);
        buffer.insert(at, srcdata);
    }
    NotifyChangeHandler();
}

void Line::Move(Line::Ref dst, int dstOfs, int srcOfs, int nChar) {
    {
        std::lock_guard<std::mutex> guard(lock);

        if (nChar == -1) {
            nChar = buffer.size() - srcOfs;
            if (nChar < 0) {
                return;
            }
        }
        // nChar now holds the length from srcOfs
        std::u32string str(buffer, srcOfs, nChar);
        dst->Append(str);
        buffer.erase(srcOfs, nChar);
    }
    NotifyChangeHandler();
}

void Line::Delete(int at) {
    {
        std::lock_guard<std::mutex> guard(lock);
        buffer.erase(at, 1);
    }
    NotifyChangeHandler();
}

void Line::Delete(int at, int n) {
    {
        std::lock_guard<std::mutex> guard(lock);
        buffer.erase(at, n);
    }
    NotifyChangeHandler();
}

int Line::Unindent(size_t tabSize) {
    size_t maxLen = 0;
    {
        std::lock_guard<std::mutex> guard(lock);
        if (buffer.size() == 0) {
            return 0;
        }
        static const std::u32string &chars = U"\t\n\v\f\r ";
        maxLen = buffer.find_first_not_of(chars);
        if (maxLen == std::string::npos) {
            return 0;
        }
        if (maxLen > tabSize) {
            maxLen = tabSize;
        }
        buffer.erase(0, maxLen);
    }
    NotifyChangeHandler();
    return maxLen;
}

Line::LineAttribIterator Line::AttributeAt(int pos) {
    for(size_t i=0;i<attribs.size()-1;i++) {
        if ((pos >= attribs[i].idxOrigString) && (pos < attribs[i+1].idxOrigString)) {
            return attribs.begin()+i;
        }
    }
    // Not sure...
    return attribs.begin();
}

bool Line::StartsWith(const std::u32string &prefix) {
    std::lock_guard<std::mutex> guard(lock);
    return strutil::startsWith(buffer, prefix);
}

const std::string Line::BufferAsUTF8() const {
    return UnicodeHelper::utf32to8(buffer);
}


