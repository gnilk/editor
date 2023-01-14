//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_LINE_H
#define EDITOR_LINE_H

#include <vector>
#include <string_view>
#include <string>

#define MAX_LINE_LENGTH 1024

class Line {
        public:
        Line();
        Line(const char *data);
        void Clear();
        void Append(int ch);
        void Append(std::string_view &srcdata);
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

        const size_t Length() const { return buffer.size(); }
        const std::string_view Buffer() const { return buffer.c_str(); }
        private:
        std::string buffer = "";
        bool active = false;
        int indent = 0;
};

typedef std::vector<Line *> Buffer;


#endif //EDITOR_LINE_H
