//
// Source -> FileStructure for one language (docs/code-index.md §4.2). A scanner has state while it runs - use
// one per thread.
//
// Two ways in: 'Scan(text)' lexes a whole file (the indexer, gedit-bench), and the line-fed scan reads the
// tokens the syntax highlighter already left on a buffer's lines - no text copy, no second lex - so a buffer
// can rescan just the lines around an edit, resuming from the state stored on a line (LineStructure).
//

#ifndef GOATEDIT_STRUCTURESCANNER_H
#define GOATEDIT_STRUCTURESCANNER_H

#include <memory>
#include <string>

#include "FileStructure.h"
#include "LineStructure.h"
#include "Core/Line.h"

namespace gedit {
    class LanguageBase;

    class StructureScanner {
    public:
        using Ptr = std::unique_ptr<StructureScanner>;
    public:
        virtual ~StructureScanner() = default;
        // The scanner for a language (with a copy of its tokenizer), nullptr when there is none
        static Ptr CreateForLanguage(LanguageBase &language);

        virtual FileStructure Scan(const std::u32string &text) = 0;

        // Line-fed scan: Begin, then per line StateAtLineStart + ScanLine, then End. 'state' nullptr: the
        // start of the file. StateAtLineStart is nullptr mid-statement - a scan can't resume there.
        virtual void Begin(const StructureScanState::Ref &state) = 0;
        virtual StructureScanState::Ref StateAtLineStart() = 0;
        virtual void ScanLine(const Line &line, uint32_t idxLine) = 0;
        virtual FileStructure End() = 0;
    };
}

#endif //GOATEDIT_STRUCTURESCANNER_H
