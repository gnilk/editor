//
// Code index (docs/code-index.md). Step 0: the lexer fast path 'LangLineTokenizer::TokenizeText' - same
// tokens as the editor's parse, line numbers, line endings. Step 1: the C/C++ structure pass
// (CPPStructureScanner) - one case per declaration shape, plus the hard ones. Throughput is measured by
// 'gedit-bench' (utests/bench/), not here: trun links the ASan runtime, which skews anything
// allocation-heavy.
//
#include <testinterface.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "Core/Editor.h"
#include "Core/Config/Config.h"
#include "Core/TextBuffer.h"
#include "Core/UnicodeHelper.h"
#include "Core/Language/LangLineTokenizer.h"
#include "Core/Index/CPPStructureScanner.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_codeindex(ITesting *t);
    DLL_EXPORT int test_codeindex_tokenize_lines(ITesting *t);
    DLL_EXPORT int test_codeindex_tokenize_matches_editor(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_classes(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_inherits(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_holds(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_outofline(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_enums(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_ctypedefs(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_preprocessor(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_macros(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_initializers(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_balance(ITesting *t);
}

namespace {
    struct TokenAt {
        size_t idxLine;
        int idxChar;
        kLanguageTokenClass classification;
        std::u32string text;
    };

    std::vector<TokenAt> Tokenize(LangLineTokenizer &tokenizer, const std::u32string &text, size_t &outNumLines) {
        std::vector<TokenAt> tokens;
        outNumLines = tokenizer.TokenizeText(text, [&tokens](size_t idxLine, const LangToken &token) {
            tokens.push_back({idxLine, token.idxOrigStr, token.classification, token.string});
        });
        return tokens;
    }

    std::string ReadFile(const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary);
        std::stringstream data;
        data << file.rdbuf();
        return data.str();
    }

    // A copy has its own state stack - what an indexer thread uses
    LangLineTokenizer CPPTokenizer() {
        return Editor::Instance().GetLanguageForExtension(".cpp")->Tokenizer();
    }

    FileStructure ScanCPP(const std::u32string &text) {
        CPPStructureScanner scanner(CPPTokenizer());
        return scanner.Scan(text);
    }

    // The symbol with this qualified name and kind, or nullptr
    const CodeSymbol *Find(const FileStructure &structure, const std::u32string &qualifiedName, kSymbolKind kind) {
        for (auto &symbol : structure.symbols) {
            if ((symbol.qualifiedName == qualifiedName) && (symbol.kind == kind)) {
                return &symbol;
            }
        }
        return nullptr;
    }

    size_t Count(const FileStructure &structure, kSymbolKind kind) {
        return (size_t)std::count_if(structure.symbols.begin(), structure.symbols.end(), [kind](const CodeSymbol &symbol) {
            return symbol.kind == kind;
        });
    }

    bool HasEdge(const FileStructure &structure, kEdgeKind kind, const std::u32string &from, const std::u32string &to, kOwnership ownership = kOwnership::kNone) {
        for (auto &edge : structure.edges) {
            auto fromName = (edge.idxFrom >= 0) ? structure.symbols[edge.idxFrom].qualifiedName : std::u32string();
            if ((edge.kind == kind) && (fromName == from) && (edge.toName == to) && (edge.ownership == ownership)) {
                return true;
            }
        }
        return false;
    }

    bool HasEdgeTo(const FileStructure &structure, const std::u32string &to) {
        return std::any_of(structure.edges.begin(), structure.edges.end(), [&to](const CodeEdge &edge) {
            return edge.toName == to;
        });
    }
}

DLL_EXPORT int test_codeindex(ITesting *t) {
    return kTR_Pass;
}

// Line numbers follow '\n'; '\r\n' is one line ending; an empty line has no tokens; a block comment
// carries its state to the next line; a trailing newline doesn't add a line
DLL_EXPORT int test_codeindex_tokenize_lines(ITesting *t) {
    auto tokenizer = CPPTokenizer();
    size_t numLines = 0;
    auto tokens = Tokenize(tokenizer, U"int a;\r\n/* one\n two */ int b;\n\nclass Foo {};\n", numLines);
    TR_ASSERT(t, numLines == 5);

    std::vector<size_t> perLine(5, 0);
    for (auto &token : tokens) {
        TR_ASSERT(t, token.idxLine < 5);
        perLine[token.idxLine]++;
        // '\r' is dropped (as the editor's loader does) - the lexer would skip it as white space anyway
        TR_ASSERT(t, token.text.find(U'\r') == std::u32string::npos);
    }
    TR_ASSERT(t, perLine[0] > 0);
    TR_ASSERT(t, perLine[3] == 0);

    // Line 2: 'two' is still inside the comment (the state carried over), 'int' after it is code again
    bool hasCommentOnLine2 = false;
    bool hasIntOnLine2 = false;
    for (auto &token : tokens) {
        if (token.idxLine != 2) {
            continue;
        }
        if (token.text == U"two") {
            hasCommentOnLine2 = (token.classification != kLanguageTokenClass::kRegular);
        }
        if (token.text == U"int") {
            TR_ASSERT(t, token.classification == kLanguageTokenClass::kKnownType);
            TR_ASSERT(t, token.idxChar == 8);
            hasIntOnLine2 = true;
        }
    }
    TR_ASSERT(t, hasCommentOnLine2);
    TR_ASSERT(t, hasIntOnLine2);

    // Line 4: 'class' is a keyword, 'Foo' an identifier (kRegular)
    bool hasClass = false;
    bool hasFoo = false;
    for (auto &token : tokens) {
        if ((token.idxLine == 4) && (token.text == U"class")) {
            hasClass = (token.classification == kLanguageTokenClass::kKeyword);
        }
        if ((token.idxLine == 4) && (token.text == U"Foo")) {
            hasFoo = (token.classification == kLanguageTokenClass::kRegular);
        }
    }
    TR_ASSERT(t, hasClass);
    TR_ASSERT(t, hasFoo);

    // Unterminated comment at the end - the next call starts clean
    Tokenize(tokenizer, U"/* never closed\nstill comment", numLines);
    tokens = Tokenize(tokenizer, U"int c;", numLines);
    TR_ASSERT(t, !tokens.empty());
    TR_ASSERT(t, tokens[0].classification == kLanguageTokenClass::kKnownType);
    return kTR_Pass;
}

// The fast path gives the same tokens (position + class) as the editor's parse of the same file
DLL_EXPORT int test_codeindex_tokenize_matches_editor(ITesting *t) {
    auto data = ReadFile("testfiles/ConvertUTF.cpp");
    TR_ASSERT(t, !data.empty());

    // The editor path - synchronous parse
    auto mainConfig = Config::Instance()["main"];
    auto wasThreaded = mainConfig.GetBool("threaded_syntaxparser", false);
    mainConfig.SetBool("threaded_syntaxparser", false);
    auto buffer = TextBuffer::CreateEmptyBuffer();
    buffer->DeleteLineAt(0);
    std::stringstream lines(data);
    std::string lineText;
    while (std::getline(lines, lineText)) {
        if (!lineText.empty() && (lineText.back() == '\r')) {
            lineText.pop_back();
        }
        buffer->AddLineUTF8(lineText.c_str());
    }
    buffer->SetLanguage(Editor::Instance().GetLanguageForExtension(".cpp"));
    buffer->Reparse();
    mainConfig.SetBool("threaded_syntaxparser", wasThreaded);

    auto tokenizer = CPPTokenizer();
    size_t numLines = 0;
    auto tokens = Tokenize(tokenizer, UnicodeHelper::utf8to32(data), numLines);
    TR_ASSERT(t, numLines == buffer->NumLines());

    size_t idxToken = 0;
    for (size_t idxLine = 0; idxLine < numLines; idxLine++) {
        auto line = buffer->LineAt(idxLine);
        if (line->Buffer().empty()) {
            TR_ASSERT(t, (idxToken >= tokens.size()) || (tokens[idxToken].idxLine != idxLine));
            continue;
        }
        auto &attributes = line->Attributes();
        size_t idxAttrib = 0;
        // ToLineAttrib starts a line with a 'regular' attribute when the first token isn't at 0
        bool firstAtZero = (idxToken < tokens.size()) && (tokens[idxToken].idxLine == idxLine) && (tokens[idxToken].idxChar == 0);
        if (!firstAtZero) {
            idxAttrib = 1;
        }
        for (; idxAttrib < attributes.size(); idxAttrib++, idxToken++) {
            TR_ASSERT(t, idxToken < tokens.size());
            TR_ASSERT(t, tokens[idxToken].idxLine == idxLine);
            TR_ASSERT(t, tokens[idxToken].idxChar == attributes[idxAttrib].idxOrigString);
            TR_ASSERT(t, tokens[idxToken].classification == attributes[idxAttrib].tokenClass);
        }
        TR_ASSERT(t, (idxToken >= tokens.size()) || (tokens[idxToken].idxLine != idxLine));
    }
    TR_ASSERT(t, idxToken == tokens.size());
    return kTR_Pass;
}

// Namespaces (nested, C++17 'a::b'), classes with members and methods, constructors / destructors,
// declarations vs inline definitions, extents, access specifiers, a nested class
DLL_EXPORT int test_codeindex_struct_classes(ITesting *t) {
    auto structure = ScanCPP(
        U"namespace gedit {\n"                          // 0
        U"    class Document {\n"                       // 1
        U"    public:\n"                                // 2
        U"        Document();\n"                        // 3
        U"        virtual ~Document() = default;\n"     // 4
        U"        int Count() const { return n; }\n"    // 5
        U"        void Save(const std::string &path);\n"// 6
        U"    protected:\n"                             // 7
        U"        struct Inner {\n"                     // 8
        U"            int x = 0;\n"                     // 9
        U"        };\n"                                 // 10
        U"    private:\n"                               // 11
        U"        int n = 0, m;\n"                      // 12
        U"    };\n"                                     // 13
        U"}\n"                                          // 14
        U"namespace a::b {\n"                           // 15
        U"    void Free(int x);\n"                      // 16
        U"}\n");
    auto ns = Find(structure, U"gedit", kSymbolKind::kNamespace);
    TR_ASSERT(t, ns != nullptr);
    TR_ASSERT(t, (ns->beginLine == 0) && (ns->endLine == 14));
    auto cls = Find(structure, U"gedit::Document", kSymbolKind::kClass);
    TR_ASSERT(t, cls != nullptr);
    TR_ASSERT(t, cls->isDefinition && (cls->line == 1) && (cls->endLine == 13));
    TR_ASSERT(t, &structure.symbols[cls->idxParent] == ns);

    auto ctor = Find(structure, U"gedit::Document::Document", kSymbolKind::kMethod);
    TR_ASSERT(t, (ctor != nullptr) && !ctor->isDefinition);
    TR_ASSERT(t, Find(structure, U"gedit::Document::~Document", kSymbolKind::kMethod) != nullptr);
    auto count = Find(structure, U"gedit::Document::Count", kSymbolKind::kMethod);
    TR_ASSERT(t, (count != nullptr) && count->isDefinition);
    auto save = Find(structure, U"gedit::Document::Save", kSymbolKind::kMethod);
    TR_ASSERT(t, (save != nullptr) && !save->isDefinition && (save->line == 6));

    TR_ASSERT(t, Find(structure, U"gedit::Document::Inner", kSymbolKind::kStruct) != nullptr);
    TR_ASSERT(t, Find(structure, U"gedit::Document::Inner::x", kSymbolKind::kMember) != nullptr);
    TR_ASSERT(t, Find(structure, U"gedit::Document::n", kSymbolKind::kMember) != nullptr);
    TR_ASSERT(t, Find(structure, U"gedit::Document::m", kSymbolKind::kMember) != nullptr);
    // 'public' etc. are not part of a declaration; the body of Count isn't scanned
    TR_ASSERT(t, Count(structure, kSymbolKind::kMember) == 3);
    TR_ASSERT(t, Count(structure, kSymbolKind::kMethod) == 4);

    TR_ASSERT(t, Find(structure, U"a::b", kSymbolKind::kNamespace) != nullptr);
    TR_ASSERT(t, Find(structure, U"a::b::Free", kSymbolKind::kFunction) != nullptr);
    return kTR_Pass;
}

// Base lists: several bases, access specifiers, virtual, template arguments stripped, qualified names,
// 'final', an export macro before the name, a template class
DLL_EXPORT int test_codeindex_struct_inherits(ITesting *t) {
    auto structure = ScanCPP(
        U"class RootView : public ViewBase, protected virtual Observer<RootView> {};\n"
        U"struct DLL_EXPORT Leaf final : gedit::Node {};\n"
        U"template<typename T> class Box : public Base<T, 2> { T value; };\n"
        U"class Plain {};\n");
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kInherits, U"RootView", U"ViewBase"));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kInherits, U"RootView", U"Observer"));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kInherits, U"Leaf", U"gedit::Node"));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kInherits, U"Box", U"Base"));
    TR_ASSERT(t, Find(structure, U"Leaf", kSymbolKind::kStruct) != nullptr);
    TR_ASSERT(t, Find(structure, U"Box::value", kSymbolKind::kMember) != nullptr);
    TR_ASSERT(t, Find(structure, U"Plain", kSymbolKind::kClass) != nullptr);
    size_t numInherits = (size_t)std::count_if(structure.edges.begin(), structure.edges.end(), [](const CodeEdge &edge) {
        return edge.kind == kEdgeKind::kInherits;
    });
    TR_ASSERT(t, numInherits == 4);
    return kTR_Pass;
}

// Ownership of members: by value / unique_ptr / Ptr own, shared_ptr / weak_ptr / Ref share, pointers and
// references borrow, containers take their element's; library types and builtins give no edge
DLL_EXPORT int test_codeindex_struct_holds(ITesting *t) {
    auto structure = ScanCPP(
        U"class Owner {\n"
        U"    Rect rect;\n"
        U"    std::unique_ptr<Engine> engine;\n"
        U"    ModalView::Ptr modal;\n"
        U"    std::shared_ptr<Theme> theme;\n"
        U"    std::weak_ptr<Parent> parent;\n"
        U"    TextBuffer::Ref buffer = nullptr;\n"
        U"    Window *window = nullptr;\n"
        U"    const Config &config;\n"
        U"    std::vector<std::unique_ptr<Item>> items;\n"
        U"    std::map<std::string, Line::Ref> lines;\n"
        U"    std::vector<Cell> *cells;\n"
        U"    std::string name;\n"
        U"    int count = 0;\n"
        U"    std::function<void(int)> callback;\n"
        U"};\n");
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"Owner", U"Rect", kOwnership::kOwns));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"Owner", U"Engine", kOwnership::kOwns));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"Owner", U"ModalView", kOwnership::kOwns));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"Owner", U"Theme", kOwnership::kShares));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"Owner", U"Parent", kOwnership::kShares));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"Owner", U"TextBuffer", kOwnership::kShares));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"Owner", U"Window", kOwnership::kBorrows));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"Owner", U"Config", kOwnership::kBorrows));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"Owner", U"Item", kOwnership::kOwns));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"Owner", U"Line", kOwnership::kShares));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"Owner", U"Cell", kOwnership::kBorrows));
    TR_ASSERT(t, !HasEdgeTo(structure, U"std::string"));
    TR_ASSERT(t, !HasEdgeTo(structure, U"int"));
    TR_ASSERT(t, !HasEdgeTo(structure, U"std::function"));
    TR_ASSERT(t, Count(structure, kSymbolKind::kMember) == 14);
    return kTR_Pass;
}

// A .cpp: out-of-line definitions (function kind - the qualifier may be a class or a namespace), static
// member definitions, operators, a constructor initializer list with a brace initializer, free functions
DLL_EXPORT int test_codeindex_struct_outofline(ITesting *t) {
    auto structure = ScanCPP(
        U"using namespace gedit;\n"                                         // 0
        U"bool Runloop::bQuit = false;\n"                                   // 1
        U"Document::Document(int n) : count(n), items{1, 2} {\n"            // 2
        U"    if (n > 0) { Run(); }\n"                                      // 3
        U"}\n"                                                              // 4
        U"void Document::SearchFor(const std::u32string &item, bool cs) {\n"// 5
        U"    auto lambda = [](int x) { return x; };\n"                     // 6
        U"}\n"                                                              // 7
        U"bool Point::operator==(const Point &other) const { return true; }\n" // 8
        U"Document::~Document() {}\n"                                       // 9
        U"static int Helper(int a) {\n"                                     // 10
        U"    return a;\n"                                                  // 11
        U"}\n");
    auto ctor = Find(structure, U"Document::Document", kSymbolKind::kFunction);
    TR_ASSERT(t, (ctor != nullptr) && ctor->isDefinition);
    TR_ASSERT(t, (ctor->beginLine == 2) && (ctor->endLine == 4));
    auto search = Find(structure, U"Document::SearchFor", kSymbolKind::kFunction);
    TR_ASSERT(t, (search != nullptr) && (search->endLine == 7));
    TR_ASSERT(t, Find(structure, U"Point::operator==", kSymbolKind::kFunction) != nullptr);
    TR_ASSERT(t, Find(structure, U"Document::~Document", kSymbolKind::kFunction) != nullptr);
    TR_ASSERT(t, Find(structure, U"Helper", kSymbolKind::kFunction) != nullptr);
    TR_ASSERT(t, Find(structure, U"Runloop::bQuit", kSymbolKind::kVariable) != nullptr);
    // Nothing from inside the bodies, nothing from 'using namespace'
    TR_ASSERT(t, Find(structure, U"Run", kSymbolKind::kFunction) == nullptr);
    TR_ASSERT(t, Find(structure, U"lambda", kSymbolKind::kVariable) == nullptr);
    TR_ASSERT(t, structure.symbols.size() == 6);
    return kTR_Pass;
}

// Enums: plain (enumerators in the enclosing scope), enum class (enumerators scoped), an underlying type,
// enumerator values with expressions
DLL_EXPORT int test_codeindex_struct_enums(ITesting *t) {
    auto structure = ScanCPP(
        U"namespace ui {\n"
        U"    enum Color { kRed = 1 << 2, kGreen = MAKE(1, 2), kBlue };\n"
        U"    enum class kMode : uint8_t {\n"
        U"        kNone,\n"
        U"        kEdit,\n"
        U"    };\n"
        U"}\n");
    TR_ASSERT(t, Find(structure, U"ui::Color", kSymbolKind::kEnum) != nullptr);
    TR_ASSERT(t, Find(structure, U"ui::kRed", kSymbolKind::kEnumerator) != nullptr);
    TR_ASSERT(t, Find(structure, U"ui::kGreen", kSymbolKind::kEnumerator) != nullptr);
    TR_ASSERT(t, Find(structure, U"ui::kBlue", kSymbolKind::kEnumerator) != nullptr);
    auto mode = Find(structure, U"ui::kMode", kSymbolKind::kEnum);
    TR_ASSERT(t, (mode != nullptr) && (mode->endLine == 5));
    TR_ASSERT(t, Find(structure, U"ui::kMode::kNone", kSymbolKind::kEnumerator) != nullptr);
    TR_ASSERT(t, Find(structure, U"ui::kMode::kEdit", kSymbolKind::kEnumerator) != nullptr);
    TR_ASSERT(t, Count(structure, kSymbolKind::kEnumerator) == 5);
    return kTR_Pass;
}

// C: typedef'd anonymous struct/enum named at the ';' (extent kept), a typedef alias of a named struct,
// 'struct {...} name;' members, function pointer typedefs and members, a struct-returning function (not
// a struct definition), forward declarations
DLL_EXPORT int test_codeindex_struct_ctypedefs(ITesting *t) {
    auto structure = ScanCPP(
        U"typedef struct\n"                             // 0
        U"{\n"                                          // 1
        U"    int flags;\n"                             // 2
        U"    GOA_PIXMAP *textures[4];\n"               // 3
        U"    struct {\n"                               // 4
        U"        int size;\n"                          // 5
        U"    } vtxinfo;\n"                             // 6
        U"    void (*dispose)(void *);\n"               // 7
        U"} GOA_MATERIAL;\n"                            // 8
        U"typedef enum { kA, kB } State;\n"             // 9
        U"typedef struct Node { int v; } Node_t;\n"     // 10
        U"typedef void (*Callback)(int);\n"             // 11
        U"typedef unsigned int u32;\n"                  // 12
        U"struct Node *make_node(int v) { return 0; }\n"// 13
        U"struct Later;\n"                              // 14
        U"class Fwd;\n"                                 // 15
        U"namespace net {\n"                            // 16
        U"typedef struct { union { int a; } u; } Packet;\n" // 17 - an anonymous type inside one being named
        U"}\n");
    auto material = Find(structure, U"GOA_MATERIAL", kSymbolKind::kStruct);
    TR_ASSERT(t, material != nullptr);
    TR_ASSERT(t, (material->line == 8) && (material->beginLine == 0) && (material->endLine == 8));
    TR_ASSERT(t, Find(structure, U"GOA_MATERIAL::flags", kSymbolKind::kMember) != nullptr);
    TR_ASSERT(t, Find(structure, U"GOA_MATERIAL::textures", kSymbolKind::kMember) != nullptr);
    TR_ASSERT(t, Find(structure, U"GOA_MATERIAL::vtxinfo", kSymbolKind::kMember) != nullptr);
    TR_ASSERT(t, Find(structure, U"GOA_MATERIAL::dispose", kSymbolKind::kMember) != nullptr);
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"GOA_MATERIAL", U"GOA_PIXMAP", kOwnership::kBorrows));
    TR_ASSERT(t, Find(structure, U"State", kSymbolKind::kEnum) != nullptr);
    TR_ASSERT(t, Find(structure, U"kA", kSymbolKind::kEnumerator) != nullptr);
    TR_ASSERT(t, Find(structure, U"Node", kSymbolKind::kStruct) != nullptr);
    TR_ASSERT(t, Find(structure, U"Node_t", kSymbolKind::kAlias) != nullptr);
    TR_ASSERT(t, Find(structure, U"Callback", kSymbolKind::kAlias) != nullptr);
    TR_ASSERT(t, Find(structure, U"u32", kSymbolKind::kAlias) != nullptr);
    auto makeNode = Find(structure, U"make_node", kSymbolKind::kFunction);
    TR_ASSERT(t, (makeNode != nullptr) && makeNode->isDefinition);
    auto later = Find(structure, U"Later", kSymbolKind::kStruct);
    TR_ASSERT(t, (later != nullptr) && !later->isDefinition);
    auto fwd = Find(structure, U"Fwd", kSymbolKind::kClass);
    TR_ASSERT(t, (fwd != nullptr) && !fwd->isDefinition);
    TR_ASSERT(t, Find(structure, U"net::Packet", kSymbolKind::kStruct) != nullptr);
    TR_ASSERT(t, Find(structure, U"net::Packet", kSymbolKind::kUnion) != nullptr);
    TR_ASSERT(t, Find(structure, U"net::Packet::a", kSymbolKind::kMember) != nullptr);
    TR_ASSERT(t, Find(structure, U"net::Packet::u", kSymbolKind::kMember) != nullptr);
    TR_ASSERT(t, Count(structure, kSymbolKind::kStruct) == 5);      // GOA_MATERIAL, the anonymous one, Node, Later, Packet
    return kTR_Pass;
}

// Includes (quoted / system), defines (with parameters), conditionals (first arm; '#if 0' takes the
// '#else'), a multi-line #define, braces and 'class' inside comments and strings
DLL_EXPORT int test_codeindex_struct_preprocessor(ITesting *t) {
    auto structure = ScanCPP(
        U"#include \"Core/Line.h\"\n"
        U"#include <memory>\n"
        U"#define MAX_LEN 256\n"
        U"#define MIN(a, b) ((a) < (b) ? (a) : (b))\n"
        U"#define MULTI(x) \\\n"
        U"    struct x {\n"
        U"#ifdef __APPLE__\n"
        U"class MacOnly {\n"
        U"#else\n"
        U"class Other {\n"
        U"#endif\n"
        U"};\n"
        U"#if 0\n"
        U"class Dead {};\n"
        U"#else\n"
        U"class Alive {};\n"
        U"#endif\n"
        U"// class Commented { };\n"
        U"/* class Blocked {\n"
        U"   } */\n"
        U"const char *text = \"class Quoted { }\";\n"
        U"#define TRAILING(x) \\\n"
        U"    if (x) { \\\n"
        U"    }       \\\n"
        U"\n"                                                   // the continuation ends on the empty line
        U"class Last {};\n");
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kIncludes, U"", U"Core/Line.h"));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kIncludes, U"", U"memory"));
    bool isSystem = false;
    for (auto &edge : structure.edges) {
        isSystem = isSystem || ((edge.toName == U"memory") && edge.isSystemInclude);
    }
    TR_ASSERT(t, isSystem);
    TR_ASSERT(t, Find(structure, U"MAX_LEN", kSymbolKind::kMacro) != nullptr);
    TR_ASSERT(t, Find(structure, U"MIN", kSymbolKind::kMacro) != nullptr);
    TR_ASSERT(t, Find(structure, U"MULTI", kSymbolKind::kMacro) != nullptr);
    TR_ASSERT(t, Find(structure, U"x", kSymbolKind::kStruct) == nullptr);
    auto macOnly = Find(structure, U"MacOnly", kSymbolKind::kClass);
    TR_ASSERT(t, (macOnly != nullptr) && (macOnly->endLine == 11));
    TR_ASSERT(t, Find(structure, U"Other", kSymbolKind::kClass) == nullptr);
    TR_ASSERT(t, Find(structure, U"Dead", kSymbolKind::kClass) == nullptr);
    TR_ASSERT(t, Find(structure, U"Alive", kSymbolKind::kClass) != nullptr);
    TR_ASSERT(t, Find(structure, U"Commented", kSymbolKind::kClass) == nullptr);
    TR_ASSERT(t, Find(structure, U"Blocked", kSymbolKind::kClass) == nullptr);
    TR_ASSERT(t, Find(structure, U"Quoted", kSymbolKind::kClass) == nullptr);
    TR_ASSERT(t, Find(structure, U"text", kSymbolKind::kVariable) != nullptr);
    // Braces stayed balanced through all of it
    auto last = Find(structure, U"Last", kSymbolKind::kClass);
    TR_ASSERT(t, (last != nullptr) && (last->idxParent == -1));
    return kTR_Pass;
}

// Macros in code: a macro call before a declaration is skipped, a macro call statement is not a function,
// a macro-headed block ('TEST(x) {') adds nothing, export macros before a function's type
DLL_EXPORT int test_codeindex_struct_macros(ITesting *t) {
    auto structure = ScanCPP(
        U"class Widget {\n"
        U"    DECLARE_DYNAMIC(Widget)\n"
        U"    void Paint();\n"
        U"    Q_PROPERTY(int value);\n"
        U"};\n"
        U"REGISTER_WIDGET(Widget);\n"
        U"TEST(Widget, Paints) {\n"
        U"    Widget w;\n"
        U"}\n"
        U"DLL_EXPORT int test_widget(ITesting *t) {\n"
        U"    return 0;\n"
        U"}\n");
    TR_ASSERT(t, Find(structure, U"Widget::Paint", kSymbolKind::kMethod) != nullptr);
    TR_ASSERT(t, Find(structure, U"Widget::DECLARE_DYNAMIC", kSymbolKind::kMethod) == nullptr);
    TR_ASSERT(t, Find(structure, U"Widget::Q_PROPERTY", kSymbolKind::kMethod) == nullptr);
    TR_ASSERT(t, Find(structure, U"REGISTER_WIDGET", kSymbolKind::kFunction) == nullptr);
    TR_ASSERT(t, Find(structure, U"TEST", kSymbolKind::kFunction) == nullptr);
    TR_ASSERT(t, Find(structure, U"w", kSymbolKind::kVariable) == nullptr);
    TR_ASSERT(t, Find(structure, U"test_widget", kSymbolKind::kFunction) != nullptr);
    TR_ASSERT(t, Count(structure, kSymbolKind::kMethod) == 1);
    TR_ASSERT(t, Count(structure, kSymbolKind::kFunction) == 1);
    return kTR_Pass;
}

// Braces that aren't scopes: '= {...}', 'Foo f{1}', a lambda, a default argument, extern "C" (transparent)
DLL_EXPORT int test_codeindex_struct_initializers(ITesting *t) {
    auto structure = ScanCPP(
        U"static const int table[] = { 1, 2, 3 };\n"
        U"Point origin{0, 0};\n"
        U"auto square = [](int x) { return x * x; };\n"
        U"void Draw(Rect r = {}, int n = 1);\n"
        U"extern \"C\" {\n"
        U"    int c_function(void);\n"
        U"}\n"
        U"class After { int a{0}; std::vector<int> v = {1, 2}; };\n");
    TR_ASSERT(t, Find(structure, U"table", kSymbolKind::kVariable) != nullptr);
    TR_ASSERT(t, Find(structure, U"origin", kSymbolKind::kVariable) != nullptr);
    TR_ASSERT(t, Find(structure, U"square", kSymbolKind::kVariable) != nullptr);
    TR_ASSERT(t, Find(structure, U"Draw", kSymbolKind::kFunction) != nullptr);
    auto cFunction = Find(structure, U"c_function", kSymbolKind::kFunction);
    TR_ASSERT(t, (cFunction != nullptr) && (cFunction->idxParent == -1));
    TR_ASSERT(t, Find(structure, U"After", kSymbolKind::kClass) != nullptr);
    TR_ASSERT(t, Find(structure, U"After::a", kSymbolKind::kMember) != nullptr);
    TR_ASSERT(t, Find(structure, U"After::v", kSymbolKind::kMember) != nullptr);
    TR_ASSERT(t, structure.symbols.size() == 8);
    return kTR_Pass;
}

// Brace balance at the end of the file - the signal for a lexer misread (gedit-bench --check)
DLL_EXPORT int test_codeindex_struct_balance(ITesting *t) {
    auto balanced = ScanCPP(U"namespace a {\nclass B { void f() { if (x) { } } };\n}\n");
    TR_ASSERT(t, (balanced.numUnclosedBraces == 0) && (balanced.numStrayCloseBraces == 0));

    auto unclosed = ScanCPP(U"int x;\nnamespace a {\nclass B {\n  void f() {\n");
    TR_ASSERT(t, unclosed.numUnclosedBraces == 3);
    TR_ASSERT(t, unclosed.lineFirstUnclosed == 1);

    auto stray = ScanCPP(U"void f() {\n}\n}\nint y;\n}\n");
    TR_ASSERT(t, stray.numStrayCloseBraces == 2);
    TR_ASSERT(t, stray.lineFirstStrayClose == 2);
    TR_ASSERT(t, stray.numUnclosedBraces == 0);
    return kTR_Pass;
}
