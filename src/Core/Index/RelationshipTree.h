//
// What the relationship views show (docs/code-index.md §5.5, §11.10): the rows of a tree over the index's relations.
// Plain data, no UI, so a view, a command or an agent gets the same answer. One level at a time: a tree asks again per row
// it opens, which is what keeps a cycle in composition from being a loop.
//

#ifndef GOATEDIT_RELATIONSHIPTREE_H
#define GOATEDIT_RELATIONSHIPTREE_H

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

#include "CodeIndex.h"

namespace gedit {
    class RelationshipTree {
    public:
        // The text of a line, as the editor has it - empty if there is none
        using LineSource = std::function<std::u32string(const std::string &path, uint32_t line)>;

        struct Place {
            std::string path;
            uint32_t line = 0;
        };

        struct Row {
            std::u32string text;                // a qualified type, a group's title, or a target as written
            // What opening the row leads to. One: that type's relations. Several: the name is ambiguous, one row each.
            std::vector<std::u32string> types;
            kOwnership ownership = kOwnership::kNone;
            std::string path;                   // empty: nowhere to jump to
            uint32_t line = 0;
            uint32_t column = 0;
            std::vector<Place> places;          // every line the relation is written on - the row jumps to the first
            bool isSource = false;              // a line of code: 'Workspace.h:764  Document::Ref activeDocument'
            bool isOutside = false;             // not in the index
            bool isMacro = false;               // '#define _u8 unsigned char'
            bool isRepeat = false;              // already open further up this branch

            bool operator==(const Row &other) const {
                return (text == other.text) && (types == other.types) && (path == other.path) && (line == other.line);
            }
        };

        struct Group {
            Row title;
            std::vector<Row> rows;
        };
    public:
        // The types to show for a place in the code: the name if it is one, else the type around the line
        static std::vector<std::u32string> TypesAt(const CodeIndex &index, const std::u32string &name, const std::string &path, uint32_t line);
        static Row TypeRow(const CodeIndex &index, const std::u32string &type);
        // A type's relations, empty groups left out. Rows naming one of 'ancestors' are marked, not opened again.
        static std::vector<Group> RelationsOf(const CodeIndex &index, const std::u32string &type, const std::set<std::u32string> &ancestors);
        // Where a relation is written, to read it where it stands. 'lineText' as it is in the file.
        static Row SourceRow(const Place &place, const std::u32string &lineText);
        // What the row shows, without its location
        static std::u32string Label(const Row &row);
    };
}

#endif //GOATEDIT_RELATIONSHIPTREE_H
