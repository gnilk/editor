//
// The structure of C/C++ files as text: one line per symbol (indented by nesting), then the edges - what
// 'goatedit --dump' and 'gedit-bench --dump' print, and what the shipped typediff tool reads (agent-integration.md §6.4).
// The first piece of the index export (hitlist #74).
//

#ifndef GEDIT_STRUCTUREDUMP_H
#define GEDIT_STRUCTUREDUMP_H

#include <cstdio>
#include <filesystem>
#include <vector>

#include "Core/Language/LangLineTokenizer.h"

namespace gedit {
    class StructureDump {
    public:
        // 'root' is a file or a folder (its C/C++ files, sorted)
        static void Print(FILE *out, const std::filesystem::path &root, const LangLineTokenizer &tokenizer);
        static std::vector<std::filesystem::path> SourceFiles(const std::filesystem::path &root);
        static bool IsSource(const std::filesystem::path &path);
    };
}

#endif //GEDIT_STRUCTUREDUMP_H
