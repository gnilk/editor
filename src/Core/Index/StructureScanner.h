//
// Source text -> FileStructure for one language (docs/code-index.md §4.2). A scanner has state while it
// runs - use one per thread.
//

#ifndef GOATEDIT_STRUCTURESCANNER_H
#define GOATEDIT_STRUCTURESCANNER_H

#include <memory>
#include <string>

#include "FileStructure.h"

namespace gedit {
    class StructureScanner {
    public:
        using Ptr = std::unique_ptr<StructureScanner>;
    public:
        virtual ~StructureScanner() = default;
        virtual FileStructure Scan(const std::u32string &text) = 0;
    };
}

#endif //GOATEDIT_STRUCTURESCANNER_H
