//
// A C/C++ type as written (docs/code-index.md §11.11): 'std::vector<Foo::Ref> &'. One parser for the structure pass,
// which writes a symbol's type with it, and the resolver, which reads it back. The ownership idioms live here too, as
// 'holds' and '.' / '->' ask the same question: what does this type lead to.
//

#ifndef GOATEDIT_TYPEREF_H
#define GOATEDIT_TYPEREF_H

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "FileStructure.h"

namespace gedit {
    struct TypeRef {
        struct Token {
            std::u32string text;
            bool isBuiltin = false;     // int, void, auto, ... - from the lexer
        };

        // name [ '<' args '>' ] { '*' | '&' | '&&' | const }, from 'idx' on - leading qualifiers ('const', 'static') skipped
        static TypeRef Parse(const std::vector<Token> &tokens, size_t &idx);
        // Reads back what Spelling() wrote
        static TypeRef FromSpelling(const std::u32string &spelling);

        // One canonical spelling, no spaces or qualifiers: 'std::vector<Line::Ref>', 'Document*', 'Foo&'
        std::u32string Spelling() const;
        // The type '.' and '->' reach: 'T*', 'shared_ptr<T>', 'unique_ptr<T>', 'T::Ref', 'T::Ptr' -> 'T'. Anything else is itself.
        TypeRef Accessed() const;
        // The type '[i]' reaches: a container's element (a map's value), a pointer's target. None for anything else.
        std::optional<TypeRef> Element() const;
        // What 'for (auto x : <this>)' gives: the element - but a map gives pairs, which aren't known, so none
        std::optional<TypeRef> RangeElement() const;
        // What a member of this type means for the class holding it: the held type and how. Empty: nothing of this code base.
        std::pair<std::u32string, kOwnership> Held(bool isExtraPointer) const;

        std::u32string name;
        std::vector<TypeRef> args;
        int numPointers = 0;        // '*'
        bool isReference = false;   // '&' or '&&'
        bool isBuiltin = false;
    };
}

#endif //GOATEDIT_TYPEREF_H
