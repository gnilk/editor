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
#define GEDIT_MAX_LINE_LENGTH 65536
#endif

namespace gedit {
    //
    // The line is the lowest level!
    // all underlying data is Unicode U32 chars
    //
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

        using LineIteratorDelegate = std::function<void(const Line::LineAttribIterator &itAttrib, std::u32string &strOut)>;
    public:
        Line();
        Line(const std::u32string &data);
        static Line::Ref Create();
        static Line::Ref Create(const std::u32string &data);

        void SetOnChangeDelegate(OnChangeDelegate newOnChangeDelegate) {
            cbChanged = newOnChangeDelegate;
        }

        void Clear();
        void Append(char32_t ch);
        void Append(const std::u32string &srcdata);
        void Append(const std::u32string_view &srcdata);
        void Append(Line::Ref other);

        void Insert(int at, char32_t ch);
        void Insert(int at, const std::u32string_view &srcdata);
        int Insert(int at, int n, char32_t ch);

        void Delete(int at);
        void Move(Line::Ref dst, int dstOfs, int srcOfs, int nChar = -1);
        void Delete(int at, int n);
        int Unindent(size_t tabSize);
        int Indent(size_t tabSize);
        __inline void SetIndent(int newIndent) { indent = newIndent; }
        __inline int GetIndent() { return indent; }

        LineAttribIterator AttributeAt(int pos);
        std::vector<LineAttrib> &Attributes() { return attribs; }

        void SetSelected(bool bSelected) {
            selected = bSelected;
        }
        bool IsSelected() {
            return selected;
        }

        bool StartsWith(const std::u32string &prefix);

        size_t Length() const { return buffer.length(); }

        const std::u32string_view BufferView() const { return buffer.c_str(); }
        const std::u32string &Buffer() const { return buffer; }
        const std::string BufferAsUTF8() const;

        void Lock();
        void Release();
        __inline bool IsLocked() { return isLocked; }


        char First() {
            return buffer.front();
        }
        char32_t Last() {
            return buffer.back();
        }

        int GetStateStackDepth() {
            return stateDepthAtStart;
        }
        void SetStateStackDepth(int newStateDepth) {
            stateDepthAtStart = newStateDepth;
        }

        void IterateWithAttributes(LineIteratorDelegate callback);

    private:
        void NotifyChangeHandler();

    private:
        std::mutex lock;
        bool isLocked = false;
        std::u32string buffer = U"";
        //std::string buffer = "";
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
