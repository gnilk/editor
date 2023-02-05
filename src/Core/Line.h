//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_LINE_H
#define EDITOR_LINE_H

#include <vector>
#include <string_view>
#include <string>
#include <mutex>

#define MAX_LINE_LENGTH 1024


class Line {
public:
    struct LineAttrib {
        int idxOrigString;   // index in original string...
        // Attributes from this cursor position and onwards..
        int idxColor;       // index to color (or token classification)
    };
public:
    Line();
    Line(const char *data);
    void Clear();
    void Append(int ch);
    void Append(std::string_view &srcdata);
    void Append(std::string &srcdata);
    void Append(const std::string &srcdata);
    void Append(const char *srcdata);
    void Insert(int at, int ch);
    int Insert(int at, int n, int ch);
    void Delete(int at);
    void Move(Line *dst, int dstOfs, int srcOfs, int nChar = -1);
    void Delete(int at, int n);
    int Unindent();
    bool IsActive() { return active; }
    void SetActive(bool isActive) { active = isActive; }
    void SetIndent(int newIndent) { indent = newIndent; }
    int Indent() { return indent; }
    int ComputeIndent();

    std::vector<LineAttrib> &Attributes() { return attribs; }

    void SetSelected(bool bSelected) {
        selected = bSelected;
    }
    bool IsSelected() {
        return selected;
    }

    const size_t Length() const { return buffer.size(); }
    const std::string_view Buffer() const { return buffer.c_str(); }
private:
    std::mutex lock;
    std::string buffer = "";
    std::vector<LineAttrib> attribs;
    bool active = false;
    int indent = 0;
    bool selected = false;

    // TEST
public:
    std::string startState = "";
    std::string endState = "";
};

//typedef std::vector<Line *> Buffer;


#endif //EDITOR_LINE_H
