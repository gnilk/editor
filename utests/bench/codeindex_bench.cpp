//
// Code index lexer throughput (docs/code-index.md §11 step 0). A plain executable, not a trun case: trun
// links the ASan runtime, which makes every allocation expensive and skews the numbers badly.
//
// Usage (from the build dir - the editor resolves its assets relative to the working directory):
//   ./gedit-bench [path] [path...]         a file or a directory (C/C++ sources under it); default ../src
//   ./gedit-bench --dump [path] [path...]  print the structure (symbols + edges) of each file - step 1
//   ./gedit-bench --check [path] [path...] list files whose braces don't balance - usually a lexer misread
//   ./gedit-bench --braces <file>          brace depth after each line as the lexer sees it ('line depth') -
//                                          diff it against a reference count to find the misread line
//
// Files are read before timing. Per corpus it prints lines/s for the editor path (TextBuffer + Reparse),
// the fast path (LangLineTokenizer::TokenizeText) on one thread and on N threads, and the structure pass
// (CPPStructureScanner) on one thread.
//
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "Core/Editor.h"
#include "Core/Config/Config.h"
#include "Core/TextBuffer.h"
#include "Core/UnicodeHelper.h"
#include "Core/Language/LangLineTokenizer.h"
#include "Core/FileSystem/ExclusionRules.h"
#include "Core/Index/CodeIndex.h"
#include "Core/Index/CPPStructureScanner.h"
#include "Core/Index/Indexer.h"
#include "Core/Index/SemanticNames.h"
#include "Core/Index/GoToResolver.h"
#include "Core/Index/LocalScanner.h"

using namespace gedit;
namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

static std::string ReadFile(const fs::path &path);
static void Bench(const fs::path &root);
static void Dump(const fs::path &root);
static void Check(const fs::path &root);
static void Braces(const fs::path &root);
static void IndexBench(const fs::path &root);
static void Ignored(const fs::path &root);
static void Recolour(const fs::path &file, const fs::path &root);
static void Edges(const std::string &name, const fs::path &root);
static void Chains(const fs::path &root);
static std::string Utf8(const std::u32string &text);
static std::vector<fs::path> SourceFiles(const fs::path &root);

int main(int argc, const char **argv) {
    static const char *editorArgv[] = {"gedit-bench", "--skip-user-config", "--backend", "headless"};
    Editor::Instance().Initialize(4, editorArgv);

    std::string mode = (argc > 1) ? argv[1] : "";
    if ((mode == "--recolour") && (argc > 3)) {
        Recolour(argv[2], argv[3]);
        return 0;
    }
    if ((mode == "--edges") && (argc > 3)) {
        Edges(argv[2], argv[3]);
        return 0;
    }
    auto run = Bench;
    if (mode == "--dump") {
        run = Dump;
    } else if (mode == "--check") {
        run = Check;
    } else if (mode == "--braces") {
        run = Braces;
    } else if (mode == "--index") {
        run = IndexBench;
    } else if (mode == "--ignored") {
        run = Ignored;
    } else if (mode == "--chains") {
        run = Chains;
    }
    int idxFirstPath = (run == Bench) ? 1 : 2;
    if (argc <= idxFirstPath) {
        run("../src");
    }
    for (int i = idxFirstPath; i < argc; i++) {
        run(argv[i]);
    }
    return 0;
}

// Every file below a root that ExclusionRules ignores, one per line relative to the root - to hold against
// 'git check-ignore --no-index --stdin' on a real repository
static void Ignored(const fs::path &root) {
    auto absoluteRoot = fs::canonical(root);
    auto rules = ExclusionRules::Create(absoluteRoot, {".git"}, true, ExclusionRules::DefaultGlobalIgnoreFile());
    for (auto it = fs::recursive_directory_iterator(absoluteRoot, fs::directory_options::skip_permission_denied); it != fs::recursive_directory_iterator(); ++it) {
        if (it->path().filename() == ".git") {
            it.disable_recursion_pending();
            continue;
        }
        bool isDirectory = it->is_directory() && !it->is_symlink();
        auto verdict = rules->Check(it->path(), isDirectory);
        printf("%c %s%s\n", (verdict == ExclusionRules::kVerdict::kIgnored) ? 'I' : '.', it->path().lexically_relative(absoluteRoot).c_str(), isDirectory ? "/" : "");
    }
}

// What cross-file colouring costs a large open file (§11.6): 'file' as a document, the names from 'root'. On the
// calling thread, so the times are the buffer's own.
static void Recolour(const fs::path &file, const fs::path &root) {
    auto mainConfig = Config::Instance()["main"];
    mainConfig.SetBool("threaded_syntaxparser", false);
    auto language = Editor::Instance().GetLanguageForExtension(".cpp");

    Indexer::Languages languages;
    for (auto &extension : {".cpp", ".h", ".c", ".hpp"}) {
        languages[extension] = language;
    }
    auto index = CodeIndex::Create();
    auto indexer = Indexer::Create(index, languages, Indexer::Options{});
    indexer->AddRoot(fs::canonical(root));
    indexer->WaitIdle();
    indexer->Stop();

    auto timed = [](const char *what, const std::function<void()> &work) {
        auto timeStart = std::chrono::steady_clock::now();
        work();
        printf("  %-44s %8.1f ms\n", what, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - timeStart).count());
    };

    auto names = index->GetWorkspaceNames();
    auto changed = std::make_shared<WorkspaceNames>(*names);
    changed->Add(U"NoSuchNameAnywhere", kLanguageTokenClass::kUserType);

    printf("%s: %zu workspace names from %s\n", file.c_str(), names->Size(), root.c_str());
    timed("load library lists", []() {
        LibraryNames::Load(std::nullopt);
    });
    printf("  %zu library names\n", Editor::Instance().GetLibraryNames()->Size());
    auto buffer = TextBuffer::CreateEmptyBuffer();
    buffer->DeleteLineAt(0);
    timed("load lines", [&]() {
        std::ifstream stream(file);
        std::string line;
        while (std::getline(stream, line)) {
            buffer->AddLineUTF8(line.c_str());
        }
    });
    printf("  %zu lines\n", buffer->NumLines());
    timed("first full parse, no workspace names", [&]() { buffer->SetLanguage(language); });
    // Proof that a recolour did something - a fast one that changed nothing proves nothing
    auto colouredTokens = [&buffer]() {
        size_t numColoured = 0;
        for (size_t idxLine = 0; idxLine < buffer->NumLines(); idxLine++) {
            for (auto &attrib : buffer->LineAt(idxLine)->Attributes()) {
                numColoured += SemanticNames::IsSemanticClass(attrib.tokenClass) ? 1 : 0;
            }
        }
        return numColoured;
    };
    printf("  %-44s %8zu\n", "  coloured tokens, the file's own names", colouredTokens());
    WorkspaceNames::Ref current = names;
    buffer->SetWorkspaceNamesSource([&current]() -> WorkspaceNames::Ref { return current; });
    current = nullptr;
    timed("full parse, names kept", [&]() { buffer->Reparse(); });
    for (auto &[what, next] : std::vector<std::pair<const char *, WorkspaceNames::Ref>>{
            {"recolour: one name added", changed}, {"recolour: one name removed", names}, {"recolour: names removed", std::make_shared<WorkspaceNames>()}, {"recolour: names back", names}}) {
        current = next;
        timed(what, [&]() { buffer->SetWorkspaceNamesSource([&current]() -> WorkspaceNames::Ref { return current; }); });
        printf("  %-44s %8zu\n", "  coloured tokens", colouredTokens());
    }
    timed("full parse with workspace names", [&]() { buffer->Reparse(); });
    timed("collect structure (each pause in typing)", [&]() { buffer->CollectStructure(); });
    buffer->SetWorkspaceNamesSource(nullptr);
}

// Step 2's targets on real code: how long a tree takes to index, what it costs in memory, and what a keypress
// costs (< 10 ms at 1M symbols, §4.5). Queries run on the finished index, single-threaded.
static CodeIndex::Ref IndexTree(const fs::path &root) {
    auto language = Editor::Instance().GetLanguageForExtension(".cpp");
    Indexer::Languages languages;
    for (auto &extension : {".cpp", ".h", ".c", ".hpp"}) {
        languages[extension] = language;
    }
    auto workspaceConfig = Config::Instance()["workspace"];
    auto absoluteRoot = fs::canonical(root);
    auto rules = ExclusionRules::Create(absoluteRoot, workspaceConfig.GetSequenceOfStr("exclude"), workspaceConfig.GetBool("use_git_ignore", true));

    auto index = CodeIndex::Create();
    auto indexer = Indexer::Create(index, languages, Indexer::Options{});
    indexer->AddRoot(absoluteRoot, rules);
    indexer->WaitIdle();
    return index;
}

// Go to definition on every 'a.b' / 'a->b' of a tree (docs/code-index.md §11.11): what the chain gives, against the same
// name with the receiver unknown - how often the types on symbols narrow it, and to what. Each file is a parsed buffer, so the
// parameters and locals come from its lines as in the editor; the count without them is slice B alone.
static GoToResolver::Locals LocalsAt(const CodeIndex &index, const TextBuffer::Ref &buffer, const std::string &path, uint32_t idxLine, size_t idxColumn) {
    auto function = index.NearestSymbol(path, idxLine);
    if (!function.has_value() || ((function->kind != kSymbolKind::kFunction) && (function->kind != kSymbolKind::kMethod))) {
        return {};
    }
    std::vector<LocalScanner::Token> tokens;
    for (auto i = function->line; i <= idxLine; i++) {
        LocalScanner::AppendLineTokens(*buffer->LineAt(i), i, (i == idxLine) ? idxColumn : SIZE_MAX, tokens);
    }
    return LocalScanner::Scan(tokens);
}

static void Chains(const fs::path &root) {
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);
    auto language = Editor::Instance().GetLanguageForExtension(".cpp");
    auto tStart = std::chrono::steady_clock::now();
    auto index = IndexTree(root);
    auto msIndex = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tStart).count();
    size_t numSameList = 0, numNarrowedWithoutLocals = 0;
    size_t numUses = 0, numNarrowed = 0, numToOne = 0, numLibrary = 0, numSame = 0, numNone = 0;
    std::map<std::u32string, size_t> libraryTypes;
    double msResolve = 0;
    for (auto &file : SourceFiles(root)) {
        auto path = fs::canonical(file).string();
        auto buffer = TextBuffer::CreateEmptyBuffer();
        buffer->DeleteLineAt(0);
        std::ifstream stream(file);
        std::string utf8;
        while (std::getline(stream, utf8)) {
            buffer->AddLineUTF8(utf8.c_str());
        }
        buffer->SetLanguage(language);
        for (uint32_t idxLine = 0; idxLine < buffer->NumLines(); idxLine++) {
            auto line = buffer->LineAt(idxLine)->Buffer();
            auto idxCode = line.find_first_not_of(U" \t");
            bool isCode = (idxCode != std::u32string::npos) && (line.compare(idxCode, 2, U"//") != 0) && (line[idxCode] != U'#') && (line[idxCode] != U'*');
            for (size_t i = 1; isCode && (i + 1 < line.size()); i++) {
                bool isAccess = ((line[i] == U'.') && (line[i - 1] != U'.')) || ((line[i] == U'>') && (line[i - 1] == U'-'));
                auto ch = line[i + 1];
                bool isName = ((ch >= U'a') && (ch <= U'z')) || ((ch >= U'A') && (ch <= U'Z')) || (ch == U'_');
                if (!isAccess || !isName) {
                    continue;
                }
                auto tResolve = std::chrono::steady_clock::now();
                auto locals = LocalsAt(*index, buffer, path, idxLine, i + 1);
                auto result = GoToResolver::Resolve(*index, line, i + 1, idxLine, path, GoToResolver::kMode::kDefinition, locals);
                msResolve += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tResolve).count();
                auto withoutLocals = GoToResolver::Resolve(*index, line, i + 1, idxLine, path, GoToResolver::kMode::kDefinition);
                auto unknownLine = U"x." + line.substr(i + 1);
                auto without = GoToResolver::Resolve(*index, unknownLine, 2, idxLine, path, GoToResolver::kMode::kDefinition);
                numUses++;
                numNarrowedWithoutLocals += (!withoutLocals.hits.empty() && (withoutLocals.hits.size() < without.hits.size())) ? 1 : 0;
                if (!result.libraryType.empty()) {
                    numLibrary++;
                    libraryTypes[result.libraryType]++;
                } else if (result.hits.empty()) {
                    numNone++;
                } else if (result.hits.size() < without.hits.size()) {
                    numNarrowed++;
                    numToOne += (result.hits.size() == 1) ? 1 : 0;
                } else {
                    numSame++;
                    numSameList += (result.hits.size() > 1) ? 1 : 0;
                }
            }
        }
    }
    printf("%s (indexed in %.0f ms)\n", root.c_str(), msIndex);
    printf("  %zu uses of '.' / '->': %zu narrowed (%zu to one place; %zu without locals), %zu on a library type, %zu as before (%zu of them a list), %zu not in the index\n",
           numUses, numNarrowed, numToOne, numNarrowedWithoutLocals, numLibrary, numSame, numSameList, numNone);
    printf("  locals + resolving took %.1f ms, %.1f us a use\n", msResolve, numUses ? (1000.0 * msResolve / (double)numUses) : 0.0);
    std::vector<std::pair<size_t, std::u32string>> sorted;
    for (auto &[type, count] : libraryTypes) {
        sorted.push_back({count, type});
    }
    std::sort(sorted.rbegin(), sorted.rend());
    for (size_t i = 0; i < std::min<size_t>(8, sorted.size()); i++) {
        printf("    %5zu %s\n", sorted[i].first, Utf8(sorted[i].second).c_str());
    }
}

// The relationship queries from the outside, as a view or an agent asks them (docs/code-index.md §11.10)
static void Edges(const std::string &name, const fs::path &root) {
    auto index = IndexTree(root);
    auto printRows = [&root](const char *title, const std::vector<CodeIndex::Relation> &relations, bool isToTarget) {
        printf("  %s (%zu)\n", title, relations.size());
        for (auto &relation : relations) {
            auto row = isToTarget ? Utf8(relation.toName) : Utf8(relation.fromName);
            std::string how = (relation.ownership != kOwnership::kNone) ? std::string(" (") + OwnershipName(relation.ownership) + ")" : "";
            std::string targets;
            if (isToTarget && relation.targets.empty()) {
                targets = " [outside the index]";
            } else if (isToTarget && (relation.targets.size() > 1)) {
                targets = " [ambiguous:";
                for (auto &target : relation.targets) {
                    targets += " " + Utf8(target);
                }
                targets += "]";
            }
            printf("    %s%s%s  %s:%u\n", row.c_str(), how.c_str(), targets.c_str(), fs::path(relation.path).lexically_relative(fs::canonical(root)).c_str(), relation.line + 1);
        }
    };
    auto tStart = std::chrono::steady_clock::now();
    auto types = index->TypesNamed(UnicodeHelper::utf8to32(name));
    for (auto &type : types) {
        printf("%s\n", Utf8(type).c_str());
        printRows("bases", index->BasesOf(type), true);
        printRows("derived", index->DerivedFrom(type), false);
        printRows("holds", index->HoldsOf(type), true);
        printRows("held by", index->HeldBy(type), false);
    }
    auto msQuery = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tStart).count();
    printf("%zu type(s), %zu edges in the index, queries and printing %.2f ms\n", types.size(), index->GetStats().numEdges, msQuery);
}

static void IndexBench(const fs::path &root) {
    auto tStart = std::chrono::steady_clock::now();
    auto index = IndexTree(root);
    auto msIndex = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tStart).count();

    auto stats = index->GetStats();
    printf("%s\n  indexed %zu files in %.0f ms: %zu symbols, %zu edges, %zu names, %zu scopes, %zu types, ~%.1f MB\n", root.c_str(), stats.numFiles, msIndex,
           stats.numSymbols, stats.numEdges, stats.numNames, stats.numScopes, stats.numTypes, (double)stats.numBytes / (1024.0 * 1024.0));

    auto timeRelations = std::chrono::steady_clock::now();
    auto relations = index->AllRelations();
    auto msRelations = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - timeRelations).count();
    size_t numOutside = 0;
    size_t numAmbiguous = 0;
    for (auto &relation : relations) {
        numOutside += relation.targets.empty() ? 1 : 0;
        numAmbiguous += (relation.targets.size() > 1) ? 1 : 0;
    }
    printf("  relations: %zu, %zu resolved, %zu outside the index, %zu ambiguous, all in %.1f ms\n", relations.size(),
           relations.size() - numOutside - numAmbiguous, numOutside, numAmbiguous, msRelations);
    // What falls outside says what the index can't see - a tree that stands out here is worth a look
    std::map<std::string, size_t> outsideNames;
    std::map<std::string, size_t> outsideFolders;
    auto absoluteRoot = fs::canonical(root);
    for (auto &relation : relations) {
        if (relation.targets.empty()) {
            outsideNames[Utf8(relation.toName)]++;
            auto folder = fs::path(relation.path).lexically_relative(absoluteRoot);
            auto it = folder.begin();
            std::string top = it->string();
            if (++it != folder.end()) {
                top += "/" + it->string();
            }
            outsideFolders[top]++;
        }
    }
    auto printTop = [](const char *title, const std::map<std::string, size_t> &counts) {
        std::vector<std::pair<size_t, std::string>> sorted;
        for (auto &[name, count] : counts) {
            sorted.emplace_back(count, name);
        }
        std::sort(sorted.rbegin(), sorted.rend());
        printf("  outside, by %s:", title);
        for (size_t i = 0; i < std::min<size_t>(sorted.size(), 8); i++) {
            printf(" %s %zu", sorted[i].second.c_str(), sorted[i].first);
        }
        printf("\n");
    };
    printTop("name", outsideNames);
    printTop("folder", outsideFolders);

    auto timeNames = std::chrono::steady_clock::now();
    auto workspaceNames = index->GetWorkspaceNames();
    auto msNames = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - timeNames).count();
    printf("  workspace names: %zu with one meaning, %zu not, built in %.2f ms\n", workspaceNames->Size(), stats.numNames - workspaceNames->Size(), msNames);

    static const std::u32string queries[] = {U"s", U"se", U"sea", U"init", U"srch", U"SF", U"Document::Sea", U"get::", U"zqxj"};
    for (auto &query : queries) {
        static constexpr int NUM_RUNS = 20;
        double msWorst = 0;
        double msTotal = 0;
        size_t numHits = 0;
        for (int i = 0; i < NUM_RUNS; i++) {
            auto tQuery = std::chrono::steady_clock::now();
            numHits = index->Query(query).size();
            auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tQuery).count();
            msWorst = std::max(msWorst, ms);
            msTotal += ms;
        }
        printf("  %-16s %3zu hits  avg %6.2f ms  worst %6.2f ms\n", Utf8(query).c_str(), numHits, msTotal / NUM_RUNS, msWorst);
    }
}

static bool IsSource(const fs::path &path) {
    auto ext = path.extension().string();
    return (ext == ".c") || (ext == ".cpp") || (ext == ".cc") || (ext == ".h") || (ext == ".hpp");
}

static std::vector<fs::path> SourceFiles(const fs::path &root) {
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

static std::string Utf8(const std::u32string &text) {
    return UnicodeHelper::utf32toascii(text);
}

// One line per symbol (indented by nesting), then the edges
static void Dump(const fs::path &root) {
    auto language = Editor::Instance().GetLanguageForExtension(".cpp");
    CPPStructureScanner scanner(language->Tokenizer());
    for (auto &path : SourceFiles(root)) {
        auto structure = scanner.Scan(UnicodeHelper::utf8to32(ReadFile(path)));
        printf("== %s (%zu lines, %zu symbols, %zu edges)\n", path.string().c_str(), structure.numLines, structure.symbols.size(), structure.edges.size());
        for (auto &symbol : structure.symbols) {
            int depth = 0;
            for (auto idx = symbol.idxParent; idx >= 0; idx = structure.symbols[idx].idxParent) {
                depth++;
            }
            std::string extent = (symbol.endLine != symbol.beginLine) ? " " + std::to_string(symbol.beginLine + 1) + "-" + std::to_string(symbol.endLine + 1) : "";
            std::string type = symbol.type.empty() ? "" : "  : " + Utf8(symbol.type);
            printf("  %5u %*s%-10s %s%s%s%s\n", symbol.line + 1, depth * 2, "", SymbolKindName(symbol.kind),
                   Utf8(symbol.qualifiedName).c_str(), type.c_str(), symbol.isDefinition ? "" : "  [decl]", extent.c_str());
        }
        for (auto &edge : structure.edges) {
            auto from = edge.fromName.empty() ? std::string("<file>") : Utf8(edge.fromName);
            std::string how = (edge.ownership != kOwnership::kNone) ? std::string("(") + OwnershipName(edge.ownership) + ")" : (edge.isSystemInclude ? "<sys>" : "");
            printf("  %5u %s %s%s %s\n", edge.line + 1, from.c_str(), EdgeKindName(edge.kind), how.c_str(), Utf8(edge.toName).c_str());
        }
    }
}

static void Braces(const fs::path &root) {
    auto tokenizer = Editor::Instance().GetLanguageForExtension(".cpp")->Tokenizer();
    auto text = UnicodeHelper::utf8to32(ReadFile(root));
    std::vector<int> depthAfterLine;
    int depth = 0;
    auto numLines = tokenizer.TokenizeText(text, [&depth, &depthAfterLine](size_t idxLine, const LangToken &token) {
        if (token.classification == kLanguageTokenClass::kCodeBlockStart) {
            depth++;
        } else if (token.classification == kLanguageTokenClass::kCodeBlockEnd) {
            depth--;
        }
        if (depthAfterLine.size() <= idxLine) {
            depthAfterLine.resize(idxLine + 1, INT32_MIN);
        }
        depthAfterLine[idxLine] = depth;
    });
    int last = 0;
    for (size_t i = 0; i < numLines; i++) {
        if ((i < depthAfterLine.size()) && (depthAfterLine[i] != INT32_MIN)) {
            last = depthAfterLine[i];
        }
        printf("%zu %d\n", i + 1, last);
    }
}

// Files whose braces don't balance: still open at the end, or a '}' with nothing open
static void Check(const fs::path &root) {
    auto language = Editor::Instance().GetLanguageForExtension(".cpp");
    CPPStructureScanner scanner(language->Tokenizer());
    auto paths = SourceFiles(root);
    size_t numSuspicious = 0;
    for (auto &path : paths) {
        auto structure = scanner.Scan(UnicodeHelper::utf8to32(ReadFile(path)));
        if ((structure.numUnclosedBraces == 0) && (structure.numStrayCloseBraces == 0)) {
            continue;
        }
        numSuspicious++;
        printf("%s:", path.string().c_str());
        if (structure.numUnclosedBraces > 0) {
            printf(" %u unclosed (outermost opened at line %u)", structure.numUnclosedBraces, structure.lineFirstUnclosed + 1);
        }
        if (structure.numStrayCloseBraces > 0) {
            printf(" %u stray '}' (first at line %u)", structure.numStrayCloseBraces, structure.lineFirstStrayClose + 1);
        }
        printf("\n");
    }
    printf("[check] %s - %zu files, %zu unbalanced\n", root.string().c_str(), paths.size(), numSuspicious);
}

static double MsSince(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

static void Bench(const fs::path &root) {
    if (!fs::exists(root)) {
        printf("[bench] %s: not found\n", root.string().c_str());
        return;
    }
    std::vector<std::string> files;
    size_t totalBytes = 0;
    if (fs::is_regular_file(root)) {
        files.push_back(ReadFile(root));
        totalBytes += files.back().size();
    } else {
        for (auto &entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file() && IsSource(entry.path())) {
                files.push_back(ReadFile(entry.path()));
                totalBytes += files.back().size();
            }
        }
    }
    if (files.empty()) {
        printf("[bench] %s: no C/C++ sources\n", root.string().c_str());
        return;
    }

    // UTF-8 -> UTF-32 (once per file, part of indexing)
    auto start = Clock::now();
    std::vector<std::u32string> texts;
    texts.reserve(files.size());
    for (auto &data : files) {
        texts.push_back(UnicodeHelper::utf8to32(data));
    }
    auto msDecode = MsSince(start);

    // A copy has its own state stack - what an indexer thread uses
    auto language = Editor::Instance().GetLanguageForExtension(".cpp");

    // Fast path, one thread
    auto tokenizer = language->Tokenizer();
    size_t totalLines = 0;
    size_t totalTokens = 0;
    start = Clock::now();
    for (auto &text : texts) {
        totalLines += tokenizer.TokenizeText(text, [&totalTokens](size_t, const LangToken &) {
            totalTokens++;
        });
    }
    auto msFast = MsSince(start);

    // Fast path, N threads (a tokenizer copy each, files split round-robin)
    auto numThreads = std::max(1u, std::thread::hardware_concurrency() - 1);
    start = Clock::now();
    {
        std::vector<std::thread> threads;
        for (unsigned idxThread = 0; idxThread < numThreads; idxThread++) {
            threads.emplace_back([&texts, &language, idxThread, numThreads]() {
                auto threadTokenizer = language->Tokenizer();
                for (size_t i = idxThread; i < texts.size(); i += numThreads) {
                    threadTokenizer.TokenizeText(texts[i], [](size_t, const LangToken &) {});
                }
            });
        }
        for (auto &thread : threads) {
            thread.join();
        }
    }
    auto msThreaded = MsSince(start);

    // Structure pass (lexer + structure), one thread
    CPPStructureScanner scanner(language->Tokenizer());
    size_t totalSymbols = 0;
    size_t totalEdges = 0;
    start = Clock::now();
    for (auto &text : texts) {
        auto structure = scanner.Scan(text);
        totalSymbols += structure.symbols.size();
        totalEdges += structure.edges.size();
    }
    auto msStructure = MsSince(start);

    // Editor path - TextBuffer lines + synchronous Reparse
    auto mainConfig = Config::Instance()["main"];
    mainConfig.SetBool("threaded_syntaxparser", false);
    start = Clock::now();
    for (auto &data : files) {
        auto buffer = TextBuffer::CreateEmptyBuffer();
        std::stringstream lines(data);
        std::string lineText;
        while (std::getline(lines, lineText)) {
            buffer->AddLineUTF8(lineText.c_str());
        }
        buffer->SetLanguage(language);
        buffer->Reparse();
    }
    auto msEditor = MsSince(start);

    auto linesPerSec = [totalLines](double ms) {
        return (ms > 0) ? (double)totalLines / (ms / 1000.0) : 0.0;
    };
    printf("[bench] %s - %zu files, %zu lines, %.1f MB, %zu tokens\n", root.string().c_str(),
           files.size(), totalLines, (double)totalBytes / (1024.0 * 1024.0), totalTokens);
    printf("[bench]   utf8->utf32       : %9.1f ms\n", msDecode);
    printf("[bench]   editor path       : %9.1f ms  %10.0f lines/s\n", msEditor, linesPerSec(msEditor));
    printf("[bench]   fast path, 1 thr  : %9.1f ms  %10.0f lines/s\n", msFast, linesPerSec(msFast));
    printf("[bench]   fast path, %2u thr : %9.1f ms  %10.0f lines/s\n", numThreads, msThreaded, linesPerSec(msThreaded));
    printf("[bench]   structure, 1 thr  : %9.1f ms  %10.0f lines/s  (%zu symbols, %zu edges)\n", msStructure, linesPerSec(msStructure), totalSymbols, totalEdges);
}

static std::string ReadFile(const fs::path &path) {
    std::ifstream file(path, std::ios::binary);
    std::stringstream data;
    data << file.rdbuf();
    return data.str();
}
