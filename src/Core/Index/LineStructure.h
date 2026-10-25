//
// What the structure pass leaves on a Line (docs/code-index.md §10.1): the scanner's state at the line's
// start - where a later scan can resume, like the tokenizer's 'stateDepthAtStart' - and the names the line
// declares. The line owns it but doesn't look into it.
//

#ifndef GOATEDIT_LINESTRUCTURE_H
#define GOATEDIT_LINESTRUCTURE_H

#include <memory>
#include <string>
#include <vector>

#include "Core/Language/LanguageTokenClass.h"

namespace gedit {
    // A scanner's resumable state - opaque outside the scanner. Immutable, shared by consecutive lines with
    // the same state.
    class StructureScanState {
    public:
        using Ref = std::shared_ptr<const StructureScanState>;
    public:
        virtual ~StructureScanState() = default;
        virtual bool Equals(const StructureScanState &other) const = 0;
    };

    struct SemanticDeclaration {
        std::u32string name;
        kLanguageTokenClass tokenClass = kLanguageTokenClass::kRegular;

        bool operator==(const SemanticDeclaration &other) const {
            return (name == other.name) && (tokenClass == other.tokenClass);
        }
    };

    class LineStructure {
    public:
        using Ref = std::shared_ptr<LineStructure>;
    public:
        StructureScanState::Ref stateAtStart = nullptr;     // nullptr: the line starts mid-statement - no scan starts here
        std::vector<SemanticDeclaration> declarations;
    };
}

#endif //GOATEDIT_LINESTRUCTURE_H
