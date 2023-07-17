//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_LINE_H
#define EDITOR_LINE_H

#include <vector>
#include <string_view>
#include <string>
#include <mutex>
#include <memory>
#include <functional>

#include "Core/TextAttributes.h"
#include "Core/Language/LanguageTokenClass.h"

#ifndef GEDIT_MAX_LINE_LENGTH
#define GEDIT_MAX_LINE_LENGTH 1024
#endif

namespace gedit {
    class Line {
    public:
        struct LineAttrib {
            int idxOrigString;   // index in original string...
            gedit::kTextAttributes textAttributes = gedit::kTextAttributes::kNormal;
            kLanguageTokenClass tokenClass; // this one is for better (more formal) analysis when computing indent and similar
        };
        using LineAttribIterator = std::vector<LineAttrib>::iterator;
        using OnChangeDelegate = std::function<void(const Line &)>;
        using Ref = std::shared_ptr<Line>;
    public:
        Line();
        Line(const char *data);
        static Line::Ref Create(const char *data);

        void SetOnChangeDelegate(OnChangeDelegate newOnChangeDelegate) {
            cbChanged = newOnChangeDelegate;
        }

        void Clear();
        void Append(int ch);
        void Append(std::string_view &srcdata);
        void Append(std::string &srcdata);
        void Append(const std::string &srcdata);
        void Append(const char *srcdata);
        void Append(Line::Ref other);

        void Insert(int at, int ch);
        void Insert(int at, const std::string_view &srcdata);
        int Insert(int at, int n, int ch);

        void Delete(int at);
        void Move(Line::Ref dst, int dstOfs, int srcOfs, int nChar = -1);
        void Delete(int at, int n);
        int Unindent();
        void SetIndent(int newIndent) { indent = newIndent; }
        int Indent() { return indent; }
//        int ComputeIndent();

        LineAttribIterator AttributeAt(int pos);
        std::vector<LineAttrib> &Attributes() { return attribs; }

        void SetSelected(bool bSelected) {
            selected = bSelected;
        }
        bool IsSelected() {
            return selected;
        }

        bool StartsWith(const std::string &prefix);
        bool StartsWith(const std::string_view &prefix);

        size_t Length() const { return buffer.size(); }
        const std::string_view Buffer() const { return buffer.c_str(); }

        void Lock();
        void Release();


        int GetStateStackDepth() {
            return stateDepthAtStart;
        }
        void SetStateStackDepth(int newStateDepth) {
            stateDepthAtStart = newStateDepth;
        }
    private:
        void NotifyChangeHandler();

    private:
        std::mutex lock;
        bool isLocked = false;
        std::string buffer = "";
        std::vector<LineAttrib> attribs;
        //bool active = false;
        int indent = 0;
        bool selected = false;
        OnChangeDelegate cbChanged = nullptr;
    private:
        // This tells us how deeply nested the language state stack is at this point
        // It is set when reparsing the whole file
        // It is used to parse the smallest region of a file
        // Search the first line backwards 'til the stateDepth == 1 and start parsing from that line
        int stateDepthAtStart = 0;
    };
}


#endif //EDITOR_LINE_H
