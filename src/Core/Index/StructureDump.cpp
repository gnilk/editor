//
// See StructureDump.h
//

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "StructureDump.h"
#include "CPPStructureScanner.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;
namespace fs = std::filesystem;

static std::string ReadFile(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream data;
    data << file.rdbuf();
    return data.str();
}

static std::string Utf8(const std::u32string &text) {
    return UnicodeHelper::utf32toascii(text);
}

void StructureDump::Print(FILE *out, const fs::path &root, const LangLineTokenizer &tokenizer) {
    CPPStructureScanner scanner(tokenizer);
    for (auto &path : SourceFiles(root)) {
        auto structure = scanner.Scan(UnicodeHelper::utf8to32(ReadFile(path)));
        fprintf(out, "== %s (%zu lines, %zu symbols, %zu edges)\n", path.string().c_str(), structure.numLines, structure.symbols.size(), structure.edges.size());
        for (auto &symbol : structure.symbols) {
            int depth = 0;
            for (auto idx = symbol.idxParent; idx >= 0; idx = structure.symbols[idx].idxParent) {
                depth++;
            }
            std::string extent = (symbol.endLine != symbol.beginLine) ? " " + std::to_string(symbol.beginLine + 1) + "-" + std::to_string(symbol.endLine + 1) : "";
            std::string type = symbol.type.empty() ? "" : "  : " + Utf8(symbol.type);
            fprintf(out, "  %5u %*s%-10s %s%s%s%s\n", symbol.line + 1, depth * 2, "", SymbolKindName(symbol.kind),
                    Utf8(symbol.qualifiedName).c_str(), type.c_str(), symbol.isDefinition ? "" : "  [decl]", extent.c_str());
        }
        for (auto &edge : structure.edges) {
            auto from = edge.fromName.empty() ? std::string("<file>") : Utf8(edge.fromName);
            std::string how = (edge.ownership != kOwnership::kNone) ? std::string("(") + OwnershipName(edge.ownership) + ")" : (edge.isSystemInclude ? "<sys>" : "");
            fprintf(out, "  %5u %s %s%s %s\n", edge.line + 1, from.c_str(), EdgeKindName(edge.kind), how.c_str(), Utf8(edge.toName).c_str());
        }
    }
}

std::vector<fs::path> StructureDump::SourceFiles(const fs::path &root) {
    std::vector<fs::path> paths;
    if (fs::is_regular_file(root)) {
        paths.push_back(root);
    } else if (fs::is_directory(root)) {
        for (auto &entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file() && IsSource(entry.path())) {
                paths.push_back(entry.path());
            }
        }
        std::sort(paths.begin(), paths.end());
    }
    return paths;
}

bool StructureDump::IsSource(const fs::path &path) {
    auto ext = path.extension().string();
    return (ext == ".c") || (ext == ".cpp") || (ext == ".cc") || (ext == ".h") || (ext == ".hpp");
}
