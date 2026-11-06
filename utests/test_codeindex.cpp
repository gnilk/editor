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
#include <tuple>
#include <vector>

#include "Core/Editor.h"
#include "Core/Config/Config.h"
#include "Core/TextBuffer.h"
#include "Core/UnicodeHelper.h"
#include "Core/Language/LangLineTokenizer.h"
#include "Core/Index/CodeIndex.h"
#include "Core/Index/CPPStructureScanner.h"
#include "Core/Index/Indexer.h"
#include "Core/Index/SemanticNames.h"
#include "Core/Index/GoToResolver.h"
#include "Core/Index/NavigationStack.h"
#include "Core/ActionHelper.h"
#include "Core/Runloop.h"
#include "Core/Session/SessionSerializer.h"
#include "Core/Index/StructureScanner.h"
#include "Core/Editor/Modals/SymbolSearchModal.h"
#include "Core/UI/UIHost.h"
#include "Core/UI/Views/RootView.h"
#include <atomic>
#include <functional>
#include <thread>

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_codeindex(ITesting *t);
    DLL_EXPORT int test_codeindex_tokenize_lines(ITesting *t);
    DLL_EXPORT int test_codeindex_tokenize_matches_editor(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_classes(ITesting *t);
    DLL_EXPORT int test_codeindex_library_shipped_lists(ITesting *t);
    DLL_EXPORT int test_codeindex_library_malformed_list(ITesting *t);
    DLL_EXPORT int test_codeindex_library_std_qualified(ITesting *t);
    DLL_EXPORT int test_codeindex_library_std_beats_file(ITesting *t);
    DLL_EXPORT int test_codeindex_library_c_names(ITesting *t);
    DLL_EXPORT int test_codeindex_library_bare_needs_using(ITesting *t);
    DLL_EXPORT int test_codeindex_library_using_name(ITesting *t);
    DLL_EXPORT int test_codeindex_library_using_namespace_scope(ITesting *t);
    DLL_EXPORT int test_codeindex_library_project_wins(ITesting *t);
    DLL_EXPORT int test_codeindex_library_using_edit(ITesting *t);
    DLL_EXPORT int test_codeindex_library_other_scope(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_quick_mode_view_switch(ITesting *t);
    DLL_EXPORT int test_codeindex_edges_open_file(ITesting *t);
    DLL_EXPORT int test_codeindex_edges_region_scan(ITesting *t);
    DLL_EXPORT int test_codeindex_edges_follow_lines(ITesting *t);
    DLL_EXPORT int test_codeindex_edges_incremental(ITesting *t);
    DLL_EXPORT int test_codeindex_edges_typedef(ITesting *t);
    DLL_EXPORT int test_codeindex_edges_typedef_region(ITesting *t);
    DLL_EXPORT int test_codeindex_relations(ITesting *t);
    DLL_EXPORT int test_codeindex_relations_resolve(ITesting *t);
    DLL_EXPORT int test_codeindex_relations_replace(ITesting *t);
    DLL_EXPORT int test_codeindex_relations_macro_type(ITesting *t);
    DLL_EXPORT int test_codeindex_goto_nearest_symbol(ITesting *t);
    DLL_EXPORT int test_codeindex_goto_find_exact(ITesting *t);
    DLL_EXPORT int test_codeindex_goto_toggle(ITesting *t);
    DLL_EXPORT int test_codeindex_goto_modes(ITesting *t);
    DLL_EXPORT int test_codeindex_goto_already_there(ITesting *t);
    DLL_EXPORT int test_codeindex_goto_no_target(ITesting *t);
    DLL_EXPORT int test_codeindex_goto_cursor_positions(ITesting *t);
    DLL_EXPORT int test_codeindex_goto_qualifier(ITesting *t);
    DLL_EXPORT int test_codeindex_goto_object_syntax(ITesting *t);
    DLL_EXPORT int test_codeindex_goto_enclosing_scope(ITesting *t);
    DLL_EXPORT int test_codeindex_goto_constructor(ITesting *t);
    DLL_EXPORT int test_codeindex_goto_proximity(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_back_forward(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_jump_drops_forward(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_same_symbol(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_bounce(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_no_from(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_cap(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_lines_moved(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_symbol_gone(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_overloads(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_session(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_other_file(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_back_after_edit(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_session_yaml(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_real_documents(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_missing_file(ITesting *t);
    DLL_EXPORT int test_codeindex_nav_load_symlinked(ITesting *t);
    DLL_EXPORT int test_codeindex_goto_modal_list(ITesting *t);
    DLL_EXPORT int test_codeindex_library_list_header(ITesting *t);
    DLL_EXPORT int test_codeindex_library_list_include_edit(ITesting *t);
    DLL_EXPORT int test_codeindex_library_list_no_headers(ITesting *t);
    DLL_EXPORT int test_codeindex_library_list_two_origins(ITesting *t);
    DLL_EXPORT int test_codeindex_library_list_scope(ITesting *t);
    DLL_EXPORT int test_codeindex_library_list_scope_using(ITesting *t);
    DLL_EXPORT int test_codeindex_library_list_scope_template(ITesting *t);
    DLL_EXPORT int test_codeindex_library_list_project_dir(ITesting *t);
    DLL_EXPORT int test_codeindex_library_list_bad_project_file(ITesting *t);
    DLL_EXPORT int test_codeindex_library_themefallback(ITesting *t);
    DLL_EXPORT int test_codeindex_library_realline(ITesting *t);
    DLL_EXPORT int test_codeindex_library_project_ambiguous(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_inherits(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_holds(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_outofline(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_enums(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_ctypedefs(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_preprocessor(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_macros(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_initializers(ITesting *t);
    DLL_EXPORT int test_codeindex_struct_balance(ITesting *t);
    DLL_EXPORT int test_codeindex_semantic_names(ITesting *t);
    DLL_EXPORT int test_codeindex_semantic_buffer(ITesting *t);
    DLL_EXPORT int test_codeindex_semantic_edit(ITesting *t);
    DLL_EXPORT int test_codeindex_semantic_linefed(ITesting *t);
    DLL_EXPORT int test_codeindex_semantic_regionlocal(ITesting *t);
    DLL_EXPORT int test_codeindex_semantic_delete(ITesting *t);
    DLL_EXPORT int test_codeindex_semantic_resume(ITesting *t);
    DLL_EXPORT int test_codeindex_semantic_incremental(ITesting *t);
    DLL_EXPORT int test_codeindex_store_matchquality(ITesting *t);
    DLL_EXPORT int test_codeindex_store_ranking(ITesting *t);
    DLL_EXPORT int test_codeindex_store_scope(ITesting *t);
    DLL_EXPORT int test_codeindex_store_replace(ITesting *t);
    DLL_EXPORT int test_codeindex_store_outofline(ITesting *t);
    DLL_EXPORT int test_codeindex_buffer_lines(ITesting *t);
    DLL_EXPORT int test_codeindex_buffer_region(ITesting *t);
    DLL_EXPORT int test_codeindex_buffer_handler(ITesting *t);
    DLL_EXPORT int test_codeindex_buffer_pause(ITesting *t);
    DLL_EXPORT int test_codeindex_workspace_meanings(ITesting *t);
    DLL_EXPORT int test_codeindex_workspace_context(ITesting *t);
    DLL_EXPORT int test_codeindex_workspace_snapshot(ITesting *t);
    DLL_EXPORT int test_codeindex_workspace_buffer(ITesting *t);
    DLL_EXPORT int test_codeindex_workspace_fileownfirst(ITesting *t);
    DLL_EXPORT int test_codeindex_workspace_otherlanguage(ITesting *t);
    DLL_EXPORT int test_codeindex_workspace_recolour(ITesting *t);
    DLL_EXPORT int test_codeindex_workspace_fromdisk(ITesting *t);
    DLL_EXPORT int test_codeindex_workspace_syntax(ITesting *t);
    DLL_EXPORT int test_codeindex_workspace_syntaxbuffer(ITesting *t);
    DLL_EXPORT int test_codeindex_indexer_buffer(ITesting *t);
    DLL_EXPORT int test_codeindex_indexer_gitignore(ITesting *t);
    DLL_EXPORT int test_codeindex_indexer_changedfolder(ITesting *t);
    DLL_EXPORT int test_codeindex_indexer_goneroot(ITesting *t);
    DLL_EXPORT int test_codeindex_modal_markers(ITesting *t);
    DLL_EXPORT int test_codeindex_modal_rows(ITesting *t);
    DLL_EXPORT int test_codeindex_modal_location(ITesting *t);
    DLL_EXPORT int test_codeindex_modal_preview(ITesting *t);
    DLL_EXPORT int test_codeindex_indexer_folder(ITesting *t);
    DLL_EXPORT int test_codeindex_indexer_latin1(ITesting *t);
    DLL_EXPORT int test_codeindex_indexer_change(ITesting *t);
    DLL_EXPORT int test_codeindex_indexer_concurrent(ITesting *t);
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
            if ((edge.kind == kind) && (edge.fromName == from) && (edge.toName == to) && (edge.ownership == ownership)) {
                return true;
            }
        }
        return false;
    }

    // A synchronously parsed C++ buffer
    TextBuffer::Ref CPPBuffer(const std::vector<std::u32string> &lines) {
        auto mainConfig = Config::Instance()["main"];
        auto wasThreaded = mainConfig.GetBool("threaded_syntaxparser", false);
        mainConfig.SetBool("threaded_syntaxparser", false);
        auto buffer = TextBuffer::CreateEmptyBuffer();
        buffer->DeleteLineAt(0);
        for (auto &line : lines) {
            buffer->AddLineUTF8(UnicodeHelper::utf32to8(line).c_str());
        }
        buffer->SetLanguage(Editor::Instance().GetLanguageForExtension(".cpp"));
        mainConfig.SetBool("threaded_syntaxparser", wasThreaded);
        return buffer;
    }

    // The token class at the first occurrence of 'word' on the line
    kLanguageTokenClass ClassOf(TextBuffer::Ref buffer, size_t idxLine, const std::u32string &word) {
        auto line = buffer->LineAt(idxLine);
        auto idxWord = (int)line->Buffer().find(word);
        auto tokenClass = kLanguageTokenClass::kUnknown;
        for (auto &attrib : line->Attributes()) {
            if (attrib.idxOrigString > idxWord) {
                break;
            }
            tokenClass = attrib.tokenClass;
        }
        return tokenClass;
    }

    // An edit as the editor does it: the lines change, then the region is reparsed (synchronously)
    void EditRegion(TextBuffer::Ref buffer, size_t idxStart, size_t idxEnd, const std::function<void()> &edit) {
        auto mainConfig = Config::Instance()["main"];
        auto wasThreaded = mainConfig.GetBool("threaded_syntaxparser", false);
        mainConfig.SetBool("threaded_syntaxparser", false);
        edit();
        buffer->ReparseRegion(idxStart, idxEnd);
        mainConfig.SetBool("threaded_syntaxparser", wasThreaded);
    }

    std::vector<std::u32string> BufferLines(TextBuffer::Ref buffer) {
        std::vector<std::u32string> lines;
        for (size_t i = 0; i < buffer->NumLines(); i++) {
            lines.push_back(buffer->LineAt(i)->Buffer());
        }
        return lines;
    }

    const void *StateAt(TextBuffer::Ref buffer, size_t idxLine) {
        auto &structure = buffer->LineAt(idxLine)->GetStructure();
        return (structure != nullptr) ? structure->stateAtStart.get() : nullptr;
    }

    bool HasEdgeTo(const FileStructure &structure, const std::u32string &to) {
        return std::any_of(structure.edges.begin(), structure.edges.end(), [&to](const CodeEdge &edge) {
            return edge.toName == to;
        });
    }
}

DLL_EXPORT int test_codeindex(ITesting *t) {
    // Headless screen (test_main) - the Go to symbol modal needs it to initialize
    if (UIHost::Instance().GetScreen() == nullptr) {
        Editor::Instance().OpenScreen();
    }
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
// (apart from the semantic classes the buffer adds)
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
            // The buffer's parse also applies semantic names (step 1b) - those are 'regular' to the lexer
            auto editorClass = attributes[idxAttrib].tokenClass;
            bool isSemantic = (editorClass >= kLanguageTokenClass::kUserType) && (editorClass < kLanguageTokenClass::kLastTokenClass);
            TR_ASSERT(t, tokens[idxToken].classification == (isSemantic ? kLanguageTokenClass::kRegular : editorClass));
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

// Name -> class from a file's structure; two meanings stay regular, a constructor keeps its class's meaning
DLL_EXPORT int test_codeindex_semantic_names(ITesting *t) {
    auto names = SemanticNames::FromStructure(ScanCPP(
        U"#define MAX_ITEMS 16\n"
        U"namespace app {\n"
        U"    enum class kMode { kIdle, kBusy };\n"
        U"    class Widget {\n"
        U"    public:\n"
        U"        Widget();\n"
        U"        ~Widget();\n"
        U"        void Draw();\n"
        U"        int size;\n"
        U"    };\n"
        U"    struct Other { void size(); };\n"
        U"}\n"
        U"void Run(int count);\n"));
    TR_ASSERT(t, names.Lookup(U"MAX_ITEMS") == kLanguageTokenClass::kMacro);
    TR_ASSERT(t, names.Lookup(U"app") == kLanguageTokenClass::kNamespace);
    TR_ASSERT(t, names.Lookup(U"kMode") == kLanguageTokenClass::kUserType);
    TR_ASSERT(t, names.Lookup(U"kBusy") == kLanguageTokenClass::kEnumValue);
    TR_ASSERT(t, names.Lookup(U"Widget") == kLanguageTokenClass::kUserType);
    TR_ASSERT(t, names.Lookup(U"Draw") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, names.Lookup(U"Run") == kLanguageTokenClass::kFunction);
    // A member in Widget, a method in Other
    TR_ASSERT(t, names.Lookup(U"size") == kLanguageTokenClass::kRegular);
    // Parameters aren't symbols
    TR_ASSERT(t, names.Lookup(U"count") == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

// The buffer's parse reclassifies the identifiers the file declares - at their use sites too
DLL_EXPORT int test_codeindex_semantic_buffer(ITesting *t) {
    auto buffer = CPPBuffer({
        U"#define LIMIT 4",
        U"class Item {",
        U"public:",
        U"    void Update();",
        U"    int counter;",
        U"};",
        U"void Item::Update() {",
        U"    counter = LIMIT;",
        U"    other = 1;",
        U"}",
    });
    TR_ASSERT(t, ClassOf(buffer, 1, U"Item") == kLanguageTokenClass::kUserType);
    TR_ASSERT(t, ClassOf(buffer, 3, U"Update") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassOf(buffer, 6, U"Item") == kLanguageTokenClass::kUserType);
    TR_ASSERT(t, ClassOf(buffer, 6, U"Update") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassOf(buffer, 7, U"counter") == kLanguageTokenClass::kMember);
    TR_ASSERT(t, ClassOf(buffer, 7, U"LIMIT") == kLanguageTokenClass::kMacro);
    // Unknown names, and what the tokenizer already classified, are untouched
    TR_ASSERT(t, ClassOf(buffer, 8, U"other") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, ClassOf(buffer, 3, U"void") == kLanguageTokenClass::kKnownType);
    // The theme colour follows the class
    auto line = buffer->LineAt(7);
    auto [memberColor, bg] = Editor::Instance().ColorFromLanguageToken(kLanguageTokenClass::kMember);
    bool hasMemberColor = std::any_of(line->Attributes().begin(), line->Attributes().end(), [&memberColor](const Line::LineAttrib &attrib) {
        auto &fg = attrib.foregroundColor;
        return (attrib.tokenClass == kLanguageTokenClass::kMember) && (fg.RedAsInt(255) == memberColor.RedAsInt(255)) &&
               (fg.GreenAsInt(255) == memberColor.GreenAsInt(255)) && (fg.BlueAsInt(255) == memberColor.BlueAsInt(255));
    });
    TR_ASSERT(t, hasMemberColor);
    return kTR_Pass;
}

// An edit that declares a name recolours its earlier uses (region parse, small file)
DLL_EXPORT int test_codeindex_semantic_edit(ITesting *t) {
    auto buffer = CPPBuffer({
        U"void Tick() {",
        U"    Advance();",
        U"}",
    });
    TR_ASSERT(t, ClassOf(buffer, 1, U"Advance") == kLanguageTokenClass::kRegular);

    auto mainConfig = Config::Instance()["main"];
    auto wasThreaded = mainConfig.GetBool("threaded_syntaxparser", false);
    mainConfig.SetBool("threaded_syntaxparser", false);
    buffer->Insert(3, U"void Advance();");
    buffer->ReparseRegion(3, 4);
    mainConfig.SetBool("threaded_syntaxparser", wasThreaded);

    TR_ASSERT(t, ClassOf(buffer, 3, U"Advance") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassOf(buffer, 1, U"Advance") == kLanguageTokenClass::kFunction);
    return kTR_Pass;
}

// The line-fed scan (the highlighter's tokens) finds what the text scan finds
DLL_EXPORT int test_codeindex_semantic_linefed(ITesting *t) {
    auto data = ReadFile("testfiles/ConvertUTF.cpp");
    TR_ASSERT(t, !data.empty());
    std::vector<std::u32string> lines;
    std::stringstream stream(data);
    std::string lineText;
    while (std::getline(stream, lineText)) {
        if (!lineText.empty() && (lineText.back() == '\r')) {
            lineText.pop_back();
        }
        lines.push_back(UnicodeHelper::utf8to32(lineText));
    }
    auto buffer = CPPBuffer(lines);
    std::u32string text;
    for (size_t i = 0; i < buffer->NumLines(); i++) {
        text += buffer->LineAt(i)->Buffer() + U"\n";
    }
    auto fromText = ScanCPP(text);

    CPPStructureScanner scanner(CPPTokenizer());
    scanner.Begin(nullptr);
    for (size_t i = 0; i < buffer->NumLines(); i++) {
        scanner.StateAtLineStart();
        scanner.ScanLine(*buffer->LineAt(i), (uint32_t)i);
    }
    auto fromLines = scanner.End();
    TR_ASSERT(t, fromText.symbols.size() > 10);
    TR_ASSERT(t, fromLines.symbols.size() == fromText.symbols.size());
    for (size_t i = 0; i < fromText.symbols.size(); i++) {
        TR_ASSERT(t, fromLines.symbols[i].qualifiedName == fromText.symbols[i].qualifiedName);
        TR_ASSERT(t, fromLines.symbols[i].kind == fromText.symbols[i].kind);
        TR_ASSERT(t, fromLines.symbols[i].line == fromText.symbols[i].line);
    }
    TR_ASSERT(t, fromLines.edges.size() == fromText.edges.size());
    return kTR_Pass;
}

// An edit in a function body rescans around the edit only - lines far away keep their stored state
DLL_EXPORT int test_codeindex_semantic_regionlocal(ITesting *t) {
    std::vector<std::u32string> lines;
    for (int i = 0; i < 60; i++) {
        lines.push_back(U"void Func" + UnicodeHelper::utf8to32(std::to_string(i)) + U"() {");
        lines.push_back(U"    int x = 1;");
        lines.push_back(U"}");
    }
    auto buffer = CPPBuffer(lines);
    auto stateNear = StateAt(buffer, 3);
    auto stateFar = StateAt(buffer, 150);
    TR_ASSERT(t, (stateNear != nullptr) && (stateFar != nullptr));

    EditRegion(buffer, 91, 92, [&buffer]() {
        buffer->LineAt(91)->Append(std::u32string(U" x++;"));
    });
    TR_ASSERT(t, StateAt(buffer, 3) == stateNear);
    TR_ASSERT(t, StateAt(buffer, 150) == stateFar);
    return kTR_Pass;
}

// A deleted declaration takes its name along
DLL_EXPORT int test_codeindex_semantic_delete(ITesting *t) {
    auto buffer = CPPBuffer({
        U"void Helper();",
        U"void Main() {",
        U"    Helper();",
        U"}",
    });
    TR_ASSERT(t, ClassOf(buffer, 2, U"Helper") == kLanguageTokenClass::kFunction);
    EditRegion(buffer, 0, 1, [&buffer]() {
        buffer->DeleteLineAt(0);
    });
    TR_ASSERT(t, ClassOf(buffer, 1, U"Helper") == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

// A scan resumed inside a class body or a typedef'd struct still knows where it is
DLL_EXPORT int test_codeindex_semantic_resume(ITesting *t) {
    std::vector<std::u32string> lines = { U"class Big {" };
    for (int i = 0; i < 20; i++) {
        lines.push_back(U"    int field" + UnicodeHelper::utf8to32(std::to_string(i)) + U";");
    }
    lines.push_back(U"};");
    lines.push_back(U"typedef struct {");
    for (int i = 0; i < 20; i++) {
        lines.push_back(U"    int part" + UnicodeHelper::utf8to32(std::to_string(i)) + U";");
    }
    lines.push_back(U"} Packet_t;");
    lines.push_back(U"void Use() { added = 1; extra = 2; Packet_t p; }");
    auto buffer = CPPBuffer(lines);
    auto idxUse = buffer->NumLines() - 1;
    TR_ASSERT(t, ClassOf(buffer, idxUse, U"Packet_t") == kLanguageTokenClass::kUserType);

    // Mid-class: a new member
    EditRegion(buffer, 10, 11, [&buffer]() {
        buffer->Insert(10, U"    int added;");
    });
    idxUse++;
    TR_ASSERT(t, ClassOf(buffer, idxUse, U"added") == kLanguageTokenClass::kMember);

    // Mid-struct: a new member, and the typedef name at the end is still the type
    EditRegion(buffer, 33, 34, [&buffer]() {
        buffer->Insert(33, U"    int extra;");
    });
    idxUse++;
    TR_ASSERT(t, ClassOf(buffer, idxUse, U"extra") == kLanguageTokenClass::kMember);
    TR_ASSERT(t, ClassOf(buffer, idxUse, U"Packet_t") == kLanguageTokenClass::kUserType);
    return kTR_Pass;
}

// After a series of edits the names are what a fresh parse of the result gives
DLL_EXPORT int test_codeindex_semantic_incremental(ITesting *t) {
    auto buffer = CPPBuffer({
        U"#define LIMIT 4",
        U"namespace app {",
        U"    class Item {",
        U"    public:",
        U"        void Update();",
        U"        int counter;",
        U"    };",
        U"    void Item::Update() {",
        U"        counter = LIMIT;",
        U"    }",
        U"}",
        U"void Run() { app::Item item; item.Update(); item.Reset(); total = 0; }",
    });
    // Rename a method, add one, add a member, drop the macro
    EditRegion(buffer, 4, 5, [&buffer]() {
        auto line = buffer->LineAt(4);
        line->Delete(0, (int)line->Length());
        line->Append(std::u32string(U"        void Refresh();"));
    });
    EditRegion(buffer, 5, 6, [&buffer]() {
        buffer->Insert(5, U"        void Reset();");
    });
    EditRegion(buffer, 7, 8, [&buffer]() {
        buffer->Insert(7, U"        int total;");
    });
    EditRegion(buffer, 0, 1, [&buffer]() {
        buffer->DeleteLineAt(0);
    });

    auto fresh = CPPBuffer(BufferLines(buffer));
    TR_ASSERT(t, fresh->NumLines() == buffer->NumLines());
    for (size_t idxLine = 0; idxLine < buffer->NumLines(); idxLine++) {
        auto &attributes = buffer->LineAt(idxLine)->Attributes();
        auto &freshAttributes = fresh->LineAt(idxLine)->Attributes();
        TR_ASSERT(t, attributes.size() == freshAttributes.size());
        for (size_t i = 0; i < attributes.size(); i++) {
            TR_ASSERT(t, attributes[i].tokenClass == freshAttributes[i].tokenClass);
        }
    }
    auto idxRun = buffer->NumLines() - 1;
    TR_ASSERT(t, ClassOf(buffer, idxRun, U"Reset") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassOf(buffer, idxRun, U"total") == kLanguageTokenClass::kMember);
    TR_ASSERT(t, ClassOf(buffer, idxRun, U"Update") == kLanguageTokenClass::kFunction);   // the definition still declares it
    return kTR_Pass;
}

// Step 2 - the in-memory index and its query (§4.3, §4.5), the background indexer (§4.4)

static CodeSymbol MakeSymbol(const std::u32string &name, kSymbolKind kind, const std::u32string &scope = U"", uint32_t line = 0) {
    CodeSymbol symbol;
    symbol.name = name;
    symbol.qualifiedName = scope.empty() ? name : (scope + U"::" + name);
    symbol.kind = kind;
    symbol.line = line;
    symbol.isDefinition = true;
    return symbol;
}

static FileStructure MakeFile(std::vector<CodeSymbol> symbols) {
    FileStructure structure;
    structure.symbols = std::move(symbols);
    return structure;
}

static std::vector<std::u32string> HitNames(const std::vector<CodeIndex::Hit> &hits) {
    std::vector<std::u32string> names;
    for (auto &hit : hits) {
        names.push_back(hit.name);
    }
    return names;
}

DLL_EXPORT int test_codeindex_store_matchquality(ITesting *t) {
    using Quality = CodeIndex::kMatchQuality;
    auto match = [](const std::u32string &query, const std::u32string &name) {
        std::u32string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), [](char32_t ch) {
            return ((ch >= U'A') && (ch <= U'Z')) ? (ch + 32) : ch;
        });
        return CodeIndex::MatchName(query, name, lower);
    };
    TR_ASSERT(t, match(U"searchfor", U"SearchFor") == Quality::kExact);
    TR_ASSERT(t, match(U"sea", U"SearchFor") == Quality::kPrefix);
    TR_ASSERT(t, match(U"sf", U"SearchFor") == Quality::kWordStarts);
    TR_ASSERT(t, match(U"sefo", U"SearchFor") == Quality::kWordStarts);
    TR_ASSERT(t, match(U"ss", U"HTTPServerState") == Quality::kWordStarts);
    TR_ASSERT(t, match(U"ws", U"word_start") == Quality::kWordStarts);
    TR_ASSERT(t, match(U"arch", U"SearchFor") == Quality::kSubstring);
    TR_ASSERT(t, match(U"srch", U"SearchFor") == Quality::kSubsequence);
    TR_ASSERT(t, match(U"xyz", U"SearchFor") == Quality::kNone);
    TR_ASSERT(t, match(U"searchforx", U"SearchFor") == Quality::kNone);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_store_ranking(ITesting *t) {
    auto index = CodeIndex::Create();
    index->ReplaceFile("/p/a/one.h", MakeFile({
        MakeSymbol(U"Search", kSymbolKind::kClass),
        MakeSymbol(U"SearchFor", kSymbolKind::kMethod, U"Document"),
        MakeSymbol(U"Search_MAX", kSymbolKind::kMacro),
        MakeSymbol(U"searchState", kSymbolKind::kMember, U"Document"),
        MakeSymbol(U"Research", kSymbolKind::kFunction),
    }));

    // Quality first: exact, then the prefixes - a macro is a better match than a substring, even of a function
    auto hits = HitNames(index->Query(U"search"));
    TR_ASSERT(t, hits.size() == 5);
    TR_ASSERT(t, hits[0] == U"Search");
    TR_ASSERT(t, hits[1] == U"SearchFor");
    TR_ASSERT(t, hits[2] == U"searchState");
    TR_ASSERT(t, hits[3] == U"Search_MAX");
    TR_ASSERT(t, hits[4] == U"Research");

    // Same name and kind: the active file, then its folder, then the rest - and a definition before a declaration
    index->ReplaceFile("/p/b/two.cpp", MakeFile({MakeSymbol(U"Update", kSymbolKind::kFunction)}));
    index->ReplaceFile("/p/a/near.cpp", MakeFile({MakeSymbol(U"Update", kSymbolKind::kFunction)}));
    index->ReplaceFile("/p/a/sub/deep.cpp", MakeFile({MakeSymbol(U"Update", kSymbolKind::kFunction)}));
    auto declaration = MakeSymbol(U"Update", kSymbolKind::kFunction);
    declaration.isDefinition = false;
    index->ReplaceFile("/p/a/active.cpp", MakeFile({declaration}));
    index->ReplaceFile("/p/c/three.cpp", MakeFile({declaration}));

    CodeIndex::QueryContext context;
    context.activePath = "/p/a/active.cpp";
    auto updates = index->Query(U"Update", context);
    TR_ASSERT(t, updates.size() == 5);
    TR_ASSERT(t, updates[0].path == "/p/a/active.cpp");
    TR_ASSERT(t, updates[1].path == "/p/a/near.cpp");
    // A sub folder isn't the same folder; among the far ones the definitions come first
    TR_ASSERT(t, updates[4].path == "/p/c/three.cpp");

    context.maxHits = 2;
    TR_ASSERT(t, index->Query(U"Update", context).size() == 2);
    TR_ASSERT(t, index->Query(U"").empty());
    TR_ASSERT(t, index->Query(U"::").empty());
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_store_scope(ITesting *t) {
    auto index = CodeIndex::Create();
    index->ReplaceFile("/p/one.h", MakeFile({
        MakeSymbol(U"Search", kSymbolKind::kMethod, U"gedit::Document"),
        MakeSymbol(U"Search", kSymbolKind::kMethod, U"gedit::Workspace"),
        MakeSymbol(U"Save", kSymbolKind::kMethod, U"gedit::Document"),
    }));

    auto hits = index->Query(U"Document::Sea");
    TR_ASSERT(t, hits.size() == 1);
    TR_ASSERT(t, hits[0].qualifiedName == U"gedit::Document::Search");
    // Case-insensitive, and the scope alone lists it
    TR_ASSERT(t, index->Query(U"document::").size() == 2);
    TR_ASSERT(t, index->Query(U"Nothing::Sea").empty());
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_store_replace(ITesting *t) {
    auto index = CodeIndex::Create();
    index->ReplaceFile("/p/one.h", MakeFile({MakeSymbol(U"Alpha", kSymbolKind::kClass), MakeSymbol(U"Alpha", kSymbolKind::kFunction), MakeSymbol(U"Beta", kSymbolKind::kClass)}));
    index->ReplaceFile("/p/two.h", MakeFile({MakeSymbol(U"Beta", kSymbolKind::kClass)}));
    TR_ASSERT(t, index->GetStats().numSymbols == 4);
    TR_ASSERT(t, index->GetStats().numNames == 2);

    // Replacing drops what the file had, not what others have
    index->ReplaceFile("/p/one.h", MakeFile({MakeSymbol(U"Gamma", kSymbolKind::kClass)}));
    TR_ASSERT(t, index->Query(U"Alpha").empty());
    TR_ASSERT(t, index->Query(U"Beta").size() == 1);
    TR_ASSERT(t, index->Query(U"Gamma").size() == 1);
    TR_ASSERT(t, index->GetStats().numNames == 2);

    index->RemoveFile("/p/two.h");
    TR_ASSERT(t, index->Query(U"Beta").empty());
    TR_ASSERT(t, index->GetStats().numFiles == 1);

    // Repeated edits must not grow the tables: a removed name's entry is reused
    auto numBytes = index->GetStats().numBytes;
    for (int i = 0; i < 100; i++) {
        index->ReplaceFile("/p/one.h", MakeFile({MakeSymbol(U"Temporary" + std::u32string(1, (char32_t)(U'a' + (i % 26))), kSymbolKind::kClass)}));
    }
    TR_ASSERT(t, index->GetStats().numNames == 1);
    TR_ASSERT(t, index->GetStats().numBytes <= numBytes + 256);

    // A nameless symbol isn't searchable
    index->ReplaceFile("/p/one.h", MakeFile({MakeSymbol(U"", kSymbolKind::kStruct)}));
    TR_ASSERT(t, index->GetStats().numSymbols == 0);
    return kTR_Pass;
}

static std::filesystem::path IndexerFolder(const std::string &name) {
    auto root = std::filesystem::temp_directory_path() / ("goatedit_indexer_" + name);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

static std::filesystem::path fs_canonical_root(const std::string &name) {
    auto root = IndexerFolder(name);
    return std::filesystem::canonical(root);
}

static void WriteTextFile(const std::filesystem::path &path, const std::string &text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << text;
}

static Indexer::Ref MakeIndexer(const CodeIndex::Ref &index, uintmax_t maxFileSize = 4 * 1024 * 1024) {
    Indexer::Languages languages;
    auto language = Editor::Instance().GetLanguageForExtension(".cpp");
    languages[".cpp"] = language;
    languages[".h"] = language;
    Indexer::Options options;
    options.maxFileSize = maxFileSize;
    return Indexer::Create(index, languages, options);
}

DLL_EXPORT int test_codeindex_indexer_folder(ITesting *t) {
    auto root = IndexerFolder("folder");
    WriteTextFile(root / "a.h", "class Alpha { public: void Run(); };\n");
    WriteTextFile(root / "sub/deep/b.cpp", "namespace n { void Bravo() {} }\n");
    WriteTextFile(root / "skipped/c.h", "class Charlie {};\n");
    WriteTextFile(root / "notes.txt", "class Delta {};\n");
    WriteTextFile(root / "big.h", "class Echo {};\n" + std::string(4096, ' '));

    auto index = CodeIndex::Create();
    auto indexer = MakeIndexer(index, 1024);
    indexer->AddRoot(root, ExclusionRules::Create(root, {"skipped"}, false));
    indexer->WaitIdle();

    auto alpha = index->Query(U"Alpha");
    TR_ASSERT(t, alpha.size() == 1);
    TR_ASSERT(t, alpha[0].path == (root / "a.h").string());
    TR_ASSERT(t, alpha[0].kind == kSymbolKind::kClass);
    TR_ASSERT(t, index->Query(U"Run").size() == 1);
    TR_ASSERT(t, index->Query(U"n::Bravo").size() == 1);
    TR_ASSERT(t, index->Query(U"Charlie").empty());     // excluded folder
    TR_ASSERT(t, index->Query(U"Delta").empty());       // no language for it
    TR_ASSERT(t, index->Query(U"Echo").empty());        // over the size cap

    auto progress = indexer->GetProgress();
    TR_ASSERT(t, progress.isIdle);
    TR_ASSERT(t, progress.numDone == progress.numTotal);
    std::filesystem::remove_all(root);
    return kTR_Pass;
}

// Open bug #15: a bad UTF-8 byte must not cost the rest of the file (old C with Latin-1 comments)
DLL_EXPORT int test_codeindex_indexer_latin1(ITesting *t) {
    auto root = IndexerFolder("latin1");
    WriteTextFile(root / "old.h", "struct Before { int a; };\n/* r\xe4ksm\xf6rg\xe5s */\nstruct After { int b; };\n/* \xe2\x82 */ struct Last {};\n");

    auto index = CodeIndex::Create();
    auto indexer = MakeIndexer(index);
    indexer->AddRoot(root);
    indexer->WaitIdle();

    TR_ASSERT(t, index->Query(U"Before").size() == 1);
    TR_ASSERT(t, index->Query(U"After").size() == 1);
    TR_ASSERT(t, index->Query(U"Last").size() == 1);
    std::filesystem::remove_all(root);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_indexer_change(ITesting *t) {
    auto root = IndexerFolder("change");
    auto path = root / "a.h";
    WriteTextFile(path, "class Before {};\n");

    auto index = CodeIndex::Create();
    auto indexer = MakeIndexer(index);
    indexer->AddRoot(root);
    indexer->WaitIdle();
    TR_ASSERT(t, index->Query(U"Before").size() == 1);

    WriteTextFile(path, "class After {};\n");
    indexer->Enqueue(path.string());
    indexer->WaitIdle();
    TR_ASSERT(t, index->Query(U"Before").empty());
    TR_ASSERT(t, index->Query(U"After").size() == 1);

    // Gone from disk: it leaves the index
    std::filesystem::remove(path);
    indexer->Enqueue(path.string());
    indexer->WaitIdle();
    TR_ASSERT(t, index->Query(U"After").empty());
    std::filesystem::remove_all(root);
    return kTR_Pass;
}

// A search runs while the index fills, and Stop in the middle of it leaves nothing behind
DLL_EXPORT int test_codeindex_indexer_concurrent(ITesting *t) {
    auto root = IndexerFolder("concurrent");
    for (int i = 0; i < 200; i++) {
        auto name = std::to_string(i);
        WriteTextFile(root / ("f" + name + ".h"), "class Shared" + name + " { void Method(); };\n");
    }
    auto index = CodeIndex::Create();
    auto indexer = MakeIndexer(index);
    indexer->AddRoot(root);

    size_t lastCount = 0;
    while (!indexer->GetProgress().isIdle || (lastCount == 0)) {
        lastCount = index->Query(U"Shared").size();
    }
    indexer->WaitIdle();
    TR_ASSERT(t, index->Query(U"Shared").size() == 200);

    auto stopped = MakeIndexer(CodeIndex::Create());
    stopped->AddRoot(root);
    stopped->Stop();
    stopped->Enqueue((root / "f0.h").string());     // ignored once stopped
    TR_ASSERT(t, stopped->GetProgress().isIdle);
    std::filesystem::remove_all(root);
    return kTR_Pass;
}

// Go to symbol (§4.6)

namespace {
    struct SymbolModalFixture {
        SymbolModalFixture() {
            rootView.Initialize();
            index->ReplaceFile("/p/src/doc.cpp", MakeFile({MakeSymbol(U"Save", kSymbolKind::kMethod, U"gedit::Document", 2)}));
            index->ReplaceFile("/p/src/other.cpp", MakeFile({MakeSymbol(U"SaveAll", kSymbolKind::kFunction, U"", 7)}));
        }
        SymbolSearchModal *Show(const Document::Ref &document, const std::string &activePath, const std::u32string &query) {
            auto modal = SymbolSearchModal::Create(index, document, activePath, {"/p"});
            auto symbolModal = modal.get();
            rootView.ShowModal(std::move(modal));
            symbolModal->SetQuery(query);
            return symbolModal;
        }
        RootView rootView;
        CodeIndex::Ref index = CodeIndex::Create();
    };

    Document::Ref MakeSymbolDocument() {
        auto document = Document::Create(TextBuffer::CreateEmptyBuffer());
        document->GetTextBuffer()->DeleteLineAt(0);
        for (int i = 0; i < 20; i++) {
            document->GetTextBuffer()->AddLineUTF8(("line " + std::to_string(i)).c_str());
        }
        document->OnViewInit(gedit::Rect(80, 10));
        return document;
    }
}

DLL_EXPORT int test_codeindex_modal_rows(ITesting *t) {
    SymbolModalFixture fixture;
    auto modal = fixture.Show(nullptr, "", U"Save");
    TR_ASSERT(t, modal->GetNumRows() == 2);
    TR_ASSERT(t, modal->GetSelectedRow() == 0);
    TR_ASSERT(t, modal->GetHits()[0].qualifiedName == U"gedit::Document::Save");

    modal->SetQuery(U"nothing here");
    TR_ASSERT(t, modal->GetNumRows() == 0);
    TR_ASSERT(t, modal->GetSelectedRow() == -1);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_modal_location(ITesting *t) {
    CodeIndex::Hit hit;
    hit.path = "/p/src/doc.cpp";
    hit.line = 2;
    TR_ASSERT(t, SymbolSearchModal::HitLocation(hit, {"/other", "/p"}) == "src/doc.cpp:3");
    TR_ASSERT(t, SymbolSearchModal::HitLocation(hit, {"/other"}) == "/p/src/doc.cpp:3");
    TR_ASSERT(t, SymbolSearchModal::HitLocation(hit, {}) == "/p/src/doc.cpp:3");
    return kTR_Pass;
}

// A hit in the active document is previewed, one elsewhere isn't, and Esc undoes the preview
DLL_EXPORT int test_codeindex_modal_preview(ITesting *t) {
    SymbolModalFixture fixture;
    auto document = MakeSymbolDocument();
    document->SetCursorPosition(10, 1);

    auto modal = fixture.Show(document, "/p/src/doc.cpp", U"Save");
    TR_ASSERT(t, modal->GetHits()[0].path == "/p/src/doc.cpp");
    TR_ASSERT(t, document->GetLineCursor().idxActiveLine == 2);

    EditorAction down;
    down.uiAction = kUIAction::kUIActionLineDown;
    fixture.rootView.OnAction(down);
    TR_ASSERT(t, modal->GetHits()[(size_t)modal->GetSelectedRow()].path == "/p/src/other.cpp");
    TR_ASSERT(t, document->GetLineCursor().idxActiveLine == 10);

    EditorAction up;
    up.uiAction = kUIAction::kUIActionLineUp;
    fixture.rootView.OnAction(up);
    TR_ASSERT(t, document->GetLineCursor().idxActiveLine == 2);

    EditorAction cancel;
    cancel.uiAction = kUIAction::kUIActionCloseModal;
    fixture.rootView.OnAction(cancel);
    TR_ASSERT(t, !fixture.rootView.HasModal());
    TR_ASSERT(t, document->GetLineCursor().idxActiveLine == 10);
    return kTR_Pass;
}

// Slice 2: an out-of-line definition is a method when the index knows its class
DLL_EXPORT int test_codeindex_store_outofline(ITesting *t) {
    auto index = CodeIndex::Create();
    index->ReplaceFile("/p/doc.h", MakeFile({MakeSymbol(U"Document", kSymbolKind::kClass, U"gedit")}));
    index->ReplaceFile("/p/doc.cpp", MakeFile({
        MakeSymbol(U"Save", kSymbolKind::kFunction, U"Document"),
        MakeSymbol(U"Load", kSymbolKind::kFunction, U"gedit::Document"),
        MakeSymbol(U"Free", kSymbolKind::kFunction, U"gedit"),
        MakeSymbol(U"Lone", kSymbolKind::kFunction),
    }));
    TR_ASSERT(t, index->Query(U"Save")[0].kind == kSymbolKind::kMethod);
    TR_ASSERT(t, index->Query(U"Load")[0].kind == kSymbolKind::kMethod);
    TR_ASSERT(t, index->Query(U"Free")[0].kind == kSymbolKind::kFunction);      // a namespace is no class
    TR_ASSERT(t, index->Query(U"Lone")[0].kind == kSymbolKind::kFunction);

    // Follows the index: the class goes, and so does what it says about the rest
    index->RemoveFile("/p/doc.h");
    TR_ASSERT(t, index->Query(U"Save")[0].kind == kSymbolKind::kFunction);
    return kTR_Pass;
}

static const CodeSymbol *FindSymbol(const FileStructure &structure, const std::u32string &name) {
    for (auto &symbol : structure.symbols) {
        if (symbol.name == name) {
            return &symbol;
        }
    }
    return nullptr;
}

static void EditLines(const TextBuffer::Ref &buffer, const std::function<void()> &edit) {
    auto mainConfig = Config::Instance()["main"];
    auto wasThreaded = mainConfig.GetBool("threaded_syntaxparser", false);
    mainConfig.SetBool("threaded_syntaxparser", false);
    edit();
    mainConfig.SetBool("threaded_syntaxparser", wasThreaded);
}

// The line numbers are where the lines are now - not where they were scanned
DLL_EXPORT int test_codeindex_buffer_lines(ITesting *t) {
    auto buffer = CPPBuffer({
        U"class Alpha {",
        U"};",
        U"void Bravo();",
    });
    auto structure = buffer->CollectStructure();
    TR_ASSERT(t, FindSymbol(structure, U"Bravo") != nullptr);
    TR_ASSERT(t, FindSymbol(structure, U"Bravo")->line == 2);

    EditLines(buffer, [&buffer]() {
        buffer->Insert(0, U"// a comment");
        buffer->Insert(0, U"// and one more");
        buffer->ReparseRegion(0, 2);
    });
    structure = buffer->CollectStructure();
    TR_ASSERT(t, FindSymbol(structure, U"Alpha")->line == 2);
    TR_ASSERT(t, FindSymbol(structure, U"Bravo")->line == 4);

    // A deleted line takes its symbols along
    EditLines(buffer, [&buffer]() {
        buffer->DeleteLineAt(4);
        buffer->ReparseRegion(3, 4);
    });
    structure = buffer->CollectStructure();
    TR_ASSERT(t, FindSymbol(structure, U"Bravo") == nullptr);
    TR_ASSERT(t, FindSymbol(structure, U"Alpha") != nullptr);
    return kTR_Pass;
}

// A scan that resumes mid-file still knows the scope it is in
DLL_EXPORT int test_codeindex_buffer_region(ITesting *t) {
    auto buffer = CPPBuffer({
        U"namespace outer {",
        U"class Alpha {",
        U"public:",
        U"    void First();",
        U"};",
        U"}",
    });
    EditLines(buffer, [&buffer]() {
        buffer->Insert(4, U"    void Second();");
        buffer->ReparseRegion(4, 5);
    });
    auto structure = buffer->CollectStructure();
    auto second = FindSymbol(structure, U"Second");
    TR_ASSERT(t, second != nullptr);
    TR_ASSERT(t, second->qualifiedName == U"outer::Alpha::Second");
    TR_ASSERT(t, second->kind == kSymbolKind::kMethod);
    TR_ASSERT(t, second->line == 4);
    TR_ASSERT(t, FindSymbol(structure, U"First") != nullptr);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_buffer_handler(ITesting *t) {
    auto buffer = CPPBuffer({U"class Alpha {};"});
    size_t numCalls = 0;
    bool hasBravo = false;
    buffer->SetStructureHandler([&numCalls, &hasBravo](const FileStructure &structure) {
        numCalls++;
        hasBravo = FindSymbol(structure, U"Bravo") != nullptr;
    });
    EditLines(buffer, [&buffer]() {
        buffer->Insert(1, U"class Bravo {};");
        buffer->ReparseRegion(1, 2);
    });
    TR_ASSERT(t, numCalls == 1);
    TR_ASSERT(t, hasBravo);

    buffer->SetStructureHandler(nullptr);
    EditLines(buffer, [&buffer]() {
        buffer->Insert(2, U"class Charlie {};");
        buffer->ReparseRegion(2, 3);
    });
    TR_ASSERT(t, numCalls == 1);
    return kTR_Pass;
}

// While a buffer owns the file, what is on disk may be older: unsaved work must not be read over
DLL_EXPORT int test_codeindex_indexer_buffer(ITesting *t) {
    auto root = IndexerFolder("buffer");
    auto path = root / "a.h";
    WriteTextFile(path, "class OnDisk {};\n");

    auto index = CodeIndex::Create();
    auto indexer = MakeIndexer(index);
    indexer->AddRoot(root);
    indexer->WaitIdle();
    TR_ASSERT(t, index->Query(U"OnDisk").size() == 1);

    indexer->ReplaceFromBuffer(path.string(), MakeFile({MakeSymbol(U"Unsaved", kSymbolKind::kClass)}));
    TR_ASSERT(t, index->Query(U"Unsaved").size() == 1);
    TR_ASSERT(t, index->Query(U"OnDisk").empty());

    indexer->Enqueue(path.string());
    indexer->WaitIdle();
    TR_ASSERT(t, index->Query(U"Unsaved").size() == 1);
    TR_ASSERT(t, index->Query(U"OnDisk").empty());

    // The file vanishing from disk leaves the buffer's symbols alone too - the document is still open
    std::filesystem::remove(path);
    indexer->Enqueue(path.string());
    indexer->WaitIdle();
    TR_ASSERT(t, index->Query(U"Unsaved").size() == 1);

    // Closed without saving: back to the disk
    WriteTextFile(path, "class OnDisk {};\n");
    indexer->ReleaseFromBuffer(path.string());
    indexer->WaitIdle();
    TR_ASSERT(t, index->Query(U"Unsaved").empty());
    TR_ASSERT(t, index->Query(U"OnDisk").size() == 1);
    std::filesystem::remove_all(root);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_indexer_changedfolder(ITesting *t) {
    auto root = IndexerFolder("changedfolder");
    WriteTextFile(root / "same.h", "class Same {};\n");
    WriteTextFile(root / "changes.h", "class Before {};\n");
    WriteTextFile(root / "goes.h", "class Goes {};\n");
    WriteTextFile(root / "sub/other.h", "class Other {};\n");

    auto index = CodeIndex::Create();
    auto indexer = MakeIndexer(index);
    indexer->AddRoot(root);
    indexer->WaitIdle();
    auto numRead = indexer->GetProgress().numTotal;
    TR_ASSERT(t, numRead == 4);

    WriteTextFile(root / "changes.h", "class AfterTheChange {};\n");
    WriteTextFile(root / "comes.h", "class Comes {};\n");
    std::filesystem::remove(root / "goes.h");
    indexer->EnqueueFolder(root);
    indexer->WaitIdle();

    TR_ASSERT(t, index->Query(U"Before").empty());
    TR_ASSERT(t, index->Query(U"AfterTheChange").size() == 1);
    TR_ASSERT(t, index->Query(U"Comes").size() == 1);
    TR_ASSERT(t, index->Query(U"Goes").empty());
    TR_ASSERT(t, index->Query(U"Same").size() == 1);
    // Only those three were read - not the unchanged one, not the sub folder
    TR_ASSERT(t, indexer->GetProgress().numTotal == numRead + 3);

    // Nothing changed: nothing read
    indexer->EnqueueFolder(root);
    indexer->RescanRoots();
    indexer->WaitIdle();
    TR_ASSERT(t, indexer->GetProgress().numTotal == numRead + 3);

    // A folder that disappears takes what is below it along
    std::filesystem::remove_all(root / "sub");
    indexer->EnqueueFolder(root / "sub");
    indexer->WaitIdle();
    TR_ASSERT(t, index->Query(U"Other").empty());
    std::filesystem::remove_all(root);
    return kTR_Pass;
}

// Events were lost: a rescan finds what changed anywhere below the root
DLL_EXPORT int test_codeindex_indexer_goneroot(ITesting *t) {
    auto root = IndexerFolder("rescan");
    WriteTextFile(root / "deep/er/a.h", "class Stays {};\n");
    WriteTextFile(root / "deep/b.h", "class Vanishes {};\n");

    auto index = CodeIndex::Create();
    auto indexer = MakeIndexer(index);
    indexer->AddRoot(root);
    indexer->WaitIdle();

    std::filesystem::remove(root / "deep/b.h");
    WriteTextFile(root / "deep/er/c.h", "class Arrives {};\n");
    indexer->RescanRoots();
    indexer->WaitIdle();

    TR_ASSERT(t, index->Query(U"Stays").size() == 1);
    TR_ASSERT(t, index->Query(U"Vanishes").empty());
    TR_ASSERT(t, index->Query(U"Arrives").size() == 1);
    std::filesystem::remove_all(root);
    return kTR_Pass;
}

// Upper case is the body, for functions and methods alike
DLL_EXPORT int test_codeindex_modal_markers(ITesting *t) {
    CodeIndex::Hit hit;
    hit.kind = kSymbolKind::kMethod;
    hit.isDefinition = false;
    TR_ASSERT(t, SymbolSearchModal::KindMarker(hit) == U'm');
    hit.isDefinition = true;
    TR_ASSERT(t, SymbolSearchModal::KindMarker(hit) == U'M');
    hit.kind = kSymbolKind::kFunction;
    TR_ASSERT(t, SymbolSearchModal::KindMarker(hit) == U'F');
    hit.isDefinition = false;
    TR_ASSERT(t, SymbolSearchModal::KindMarker(hit) == U'f');
    hit.kind = kSymbolKind::kClass;
    TR_ASSERT(t, SymbolSearchModal::KindMarker(hit) == U'c');
    return kTR_Pass;
}

// The real path: the parse thread publishes once, after a pause - not per edit
DLL_EXPORT int test_codeindex_buffer_pause(ITesting *t) {
    auto mainConfig = Config::Instance()["main"];
    auto wasThreaded = mainConfig.GetBool("threaded_syntaxparser", false);
    mainConfig.SetBool("threaded_syntaxparser", true);

    auto buffer = CPPBuffer({U"class Alpha {};"});
    std::atomic_int numCalls = 0;
    std::atomic_bool hasLast = false;
    buffer->SetStructureHandler([&numCalls, &hasLast](const FileStructure &structure) {
        numCalls++;
        hasLast = FindSymbol(structure, U"Last") != nullptr;
    });
    for (int i = 0; i < 5; i++) {
        buffer->Insert(1, U"// typing " + std::u32string(1, (char32_t)(U'0' + i)));
        buffer->ReparseRegion(1, 2);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    buffer->Insert(1, U"class Last {};");
    buffer->ReparseRegion(1, 2);
    for (int i = 0; (i < 200) && !hasLast; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    TR_ASSERT(t, hasLast);
    TR_ASSERT(t, numCalls == 1);

    buffer->SetStructureHandler(nullptr);
    buffer->Close();
    mainConfig.SetBool("threaded_syntaxparser", wasThreaded);
    return kTR_Pass;
}

// '.gitignore' reaches every way a file can get in: the walk, a dirty folder, a document closing
DLL_EXPORT int test_codeindex_indexer_gitignore(ITesting *t) {
    auto root = fs_canonical_root("gitignore");
    WriteTextFile(root / ".gitignore", "/ext/\nbuild/\ngen_*.h\n");
    WriteTextFile(root / "main.h", "class Kept {};\n");
    WriteTextFile(root / "src/ext/dep.h", "class Vendored {};\n");
    WriteTextFile(root / "ext/dep.h", "class FetchedDep {};\n");
    WriteTextFile(root / "src/build/x.h", "class BuildOutput {};\n");
    WriteTextFile(root / "gen_a.h", "class Generated {};\n");

    auto index = CodeIndex::Create();
    auto indexer = MakeIndexer(index);
    indexer->AddRoot(root, ExclusionRules::Create(root, {}, true));
    indexer->WaitIdle();

    TR_ASSERT(t, index->Query(U"Kept").size() == 1);
    TR_ASSERT(t, index->Query(U"Vendored").size() == 1);    // '/ext/' is the top-level one only
    TR_ASSERT(t, index->Query(U"FetchedDep").empty());
    TR_ASSERT(t, index->Query(U"BuildOutput").empty());
    TR_ASSERT(t, index->Query(U"Generated").empty());

    // A dirty folder lists an ignored file too
    WriteTextFile(root / "gen_b.h", "class AlsoGenerated {};\n");
    indexer->EnqueueFolder(root);
    indexer->WaitIdle();
    TR_ASSERT(t, index->Query(U"AlsoGenerated").empty());

    // Opened from an ignored folder: searchable while it is open, gone when it closes
    auto ignoredPath = (root / "ext/dep.h").string();
    indexer->ReplaceFromBuffer(ignoredPath, MakeFile({MakeSymbol(U"FetchedDep", kSymbolKind::kClass)}));
    TR_ASSERT(t, index->Query(U"FetchedDep").size() == 1);
    indexer->ReleaseFromBuffer(ignoredPath);
    indexer->WaitIdle();
    TR_ASSERT(t, index->Query(U"FetchedDep").empty());
    std::filesystem::remove_all(root);
    return kTR_Pass;
}

// Slice 3: one meaning across the workspace, or none
DLL_EXPORT int test_codeindex_workspace_meanings(ITesting *t) {
    auto index = CodeIndex::Create();
    index->ReplaceFile("/a.h", MakeFile({
        MakeSymbol(U"Item", kSymbolKind::kClass),
        MakeSymbol(U"Item", kSymbolKind::kMethod, U"Item"),         // its constructor
        MakeSymbol(U"Close", kSymbolKind::kMethod, U"Item"),
        MakeSymbol(U"Both", kSymbolKind::kMember, U"Item"),
        MakeSymbol(U"Shared", kSymbolKind::kClass),
        MakeSymbol(U"Global", kSymbolKind::kVariable),
        MakeSymbol(U"kValue", kSymbolKind::kEnumerator),
    }));
    index->ReplaceFile("/b.h", MakeFile({
        MakeSymbol(U"Item", kSymbolKind::kFunction, U"Item"),       // out-of-line constructor
        MakeSymbol(U"Close", kSymbolKind::kFunction, U"Other"),     // same meaning in another class
        MakeSymbol(U"Both", kSymbolKind::kMethod, U"Other"),
        MakeSymbol(U"Shared", kSymbolKind::kVariable),
    }));
    auto names = index->GetWorkspaceNames();
    TR_ASSERT(t, names->Lookup(U"Item") == kLanguageTokenClass::kUserType);
    TR_ASSERT(t, names->Lookup(U"Close") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, names->Lookup(U"kValue") == kLanguageTokenClass::kEnumValue);
    TR_ASSERT(t, names->Lookup(U"Both") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, names->Lookup(U"Shared") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, names->Lookup(U"Global") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, names->Lookup(U"Unknown") == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

// The index has no locals - a function must be called, a member reached through an object
DLL_EXPORT int test_codeindex_workspace_context(ITesting *t) {
    WorkspaceNames names;
    names.Add(U"path", kLanguageTokenClass::kMember);
    names.Add(U"Close", kLanguageTokenClass::kFunction);
    names.Add(U"Item", kLanguageTokenClass::kUserType);

    auto lookup = [&names](const std::u32string &text, const std::u32string &name) {
        return names.LookupAt(text, text.find(name), name);
    };
    TR_ASSERT(t, lookup(U"auto path = 1;", U"path") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, lookup(U"path", U"path") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, lookup(U"a.path = 1;", U"path") == kLanguageTokenClass::kMember);
    TR_ASSERT(t, lookup(U"a->path = 1;", U"path") == kLanguageTokenClass::kMember);
    TR_ASSERT(t, lookup(U"a -> path = 1;", U"path") == kLanguageTokenClass::kMember);
    TR_ASSERT(t, lookup(U"a > path", U"path") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, lookup(U"void f(Args... path);", U"path") == kLanguageTokenClass::kRegular);

    TR_ASSERT(t, lookup(U"Close();", U"Close") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, lookup(U"Close ();", U"Close") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, lookup(U"a->Close(1);", U"Close") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, lookup(U"auto Close = 1;", U"Close") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, lookup(U"Close", U"Close") == kLanguageTokenClass::kRegular);

    TR_ASSERT(t, lookup(U"Item item;", U"Item") == kLanguageTokenClass::kUserType);
    TR_ASSERT(t, lookup(U"return Item;", U"Item") == kLanguageTokenClass::kUserType);

    // Lower case can be a local: it needs to stand where a local can't
    names.Add(U"other", kLanguageTokenClass::kUserType);
    names.Add(U"gedit", kLanguageTokenClass::kNamespace);
    names.Add(U"running", kLanguageTokenClass::kEnumValue);
    names.Add(U"log_e", kLanguageTokenClass::kMacro);
    names.Add(U"kState", kLanguageTokenClass::kEnumValue);
    TR_ASSERT(t, lookup(U"a == other;", U"other") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, lookup(U"other.x = 1;", U"other") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, lookup(U"foo(other, 1);", U"other") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, lookup(U"return other", U"other") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, lookup(U"other value;", U"other") == kLanguageTokenClass::kUserType);
    TR_ASSERT(t, lookup(U"other *value;", U"other") == kLanguageTokenClass::kUserType);
    TR_ASSERT(t, lookup(U"other<int> value;", U"other") == kLanguageTokenClass::kUserType);
    TR_ASSERT(t, lookup(U"gedit::Editor", U"gedit") == kLanguageTokenClass::kNamespace);
    TR_ASSERT(t, lookup(U"gedit;", U"gedit") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, lookup(U"x = running;", U"running") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, lookup(U"x = kind::running;", U"running") == kLanguageTokenClass::kEnumValue);
    TR_ASSERT(t, lookup(U"log_e(1);", U"log_e") == kLanguageTokenClass::kMacro);
    TR_ASSERT(t, lookup(U"auto log_e = 1;", U"log_e") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, lookup(U"x = kState;", U"kState") == kLanguageTokenClass::kEnumValue);
    return kTR_Pass;
}

// A buffer compares pointers: a new one only when a meaning changed
DLL_EXPORT int test_codeindex_workspace_snapshot(ITesting *t) {
    auto index = CodeIndex::Create();
    index->ReplaceFile("/a.h", MakeFile({MakeSymbol(U"Item", kSymbolKind::kClass)}));
    auto names = index->GetWorkspaceNames();
    TR_ASSERT(t, names == index->GetWorkspaceNames());

    // Moved a line down: the file changed, no name did
    index->ReplaceFile("/a.h", MakeFile({MakeSymbol(U"Item", kSymbolKind::kClass, U"", 7)}));
    TR_ASSERT(t, names == index->GetWorkspaceNames());

    index->ReplaceFile("/a.h", MakeFile({MakeSymbol(U"Item", kSymbolKind::kClass), MakeSymbol(U"Fresh", kSymbolKind::kClass)}));
    auto withFresh = index->GetWorkspaceNames();
    TR_ASSERT(t, names != withFresh);
    TR_ASSERT(t, withFresh->Lookup(U"Fresh") == kLanguageTokenClass::kUserType);
    // What a reader holds doesn't change under it
    TR_ASSERT(t, names->Lookup(U"Fresh") == kLanguageTokenClass::kRegular);

    index->RemoveFile("/a.h");
    TR_ASSERT(t, index->GetWorkspaceNames()->Lookup(U"Item") == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

static WorkspaceNames::Ref MakeWorkspaceNames(const std::vector<std::pair<std::u32string, kLanguageTokenClass>> &entries) {
    auto names = std::make_shared<WorkspaceNames>();
    for (auto &[name, tokenClass] : entries) {
        names->Add(name, tokenClass);
    }
    return names;
}

DLL_EXPORT int test_codeindex_workspace_buffer(ITesting *t) {
    auto buffer = CPPBuffer({
        U"ViewBase view;",
        U"view.bounds = 1;",
        U"auto bounds = view.Size();",
        U"auto Size = kAlpha;",
    });
    TR_ASSERT(t, ClassOf(buffer, 0, U"ViewBase") == kLanguageTokenClass::kRegular);

    auto names = MakeWorkspaceNames({
        {U"ViewBase", kLanguageTokenClass::kUserType},
        {U"bounds", kLanguageTokenClass::kMember},
        {U"Size", kLanguageTokenClass::kFunction},
        {U"kAlpha", kLanguageTokenClass::kEnumValue},
    });
    buffer->SetWorkspaceNamesSource([&names]() -> WorkspaceNames::Ref {
        return names;
    });
    TR_ASSERT(t, ClassOf(buffer, 0, U"ViewBase") == kLanguageTokenClass::kUserType);
    TR_ASSERT(t, ClassOf(buffer, 1, U"bounds") == kLanguageTokenClass::kMember);
    TR_ASSERT(t, ClassOf(buffer, 2, U"bounds") == kLanguageTokenClass::kRegular);       // a local
    TR_ASSERT(t, ClassOf(buffer, 2, U"Size") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassOf(buffer, 3, U"Size") == kLanguageTokenClass::kRegular);         // a local
    TR_ASSERT(t, ClassOf(buffer, 3, U"kAlpha") == kLanguageTokenClass::kEnumValue);

    // Not code, though it looks like a member access
    auto include = CPPBuffer({U"#include <sys/types.h>", U"#include \"types.h\""});
    auto withH = MakeWorkspaceNames({{U"h", kLanguageTokenClass::kMember}});
    include->SetWorkspaceNamesSource([&withH]() -> WorkspaceNames::Ref {
        return withH;
    });
    TR_ASSERT(t, ClassOf(include, 0, U"h") != kLanguageTokenClass::kMember);
    TR_ASSERT(t, ClassOf(include, 1, U"h") != kLanguageTokenClass::kMember);
    include->SetWorkspaceNamesSource(nullptr);

    // A full parse keeps them; a name that leaves the workspace leaves the colour
    buffer->Reparse();
    TR_ASSERT(t, ClassOf(buffer, 0, U"ViewBase") == kLanguageTokenClass::kUserType);
    names = MakeWorkspaceNames({{U"kAlpha", kLanguageTokenClass::kEnumValue}});
    buffer->Reparse();
    TR_ASSERT(t, ClassOf(buffer, 0, U"ViewBase") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, ClassOf(buffer, 3, U"kAlpha") == kLanguageTokenClass::kEnumValue);
    buffer->SetWorkspaceNamesSource(nullptr);
    return kTR_Pass;
}

// 'Mixed' means two things in this file - that isn't an opening for the workspace's opinion
DLL_EXPORT int test_codeindex_workspace_fileownfirst(ITesting *t) {
    auto buffer = CPPBuffer({
        U"#define Mixed 1",
        U"#define Local 2",
        U"struct Holder {",
        U"    int Mixed;",
        U"};",
        U"int value = Mixed + Local;",
    });
    auto names = MakeWorkspaceNames({
        {U"Mixed", kLanguageTokenClass::kUserType},
        {U"Local", kLanguageTokenClass::kUserType},
    });
    buffer->SetWorkspaceNamesSource([&names]() -> WorkspaceNames::Ref {
        return names;
    });
    TR_ASSERT(t, ClassOf(buffer, 5, U"Mixed") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, ClassOf(buffer, 5, U"Local") == kLanguageTokenClass::kMacro);
    buffer->SetWorkspaceNamesSource(nullptr);
    return kTR_Pass;
}

// The index is C/C++ - a word in another language happening to match it means nothing
DLL_EXPORT int test_codeindex_workspace_otherlanguage(ITesting *t) {
    auto buffer = TextBuffer::CreateEmptyBuffer();
    buffer->AddLine(U"ViewBase");
    buffer->SetLanguage(Editor::Instance().GetDefaultLanguage());
    auto names = MakeWorkspaceNames({{U"ViewBase", kLanguageTokenClass::kUserType}});
    buffer->SetWorkspaceNamesSource([&names]() -> WorkspaceNames::Ref {
        return names;
    });
    TR_ASSERT(t, ClassOf(buffer, 0, U"ViewBase") != kLanguageTokenClass::kUserType);

    // Switching language takes the colours along
    auto cppBuffer = CPPBuffer({U"ViewBase view;"});
    cppBuffer->SetWorkspaceNamesSource([&names]() -> WorkspaceNames::Ref {
        return names;
    });
    TR_ASSERT(t, ClassOf(cppBuffer, 0, U"ViewBase") == kLanguageTokenClass::kUserType);
    cppBuffer->SetLanguage(Editor::Instance().GetDefaultLanguage());
    TR_ASSERT(t, ClassOf(cppBuffer, 0, U"ViewBase") != kLanguageTokenClass::kUserType);

    buffer->SetWorkspaceNamesSource(nullptr);
    cppBuffer->SetWorkspaceNamesSource(nullptr);
    return kTR_Pass;
}

// With the parse thread: a new answer reaches a buffer nobody is typing in, and nullptr keeps the colours
DLL_EXPORT int test_codeindex_workspace_recolour(ITesting *t) {
    auto mainConfig = Config::Instance()["main"];
    auto wasThreaded = mainConfig.GetBool("threaded_syntaxparser", false);
    mainConfig.SetBool("threaded_syntaxparser", true);

    auto buffer = CPPBuffer({U"ViewBase view;"});
    buffer->Reparse();      // starts the thread - as loading a document does
    WorkspaceNames::Ref next = MakeWorkspaceNames({{U"ViewBase", kLanguageTokenClass::kUserType}});
    std::mutex lockNext;
    buffer->SetWorkspaceNamesSource([&next, &lockNext]() -> WorkspaceNames::Ref {
        std::lock_guard guard(lockNext);
        return next;
    });
    auto waitFor = [&buffer](kLanguageTokenClass tokenClass) {
        for (int i = 0; (i < 200) && (ClassOf(buffer, 0, U"ViewBase") != tokenClass); i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return ClassOf(buffer, 0, U"ViewBase") == tokenClass;
    };
    TR_ASSERT(t, waitFor(kLanguageTokenClass::kUserType));

    {
        std::lock_guard guard(lockNext);
        next = nullptr;     // indexing again
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    TR_ASSERT(t, ClassOf(buffer, 0, U"ViewBase") == kLanguageTokenClass::kUserType);

    {
        std::lock_guard guard(lockNext);
        next = MakeWorkspaceNames({});
    }
    TR_ASSERT(t, waitFor(kLanguageTokenClass::kRegular));

    buffer->SetWorkspaceNamesSource(nullptr);
    buffer->Close();
    mainConfig.SetBool("threaded_syntaxparser", wasThreaded);
    return kTR_Pass;
}

// The whole way: a header on disk, the indexer, the snapshot, a colour in another file's buffer
DLL_EXPORT int test_codeindex_workspace_fromdisk(ITesting *t) {
    auto root = IndexerFolder("workspace_fromdisk");
    WriteTextFile(root / "view.h", "#define VIEW_MAX 4\nclass ViewBase {\npublic:\n    ViewBase();\n    void Draw();\n    int width;\n};\n");

    auto index = CodeIndex::Create();
    auto indexer = MakeIndexer(index);
    indexer->AddRoot(root);
    indexer->WaitIdle();

    auto buffer = CPPBuffer({
        U"ViewBase view;",
        U"view.width = VIEW_MAX;",
        U"view.Draw();",
        U"int width = 0;",
    });
    buffer->SetWorkspaceNamesSource([&index]() -> WorkspaceNames::Ref {
        return index->GetWorkspaceNames();
    });
    TR_ASSERT(t, ClassOf(buffer, 0, U"ViewBase") == kLanguageTokenClass::kUserType);
    TR_ASSERT(t, ClassOf(buffer, 1, U"width") == kLanguageTokenClass::kMember);
    TR_ASSERT(t, ClassOf(buffer, 1, U"VIEW_MAX") == kLanguageTokenClass::kMacro);
    TR_ASSERT(t, ClassOf(buffer, 2, U"Draw") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassOf(buffer, 3, U"width") == kLanguageTokenClass::kRegular);
    buffer->SetWorkspaceNamesSource(nullptr);
    indexer->Stop();
    std::filesystem::remove_all(root);
    return kTR_Pass;
}

// After '.' or '->' the code says what a name is, no index needed
DLL_EXPORT int test_codeindex_workspace_syntax(ITesting *t) {
    auto syntaxOf = [](const std::u32string &text, const std::u32string &name) {
        auto idxName = text.find(name);
        return WorkspaceNames::ClassFromSyntax(text, idxName, idxName + name.size());
    };
    TR_ASSERT(t, syntaxOf(U"list.size();", U"size") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, syntaxOf(U"list->size ();", U"size") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, syntaxOf(U"a.b().c;", U"c") == kLanguageTokenClass::kMember);
    TR_ASSERT(t, syntaxOf(U"f()->size", U"size") == kLanguageTokenClass::kMember);
    TR_ASSERT(t, syntaxOf(U"cursor.y = 1;", U"y") == kLanguageTokenClass::kMember);
    TR_ASSERT(t, syntaxOf(U"    .first = 1,", U"first") == kLanguageTokenClass::kMember);

    TR_ASSERT(t, syntaxOf(U"size();", U"size") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, syntaxOf(U"auto size = 1;", U"size") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, syntaxOf(U"void f(Args... rest);", U"rest") == kLanguageTokenClass::kRegular);

    // '<' can go either way
    TR_ASSERT(t, syntaxOf(U"a.get<int>();", U"get") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, syntaxOf(U"a.count < 3", U"count") == kLanguageTokenClass::kRegular);

    // A trailing return type is not a member access
    TR_ASSERT(t, syntaxOf(U"auto f() -> Foo {", U"Foo") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, syntaxOf(U"[this]() -> Foo {", U"Foo") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, syntaxOf(U"auto f() const -> Foo;", U"Foo") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, syntaxOf(U"[x]() mutable -> Foo {", U"Foo") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, syntaxOf(U"auto f() noexcept -> Foo;", U"Foo") == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

// With no workspace names at all: the case that started this - two calls on two library types, coloured alike
DLL_EXPORT int test_codeindex_workspace_syntaxbuffer(ITesting *t) {
    auto buffer = CPPBuffer({
        U"#define Mixed 1",
        U"struct Holder {",
        U"    int Mixed;",
        U"};",
        U"void f() {",
        U"    argBackend.begin();",
        U"    openDocuments.size();",
        U"    other.Mixed();",
        U"    cursor.y = 1;",
        U"    Mixed = 2;",
        U"}",
    });
    TR_ASSERT(t, ClassOf(buffer, 5, U"begin") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassOf(buffer, 6, U"size") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassOf(buffer, 7, U"Mixed") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassOf(buffer, 8, U"y") == kLanguageTokenClass::kMember);
    TR_ASSERT(t, ClassOf(buffer, 9, U"Mixed") == kLanguageTokenClass::kRegular);    // the file disagrees with itself

    // A file that declares nothing has no names to look at - the syntax still counts
    auto noDeclarations = CPPBuffer({U"if (a) { list.size(); }"});
    TR_ASSERT(t, ClassOf(noDeclarations, 0, U"size") == kLanguageTokenClass::kFunction);

    // A language without a structure scanner keeps its text
    auto plain = TextBuffer::CreateEmptyBuffer();
    plain->AddLine(U"list.size();");
    plain->SetLanguage(Editor::Instance().GetDefaultLanguage());
    plain->Reparse();
    TR_ASSERT(t, ClassOf(plain, 0, U"size") != kLanguageTokenClass::kFunction);
    return kTR_Pass;
}

// docs/code-index.md §10.2 - the names a program gets from its platform

namespace {
    kLanguageTokenClass BareClass(const std::u32string &line, const std::u32string &name) {
        auto buffer = CPPBuffer({line});
        return ClassOf(buffer, 0, name);
    }
}

DLL_EXPORT int test_codeindex_library_shipped_lists(ITesting *t) {
    auto library = Editor::Instance().GetLibraryNames();
    TR_ASSERT(t, library->Size() > 1000);
    // 'NULL' is what a YAML reader turns into a null unless quoted
    SemanticNames noNames;
    std::u32string line = U"p = NULL;";
    TR_ASSERT(t, library->LookupBare(line, 4, U"NULL", noNames) == kLanguageTokenClass::kMacro);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_library_malformed_list(ITesting *t) {
    LibraryNames library;
    TR_ASSERT(t, library.Add("entries:\n  - kind: type\n    names: [good]\n"));
    TR_ASSERT(t, library.Size() == 1);
    // All or nothing
    TR_ASSERT(t, !library.Add("entries:\n  - kind: type\n    names: [other]\n  - kind: nonsense\n    names: [bad]\n"));
    TR_ASSERT(t, !library.Add("entries: ["));
    TR_ASSERT(t, !library.Add("name: nothing"));
    TR_ASSERT(t, library.Size() == 1);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_library_std_qualified(ITesting *t) {
    auto buffer = CPPBuffer({
        U"void f() {",
        U"    std::vector<int> v;",
        U"    std::string s = std::to_string(3);",
        U"    auto t = std::chrono::steady_clock::now();",
        U"    std::filesystem::current_path(p, ec);",
        U"    std::cout << s;",
        U"    ::std::size_t n = std::string::npos;",
        U"    std::no_such_function(1);",
        U"    std::no_such_type x;",
        U"    v.size();",
        U"    std::no_such_namespace::thing y;",
        U"    std::chrono::no_such_clock c;",
        U"}",
    });
    TR_ASSERT(t, ClassOf(buffer, 1, U"std") == kLanguageTokenClass::kNamespace);
    TR_ASSERT(t, ClassOf(buffer, 1, U"vector") == kLanguageTokenClass::kLibraryType);
    TR_ASSERT(t, ClassOf(buffer, 2, U"string") == kLanguageTokenClass::kLibraryType);
    TR_ASSERT(t, ClassOf(buffer, 2, U"to_string") == kLanguageTokenClass::kLibraryFunction);
    TR_ASSERT(t, ClassOf(buffer, 3, U"chrono") == kLanguageTokenClass::kNamespace);
    TR_ASSERT(t, ClassOf(buffer, 3, U"steady_clock") == kLanguageTokenClass::kLibraryType);
    TR_ASSERT(t, ClassOf(buffer, 4, U"current_path") == kLanguageTokenClass::kLibraryFunction);
    TR_ASSERT(t, ClassOf(buffer, 5, U"cout") == kLanguageTokenClass::kEnumValue);
    TR_ASSERT(t, ClassOf(buffer, 6, U"npos") == kLanguageTokenClass::kEnumValue);
    // By syntax, without a list
    TR_ASSERT(t, ClassOf(buffer, 7, U"no_such_function") == kLanguageTokenClass::kLibraryFunction);
    TR_ASSERT(t, ClassOf(buffer, 8, U"no_such_type") == kLanguageTokenClass::kLibraryType);
    // The object's own syntax still wins
    TR_ASSERT(t, ClassOf(buffer, 9, U"size") == kLanguageTokenClass::kFunction);
    TR_ASSERT(t, ClassOf(buffer, 10, U"no_such_namespace") == kLanguageTokenClass::kNamespace);
    TR_ASSERT(t, ClassOf(buffer, 11, U"no_such_clock") == kLanguageTokenClass::kLibraryType);
    return kTR_Pass;
}

// What 'std::' says isn't for the file to overrule; the bare name is the file's
DLL_EXPORT int test_codeindex_library_std_beats_file(ITesting *t) {
    auto buffer = CPPBuffer({
        U"struct sort { int x; };",
        U"void f() {",
        U"    std::sort(a, b);",
        U"    sort s;",
        U"}",
    });
    TR_ASSERT(t, ClassOf(buffer, 2, U"sort") == kLanguageTokenClass::kLibraryFunction);
    TR_ASSERT(t, ClassOf(buffer, 3, U"sort") == kLanguageTokenClass::kUserType);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_library_c_names(ITesting *t) {
    TR_ASSERT(t, BareClass(U"    printf(\"x\");", U"printf") == kLanguageTokenClass::kLibraryFunction);
    TR_ASSERT(t, BareClass(U"    FILE *f = 0;", U"FILE") == kLanguageTokenClass::kLibraryType);
    TR_ASSERT(t, BareClass(U"    if (c == EOF) {", U"EOF") == kLanguageTokenClass::kMacro);
    // A local named like a function
    TR_ASSERT(t, BareClass(U"    x = time + 1;", U"time") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, BareClass(U"    x = time(0);", U"time") == kLanguageTokenClass::kLibraryFunction);
    // The global qualifier leaves no local to confuse it with
    TR_ASSERT(t, BareClass(U"    apply(::tolower);", U"tolower") == kLanguageTokenClass::kLibraryFunction);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_library_bare_needs_using(ITesting *t) {
    auto without = CPPBuffer({
        U"void f() {",
        U"    vector<int> v;",
        U"    sort(a, b);",
        U"}",
    });
    TR_ASSERT(t, ClassOf(without, 1, U"vector") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, ClassOf(without, 2, U"sort") == kLanguageTokenClass::kRegular);

    auto with = CPPBuffer({
        U"using namespace std;",
        U"void f() {",
        U"    vector<int> v;",
        U"    sort(a, b);",
        U"    int count = 0;",
        U"    count = list + 1;",
        U"}",
    });
    TR_ASSERT(t, ClassOf(with, 2, U"vector") == kLanguageTokenClass::kLibraryType);
    TR_ASSERT(t, ClassOf(with, 3, U"sort") == kLanguageTokenClass::kLibraryFunction);
    // Where a local can stand it stays one
    TR_ASSERT(t, ClassOf(with, 5, U"count") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, ClassOf(with, 5, U"list") == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_library_using_name(ITesting *t) {
    auto buffer = CPPBuffer({
        U"using std::vector;",
        U"vector<int> v;",
        U"list<int> l;",
    });
    TR_ASSERT(t, ClassOf(buffer, 1, U"vector") == kLanguageTokenClass::kLibraryType);
    TR_ASSERT(t, ClassOf(buffer, 2, U"list") == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_library_using_namespace_scope(ITesting *t) {
    auto buffer = CPPBuffer({
        U"using namespace std::chrono;",
        U"seconds s;",
        U"vector<int> v;",
    });
    TR_ASSERT(t, ClassOf(buffer, 1, U"seconds") == kLanguageTokenClass::kLibraryType);
    TR_ASSERT(t, ClassOf(buffer, 2, U"vector") == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_library_project_wins(ITesting *t) {
    auto buffer = CPPBuffer({
        U"void g() {",
        U"    printf(1);",
        U"    run(1);",
        U"}",
    });
    TR_ASSERT(t, ClassOf(buffer, 1, U"printf") == kLanguageTokenClass::kLibraryFunction);
    auto names = std::make_shared<WorkspaceNames>();
    names->Add(U"printf", kLanguageTokenClass::kUserType);
    buffer->SetWorkspaceNamesSource([names]() -> WorkspaceNames::Ref {
        return names;
    });
    TR_ASSERT(t, ClassOf(buffer, 1, U"printf") == kLanguageTokenClass::kUserType);
    buffer->SetWorkspaceNamesSource(nullptr);
    return kTR_Pass;
}

// Typing or deleting a 'using' changes lines far from it
DLL_EXPORT int test_codeindex_library_using_edit(ITesting *t) {
    auto buffer = CPPBuffer({
        U"int a;",
        U"int b;",
        U"vector<int> v;",
    });
    TR_ASSERT(t, ClassOf(buffer, 2, U"vector") == kLanguageTokenClass::kRegular);
    EditRegion(buffer, 0, 1, [&]() {
        buffer->LineAt(0)->Clear();
        buffer->LineAt(0)->Append(std::u32string(U"using namespace std;"));
    });
    TR_ASSERT(t, ClassOf(buffer, 2, U"vector") == kLanguageTokenClass::kLibraryType);
    EditRegion(buffer, 0, 1, [&]() {
        buffer->LineAt(0)->Clear();
        buffer->LineAt(0)->Append(std::u32string(U"int a;"));
    });
    TR_ASSERT(t, ClassOf(buffer, 2, U"vector") == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

// 'Foo::remove' is Foo's, whatever <cstdio> says
DLL_EXPORT int test_codeindex_library_other_scope(ITesting *t) {
    TR_ASSERT(t, BareClass(U"    Foo::remove(1);", U"remove") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, BareClass(U"    a<b>::remove(1);", U"remove") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, BareClass(U"    stdx::vector<int> v;", U"vector") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, BareClass(U"    foo::std::vector<int> v;", U"vector") == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

// The workspace has no single meaning for 'log', but it does have the name: it isn't the C function's to take. Gone from
// the project, the name is the library's again.
DLL_EXPORT int test_codeindex_library_project_ambiguous(ITesting *t) {
    auto index = CodeIndex::Create();
    index->ReplaceFile("/a.h", MakeFile({
        MakeSymbol(U"log", kSymbolKind::kMember, U"Item"),
        MakeSymbol(U"log", kSymbolKind::kFunction),
        MakeSymbol(U"sqrt", kSymbolKind::kVariable),
        MakeSymbol(U"floor", kSymbolKind::kFunction),
    }));
    auto buffer = CPPBuffer({
        U"void g() {",
        U"    log(1);",
        U"    sqrt(2);",
        U"    tan(3);",
        U"    floor(4);",
        U"}",
    });
    TR_ASSERT(t, ClassOf(buffer, 1, U"log") == kLanguageTokenClass::kLibraryFunction);
    buffer->SetWorkspaceNamesSource([index]() -> WorkspaceNames::Ref {
        return index->GetWorkspaceNames();
    });
    TR_ASSERT(t, ClassOf(buffer, 1, U"log") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, ClassOf(buffer, 2, U"sqrt") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, ClassOf(buffer, 3, U"tan") == kLanguageTokenClass::kLibraryFunction);
    TR_ASSERT(t, ClassOf(buffer, 4, U"floor") == kLanguageTokenClass::kFunction);

    index->ReplaceFile("/a.h", MakeFile({}));
    auto names = index->GetWorkspaceNames();
    TR_ASSERT(t, !names->Contains(U"log"));
    buffer->SetWorkspaceNamesSource(nullptr);
    return kTR_Pass;
}

// The line from Editor.cpp, as it stands
DLL_EXPORT int test_codeindex_library_realline(ITesting *t) {
    auto buffer = CPPBuffer({
        U"void f() {",
        U"                    std::transform(argBackend.begin(), argBackend.end(), argBackend.begin(), ::tolower);",
        U"}",
    });
    TR_ASSERT(t, ClassOf(buffer, 1, U"std") == kLanguageTokenClass::kNamespace);
    TR_ASSERT(t, ClassOf(buffer, 1, U"transform") == kLanguageTokenClass::kLibraryFunction);
    TR_ASSERT(t, ClassOf(buffer, 1, U"tolower") == kLanguageTokenClass::kLibraryFunction);
    return kTR_Pass;
}

// A theme imported before the library classes existed still has to show them
DLL_EXPORT int test_codeindex_library_themefallback(ITesting *t) {
    auto has = [](std::vector<std::string> keys) {
        return [keys](const std::string &key) {
            return std::find(keys.begin(), keys.end(), key) != keys.end();
        };
    };
    auto library = kLanguageTokenClass::kLibraryFunction;
    TR_ASSERT(t, ThemeKeyForTokenClass(library, has({"library_function", "function", "regular"})) == "library_function");
    TR_ASSERT(t, ThemeKeyForTokenClass(library, has({"function", "regular"})) == "function");
    TR_ASSERT(t, ThemeKeyForTokenClass(library, has({"regular"})) == "regular");
    TR_ASSERT(t, ThemeKeyForTokenClass(kLanguageTokenClass::kLibraryType, has({"user_type", "regular"})) == "user_type");
    TR_ASSERT(t, ThemeKeyForTokenClass(kLanguageTokenClass::kMember, has({"regular"})) == "regular");
    return kTR_Pass;
}

// Project lists and lists that apply by header (§10.2). Probed directly: the buffers use the editor's lists.
namespace {
    const char *SDL_LIST =
        "name: sdl\n"
        "headers: [SDL3/SDL.h, SDL3/SDL_main.h]\n"
        "entries:\n"
        "  - kind: function\n"
        "    names: [SDL_Init]\n"
        "  - kind: type\n"
        "    names: [SDL_Event]\n";

    const char *FMT_LIST =
        "name: fmt\n"
        "headers: [fmt/format.h]\n"
        "entries:\n"
        "  - kind: namespace\n"
        "    names: [fmt]\n"
        "  - kind: function\n"
        "    scope: fmt\n"
        "    names: [format]\n";

    // A failed assert leaves the test early: the next one still gets the shipped lists
    class ScopedLibraryNames {
    public:
        explicit ScopedLibraryNames(LibraryNames::Ref names) : shipped(Editor::Instance().GetLibraryNames()) {
            Editor::Instance().SetLibraryNames(std::move(names));
        }
        ~ScopedLibraryNames() {
            Editor::Instance().SetLibraryNames(shipped);
        }
    private:
        LibraryNames::Ref shipped;
    };

    // The file's names, by the scanner
    SemanticNames NamesOf(const std::vector<std::u32string> &lines) {
        std::u32string text;
        for (auto &line : lines) {
            text += line + U"\n";
        }
        return SemanticNames::FromStructure(ScanCPP(text));
    }

    std::vector<std::u32string> WithInclude(const std::u32string &include) {
        return {include, U"int a;"};
    }
}

DLL_EXPORT int test_codeindex_library_list_header(ITesting *t) {
    LibraryNames library;
    TR_ASSERT(t, library.Add(SDL_LIST));
    std::u32string line = U"SDL_Init(0);";
    auto lookup = [&](const std::u32string &include) {
        return library.LookupBare(line, 0, U"SDL_Init", NamesOf(WithInclude(include)));
    };
    TR_ASSERT(t, lookup(U"#include <SDL3/SDL.h>") == kLanguageTokenClass::kLibraryFunction);
    // Any of its headers, whichever quotes
    TR_ASSERT(t, lookup(U"#include <SDL3/SDL_main.h>") == kLanguageTokenClass::kLibraryFunction);
    TR_ASSERT(t, lookup(U"#include \"SDL3/SDL.h\"") == kLanguageTokenClass::kLibraryFunction);
    TR_ASSERT(t, lookup(U"#include <vector>") == kLanguageTokenClass::kRegular);
    TR_ASSERT(t, lookup(U"// #include <SDL3/SDL.h>") == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

// Adding or removing an '#include' is seen, as adding or removing a 'using' is
DLL_EXPORT int test_codeindex_library_list_include_edit(ITesting *t) {
    auto library = std::make_shared<LibraryNames>();
    TR_ASSERT(t, library->Add(SDL_LIST));
    ScopedLibraryNames scoped(library);

    auto buffer = CPPBuffer({U"int a;", U"int b;", U"void f() { SDL_Init(0); }"});
    TR_ASSERT(t, ClassOf(buffer, 2, U"SDL_Init") == kLanguageTokenClass::kRegular);
    EditRegion(buffer, 0, 1, [&]() {
        buffer->LineAt(0)->Clear();
        buffer->LineAt(0)->Append(std::u32string(U"#include <SDL3/SDL.h>"));
    });
    TR_ASSERT(t, ClassOf(buffer, 2, U"SDL_Init") == kLanguageTokenClass::kLibraryFunction);
    EditRegion(buffer, 0, 1, [&]() {
        buffer->LineAt(0)->Clear();
        buffer->LineAt(0)->Append(std::u32string(U"int a;"));
    });
    TR_ASSERT(t, ClassOf(buffer, 2, U"SDL_Init") == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

// A list naming no headers applies everywhere, as c_std does
DLL_EXPORT int test_codeindex_library_list_no_headers(ITesting *t) {
    LibraryNames library;
    TR_ASSERT(t, library.Add("entries:\n  - kind: function\n    names: [company_log]\n"));
    SemanticNames noNames;
    std::u32string line = U"company_log(1);";
    TR_ASSERT(t, library.LookupBare(line, 0, U"company_log", noNames) == kLanguageTokenClass::kLibraryFunction);
    return kTR_Pass;
}

// One name in two lists: the list that applies says what it is
DLL_EXPORT int test_codeindex_library_list_two_origins(ITesting *t) {
    LibraryNames library;
    TR_ASSERT(t, library.Add("headers: [a.h]\nentries:\n  - kind: type\n    names: [Thing]\n"));
    TR_ASSERT(t, library.Add("headers: [b.h]\nentries:\n  - kind: macro\n    names: [Thing]\n"));
    std::u32string line = U"Thing x;";
    TR_ASSERT(t, library.LookupBare(line, 0, U"Thing", NamesOf(WithInclude(U"#include <a.h>"))) == kLanguageTokenClass::kLibraryType);
    TR_ASSERT(t, library.LookupBare(line, 0, U"Thing", NamesOf(WithInclude(U"#include <b.h>"))) == kLanguageTokenClass::kMacro);
    TR_ASSERT(t, library.LookupBare(line, 0, U"Thing", NamesOf(WithInclude(U"#include <c.h>"))) == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_library_list_scope(ITesting *t) {
    LibraryNames library;
    TR_ASSERT(t, library.Add(FMT_LIST));
    auto included = NamesOf(WithInclude(U"#include <fmt/format.h>"));
    std::u32string line = U"auto s = fmt::format(x);";
    TR_ASSERT(t, library.LookupBare(line, 9, U"fmt", included) == kLanguageTokenClass::kNamespace);
    TR_ASSERT(t, library.LookupBare(line, 14, U"format", included) == kLanguageTokenClass::kLibraryFunction);
    // Written with a scope, not as a call: it can't be a local
    std::u32string passed = U"apply(fmt::format);";
    TR_ASSERT(t, library.LookupBare(passed, 11, U"format", included) == kLanguageTokenClass::kLibraryFunction);
    // Not the header, or not the scope, or not written with it
    auto notIncluded = NamesOf(WithInclude(U"#include <vector>"));
    TR_ASSERT(t, library.LookupBare(line, 14, U"format", notIncluded) == kLanguageTokenClass::kRegular);
    std::u32string otherScope = U"auto s = other::format(x);";
    TR_ASSERT(t, library.LookupBare(otherScope, 16, U"format", included) == kLanguageTokenClass::kRegular);
    std::u32string nested = U"auto s = fmt::detail::format(x);";
    TR_ASSERT(t, library.LookupBare(nested, 22, U"format", included) == kLanguageTokenClass::kRegular);
    std::u32string bare = U"auto s = format(x);";
    TR_ASSERT(t, library.LookupBare(bare, 9, U"format", included) == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_library_list_scope_using(ITesting *t) {
    LibraryNames library;
    TR_ASSERT(t, library.Add(FMT_LIST));
    std::u32string line = U"auto s = format(x);";
    auto bothNames = NamesOf({U"#include <fmt/format.h>", U"using namespace fmt;"});
    TR_ASSERT(t, library.LookupBare(line, 9, U"format", bothNames) == kLanguageTokenClass::kLibraryFunction);
    // The 'using' alone doesn't make the list apply
    auto usingOnly = NamesOf({U"using namespace fmt;"});
    TR_ASSERT(t, library.LookupBare(line, 9, U"format", usingOnly) == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_library_list_scope_template(ITesting *t) {
    LibraryNames library;
    TR_ASSERT(t, library.Add(FMT_LIST));
    auto included = NamesOf(WithInclude(U"#include <fmt/format.h>"));
    std::u32string line = U"x = a<b>::fmt::format(1);";
    TR_ASSERT(t, library.LookupBare(line, 15, U"format", included) == kLanguageTokenClass::kRegular);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_library_list_project_dir(ITesting *t) {
    auto projectDir = IndexerFolder("libraries");
    std::filesystem::create_directories(projectDir / "libraries");
    WriteTextFile(projectDir / "libraries" / "sdl.yml", SDL_LIST);
    WriteTextFile(projectDir / "libraries" / "notes.txt", "entries:\n  - kind: type\n    names: [NotALibrary]\n");

    auto library = LibraryNames::Load(projectDir);
    std::u32string line = U"NotALibrary x;";
    SemanticNames noNames;
    TR_ASSERT(t, library->LookupBare(line, 0, U"NotALibrary", noNames) == kLanguageTokenClass::kRegular);
    std::u32string call = U"SDL_Init(0);";
    auto included = NamesOf(WithInclude(U"#include <SDL3/SDL.h>"));
    TR_ASSERT(t, library->LookupBare(call, 0, U"SDL_Init", included) == kLanguageTokenClass::kLibraryFunction);
    // The shipped lists are still there, and no project is fine
    std::u32string printfLine = U"printf(0);";
    TR_ASSERT(t, library->LookupBare(printfLine, 0, U"printf", noNames) == kLanguageTokenClass::kLibraryFunction);
    TR_ASSERT(t, LibraryNames::Load(std::nullopt)->Size() < library->Size());
    TR_ASSERT(t, LibraryNames::Load(projectDir / "no_such_dir")->Size() < library->Size());
    return kTR_Pass;
}

// One bad file costs that file, not the others or the shipped lists
DLL_EXPORT int test_codeindex_library_list_bad_project_file(ITesting *t) {
    auto projectDir = IndexerFolder("badlibraries");
    std::filesystem::create_directories(projectDir / "libraries");
    WriteTextFile(projectDir / "libraries" / "a_bad.yml", "entries: [");
    WriteTextFile(projectDir / "libraries" / "b_sdl.yml", SDL_LIST);
    auto library = LibraryNames::Load(projectDir);
    TR_ASSERT(t, library->Size() == LibraryNames::Load(std::nullopt)->Size() + 2);
    return kTR_Pass;
}

// Smart go-to and the navigation stack (docs/code-index.md §5.6)
namespace {
    const char32_t *FOO_H =
        U"namespace app {\n"                // 0
        U"class Foo {\n"                    // 1
        U"public:\n"                        // 2
        U"    Foo(int x);\n"                // 3
        U"    void run();\n"                // 4
        U"    void other();\n"              // 5
        U"    int count;\n"                 // 6
        U"};\n"                             // 7
        U"void helper();\n"                 // 8
        U"int count();\n"                   // 9
        U"}\n";

    const char32_t *FOO_CPP =
        U"#include \"foo.h\"\n"             // 0
        U"namespace app {\n"                // 1
        U"Foo::Foo(int x) { }\n"            // 2
        U"void Foo::run() {\n"              // 3
        U"    helper();\n"                  // 4
        U"}\n"                              // 5
        U"void Foo::other() {\n"            // 6
        U"    run();\n"                     // 7
        U"}\n"                              // 8
        U"void helper() { }\n"              // 9
        U"}\n";

    const char32_t *BAR_H =
        U"namespace other {\n"
        U"class Bar {\n"
        U"    void run() { }\n"
        U"};\n"
        U"}\n";

    CodeIndex::Ref GoToIndex() {
        auto index = CodeIndex::Create();
        index->ReplaceFile("/p/foo.h", ScanCPP(FOO_H));
        index->ReplaceFile("/p/foo.cpp", ScanCPP(FOO_CPP));
        index->ReplaceFile("/q/bar.h", ScanCPP(BAR_H));
        return index;
    }

    using kMode = GoToResolver::kMode;

    GoToResolver::Result Go(const CodeIndex::Ref &index, const std::u32string &text, const std::u32string &word, uint32_t idxLine, const std::string &path, kMode mode = kMode::kToggle) {
        return GoToResolver::Resolve(*index, text, text.find(word), idxLine, path, mode);
    }

    bool IsAt(const GoToResolver::Result &result, const std::string &path, uint32_t line) {
        return (result.hits.size() == 1) && (result.hits[0].path == path) && (result.hits[0].line == line);
    }

    NavigationStack::Entry Place(const std::string &path, const std::u32string &symbol, uint32_t line) {
        return {path, symbol, 0, line, 0};
    }
}

DLL_EXPORT int test_codeindex_goto_nearest_symbol(ITesting *t) {
    auto index = GoToIndex();
    // Inside a body: the function it's in
    TR_ASSERT(t, index->NearestSymbol("/p/foo.cpp", 4)->qualifiedName == U"app::Foo::run");
    TR_ASSERT(t, index->NearestSymbol("/p/foo.cpp", 3)->qualifiedName == U"app::Foo::run");
    // A namespace is too wide to mean a place
    TR_ASSERT(t, !index->NearestSymbol("/p/foo.cpp", 1).has_value());
    TR_ASSERT(t, !index->NearestSymbol("/p/none.cpp", 1).has_value());
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_goto_find_exact(ITesting *t) {
    auto index = GoToIndex();
    TR_ASSERT(t, index->FindExact(U"helper").size() == 2);
    TR_ASSERT(t, index->FindExact(U"Helper").empty());
    TR_ASSERT(t, index->FindExact(U"help").empty());
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_goto_toggle(ITesting *t) {
    auto index = GoToIndex();
    // The declaration and the definition lead to each other
    TR_ASSERT(t, IsAt(Go(index, U"    void run();", U"run", 4, "/p/foo.h"), "/p/foo.cpp", 3));
    TR_ASSERT(t, IsAt(Go(index, U"void Foo::run() {", U"run", 3, "/p/foo.cpp"), "/p/foo.h", 4));
    // A use goes to the definition
    TR_ASSERT(t, IsAt(Go(index, U"    helper();", U"helper", 4, "/p/foo.cpp"), "/p/foo.cpp", 9));
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_goto_modes(ITesting *t) {
    auto index = GoToIndex();
    std::u32string use = U"    helper();";
    TR_ASSERT(t, IsAt(Go(index, use, U"helper", 4, "/p/foo.cpp", kMode::kDefinition), "/p/foo.cpp", 9));
    TR_ASSERT(t, IsAt(Go(index, use, U"helper", 4, "/p/foo.cpp", kMode::kDeclaration), "/p/foo.h", 8));
    // Only one side exists: that's where it goes
    TR_ASSERT(t, IsAt(Go(index, U"Bar b;", U"Bar", 0, "/z.cpp", kMode::kDeclaration), "/q/bar.h", 1));
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_goto_already_there(ITesting *t) {
    auto index = GoToIndex();
    auto result = Go(index, U"class Foo {", U"Foo", 1, "/p/foo.h");
    TR_ASSERT(t, result.isAlreadyThere);
    TR_ASSERT(t, result.hits.empty());
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_goto_no_target(ITesting *t) {
    auto index = GoToIndex();
    TR_ASSERT(t, Go(index, U"    localName = 1;", U"localName", 4, "/p/foo.cpp").hits.empty());
    // 'std::' says it isn't the project's, even if the project has one
    auto library = Go(index, U"    std::run(x);", U"run", 4, "/p/foo.cpp");
    TR_ASSERT(t, library.name == U"run");
    TR_ASSERT(t, library.hits.empty());
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_goto_cursor_positions(ITesting *t) {
    auto index = GoToIndex();
    std::u32string text = U"    helper();";
    auto at = [&](size_t idxChar) {
        return GoToResolver::Resolve(*index, text, idxChar, 4, "/p/foo.cpp", kMode::kToggle).name;
    };
    TR_ASSERT(t, at(4) == U"helper");
    TR_ASSERT(t, at(7) == U"helper");
    TR_ASSERT(t, at(10) == U"helper");      // on the '(', right behind the name
    TR_ASSERT(t, at(1).empty());
    TR_ASSERT(t, at(12).empty());
    TR_ASSERT(t, at(13).empty());
    TR_ASSERT(t, at(99).empty());
    std::u32string endOfLine = U"helper";
    TR_ASSERT(t, GoToResolver::Resolve(*index, endOfLine, 6, 0, "/z", kMode::kToggle).name == U"helper");
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_goto_qualifier(ITesting *t) {
    auto index = GoToIndex();
    TR_ASSERT(t, IsAt(Go(index, U"x = Foo::run;", U"run", 0, "/z.cpp", kMode::kDefinition), "/p/foo.cpp", 3));
    TR_ASSERT(t, IsAt(Go(index, U"x = Bar::run;", U"run", 0, "/z.cpp", kMode::kDefinition), "/q/bar.h", 2));
    // 'oo::run' isn't 'Foo::run'
    TR_ASSERT(t, Go(index, U"x = oo::run;", U"run", 0, "/z.cpp", kMode::kDefinition).hits.size() == 2);
    // A qualifier the index doesn't know: every 'run' is still the best answer
    TR_ASSERT(t, Go(index, U"x = Alias::run;", U"run", 0, "/z.cpp", kMode::kDefinition).hits.size() == 2);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_goto_object_syntax(ITesting *t) {
    auto index = GoToIndex();
    // 'count' is a member and a function; the code says which
    auto member = Go(index, U"x = a.count;", U"count", 0, "/z.cpp", kMode::kDefinition);
    TR_ASSERT(t, (member.hits.size() == 1) && (member.hits[0].kind == kSymbolKind::kMember));
    auto function = Go(index, U"x = a.count();", U"count", 0, "/z.cpp", kMode::kDefinition);
    TR_ASSERT(t, (function.hits.size() == 1) && IsAt(function, "/p/foo.h", 9));
    // Both defined: the syntax alone tells them apart
    index->ReplaceFile("/p/count.cpp", ScanCPP(U"namespace app {\nint count() { return 0; }\n}\n"));
    auto definedMember = Go(index, U"x = a.count;", U"count", 0, "/z.cpp", kMode::kDefinition);
    TR_ASSERT(t, (definedMember.hits.size() == 1) && (definedMember.hits[0].kind == kSymbolKind::kMember));
    // The type of 'a' isn't known: all of them, to choose from
    TR_ASSERT(t, Go(index, U"a.run();", U"run", 0, "/z.cpp", kMode::kDefinition).hits.size() == 2);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_goto_enclosing_scope(ITesting *t) {
    auto index = GoToIndex();
    // In Foo::other, a bare 'run' is Foo's, not Bar's
    TR_ASSERT(t, IsAt(Go(index, U"    run();", U"run", 7, "/p/foo.cpp", kMode::kDefinition), "/p/foo.cpp", 3));
    // Outside any: both
    TR_ASSERT(t, Go(index, U"run();", U"run", 0, "/z.cpp", kMode::kDefinition).hits.size() == 2);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_goto_constructor(ITesting *t) {
    auto index = GoToIndex();
    TR_ASSERT(t, IsAt(Go(index, U"Foo x(1);", U"Foo", 0, "/z.cpp", kMode::kDefinition), "/p/foo.h", 1));
    // On the constructor itself it is the constructor
    TR_ASSERT(t, IsAt(Go(index, U"    Foo(int x);", U"Foo", 3, "/p/foo.h"), "/p/foo.cpp", 2));
    TR_ASSERT(t, IsAt(Go(index, U"Foo::Foo(int x) { }", U"Foo(", 2, "/p/foo.cpp"), "/p/foo.h", 3));
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_goto_proximity(ITesting *t) {
    auto index = CodeIndex::Create();
    for (auto path : {"/a/z.h", "/b/here.h", "/b/apple.h", "/b/here.cpp"}) {
        index->ReplaceFile(path, MakeFile({MakeSymbol(U"Thing", kSymbolKind::kClass)}));
    }
    auto result = Go(index, U"Thing x;", U"Thing", 40, "/b/here.cpp", kMode::kDefinition);
    TR_ASSERT(t, result.hits.size() == 4);
    TR_ASSERT(t, result.hits[0].path == "/b/here.cpp");
    TR_ASSERT(t, result.hits[1].path == "/b/here.h");
    TR_ASSERT(t, result.hits[2].path == "/b/apple.h");
    TR_ASSERT(t, result.hits[3].path == "/a/z.h");
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_nav_back_forward(ITesting *t) {
    NavigationStack stack;
    auto a = Place("/a", U"A", 1);
    auto b = Place("/b", U"B", 2);
    auto c = Place("/c", U"C", 3);
    TR_ASSERT(t, !stack.Back(a).has_value());
    stack.Jump(a, b);
    stack.Jump(b, c);
    TR_ASSERT(t, stack.Size() == 3);
    TR_ASSERT(t, !stack.Forward(c).has_value());
    TR_ASSERT(t, stack.Back(c)->path == "/b");
    TR_ASSERT(t, stack.Back(b)->path == "/a");
    TR_ASSERT(t, !stack.Back(a).has_value());
    TR_ASSERT(t, stack.Forward(a)->path == "/b");
    TR_ASSERT(t, stack.Forward(b)->path == "/c");
    return kTR_Pass;
}

// Forward comes back to where back left, not to where the jump landed
DLL_EXPORT int test_codeindex_nav_jump_drops_forward(ITesting *t) {
    NavigationStack stack;
    auto a = Place("/a", U"A", 1);
    auto b = Place("/b", U"B", 2);
    stack.Jump(a, b);
    auto movedOn = Place("/b", U"B2", 30);
    stack.Back(movedOn);
    TR_ASSERT(t, stack.Forward(a)->line == 30);

    stack.Back(movedOn);
    stack.Jump(a, Place("/c", U"C", 3));
    TR_ASSERT(t, stack.Size() == 2);
    TR_ASSERT(t, !stack.Forward(Place("/c", U"C", 3)).has_value());
    TR_ASSERT(t, stack.Back(Place("/c", U"C", 3))->path == "/a");
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_nav_same_symbol(ITesting *t) {
    NavigationStack stack;
    auto a = Place("/a", U"A", 1);
    auto b = NavigationStack::Entry{"/b", U"B", 0, 10, 0};
    auto sameB = NavigationStack::Entry{"/b", U"B", 5, 15, 0};
    // Nothing on the stack and a jump within a symbol: still nothing to go back to
    stack.Jump(b, sameB);
    TR_ASSERT(t, stack.Size() == 0);
    stack.Jump(a, b);
    stack.Jump(b, sameB);
    TR_ASSERT(t, stack.Size() == 2);
    TR_ASSERT(t, stack.Forward(sameB).has_value() == false);
    TR_ASSERT(t, stack.Back(sameB)->path == "/a");
    TR_ASSERT(t, stack.Forward(a)->line == 15);
    // Same name further down the file is another overload
    auto overload = NavigationStack::Entry{"/b", U"B", 0, 40, 0};
    stack.Jump(sameB, overload);
    TR_ASSERT(t, stack.Size() == 3);
    return kTR_Pass;
}

// F12, F12, F12 between a declaration and its definition
DLL_EXPORT int test_codeindex_nav_bounce(ITesting *t) {
    NavigationStack stack;
    auto start = Place("/s", U"S", 1);
    auto decl = Place("/a.h", U"f", 2);
    auto def = Place("/a.cpp", U"f", 3);
    stack.Jump(start, decl);
    stack.Jump(decl, def);
    for (int i = 0; i < 10; i++) {
        stack.Jump(def, decl);
        stack.Jump(decl, def);
    }
    TR_ASSERT(t, stack.Size() == 3);
    TR_ASSERT(t, stack.Back(def)->path == "/a.h");
    TR_ASSERT(t, stack.Back(decl)->path == "/s");
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_nav_no_from(ITesting *t) {
    NavigationStack stack;
    auto b = Place("/b", U"B", 2);
    stack.Jump({}, b);
    TR_ASSERT(t, stack.Size() == 1);
    TR_ASSERT(t, !stack.Back(b).has_value());
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_nav_cap(ITesting *t) {
    NavigationStack stack;
    for (uint32_t i = 0; i < 500; i++) {
        stack.Jump(Place("/f", U"S" + std::u32string(1, (char32_t)(U'a' + (i % 26))) + std::u32string(i / 26, U'x'), i),
                   Place("/g", U"T", i + 1000));
    }
    TR_ASSERT(t, stack.Size() <= 100);
    TR_ASSERT(t, stack.CurrentIndex() == stack.Size() - 1);
    return kTR_Pass;
}

// The reason an entry isn't only a line
DLL_EXPORT int test_codeindex_nav_lines_moved(ITesting *t) {
    auto index = CodeIndex::Create();
    index->ReplaceFile("/f.cpp", ScanCPP(U"void first() { }\nvoid second() {\n    int a;\n    int b;\n}\n"));
    auto entry = NavigationStack::MakeEntry(index.get(), "/f.cpp", 3, 4);
    TR_ASSERT(t, entry.symbol == U"second");
    TR_ASSERT(t, entry.lineOffset == 2);
    TR_ASSERT(t, NavigationStack::CurrentLine(index.get(), entry) == 3);

    index->ReplaceFile("/f.cpp", ScanCPP(U"// new\n// new\n// new\nvoid first() { }\nvoid second() {\n    int a;\n    int b;\n}\n"));
    TR_ASSERT(t, NavigationStack::CurrentLine(index.get(), entry) == 6);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_nav_symbol_gone(ITesting *t) {
    auto index = CodeIndex::Create();
    index->ReplaceFile("/f.cpp", ScanCPP(U"void first() { }\n\n\nvoid second() { }\n"));
    auto entry = NavigationStack::MakeEntry(index.get(), "/f.cpp", 3, 0);
    index->ReplaceFile("/f.cpp", ScanCPP(U"void first() { }\n"));
    TR_ASSERT(t, NavigationStack::CurrentLine(index.get(), entry) == 3);
    // No index, or a file it doesn't hold: the line
    TR_ASSERT(t, NavigationStack::CurrentLine(nullptr, entry) == 3);
    auto noSymbol = NavigationStack::MakeEntry(index.get(), "/none.cpp", 7, 0);
    TR_ASSERT(t, noSymbol.symbol.empty());
    TR_ASSERT(t, NavigationStack::CurrentLine(index.get(), noSymbol) == 7);
    TR_ASSERT(t, NavigationStack::MakeEntry(nullptr, "/f.cpp", 3, 0).symbol.empty());
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_nav_overloads(ITesting *t) {
    auto index = CodeIndex::Create();
    const std::u32string overloads = U"void f(int a) { }\n\n\n\n\n\nvoid f(char b) {\n    int x;\n}\n";
    index->ReplaceFile("/f.cpp", ScanCPP(overloads));
    auto entry = NavigationStack::MakeEntry(index.get(), "/f.cpp", 7, 0);
    TR_ASSERT(t, entry.symbol == U"f");
    index->ReplaceFile("/f.cpp", ScanCPP(U"// new\n// new\n" + overloads));
    TR_ASSERT(t, NavigationStack::CurrentLine(index.get(), entry) == 9);
    return kTR_Pass;
}

// A header's 'f' isn't the one in the file next to it, however close its line is
DLL_EXPORT int test_codeindex_nav_other_file(ITesting *t) {
    auto index = CodeIndex::Create();
    const std::u32string blanks(8, U'\n');
    index->ReplaceFile("/a.cpp", ScanCPP(blanks + U"void f() {\n}\n"));
    index->ReplaceFile("/b.cpp", ScanCPP(blanks + U"void f() {\n}\n"));
    auto entry = NavigationStack::MakeEntry(index.get(), "/b.cpp", 9, 0);
    index->ReplaceFile("/b.cpp", ScanCPP(blanks + blanks + U"void f() {\n}\n"));
    TR_ASSERT(t, NavigationStack::CurrentLine(index.get(), entry) == 17);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_nav_session(ITesting *t) {
    NavigationStack stack;
    auto a = NavigationStack::Entry{"/a", U"app::Foo", 3, 13, 2};
    auto b = Place("/b", U"", 20);
    auto c = Place("/c", U"C", 30);
    stack.Jump(a, b);
    stack.Jump(b, c);
    stack.Back(c);

    NavigationStack restored;
    restored.FromSession(stack.ToSession());
    TR_ASSERT(t, restored.Size() == 3);
    TR_ASSERT(t, restored.CurrentIndex() == 1);
    TR_ASSERT(t, restored.Forward(b)->path == "/c");
    auto back = restored.Back(c);
    back = restored.Back(*back);
    TR_ASSERT(t, (back->symbol == U"app::Foo") && (back->lineOffset == 3) && (back->line == 13) && (back->column == 2));

    // A hand-edited file can't point outside
    auto session = stack.ToSession();
    session.idxCurrent = 99;
    restored.FromSession(session);
    TR_ASSERT(t, restored.CurrentIndex() == 2);
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_nav_session_yaml(ITesting *t) {
    RootSession session;
    session.navigation.idxCurrent = 1;
    session.navigation.entries.push_back({"/a", "app::Foo", 3, 13, 2});
    session.navigation.entries.push_back({"/b", "", 0, 20, 0});
    RootSession restored;
    TR_ASSERT(t, SessionSerializer::FromYaml(SessionSerializer::ToYaml(session), restored));
    TR_ASSERT(t, restored.navigation.idxCurrent == 1);
    TR_ASSERT(t, restored.navigation.entries.size() == 2);
    auto &entry = restored.navigation.entries[0];
    TR_ASSERT(t, (entry.path == "/a") && (entry.symbol == "app::Foo") && (entry.lineOffset == 3) && (entry.line == 13) && (entry.column == 2));

    // A session from before the stack
    RootSession older;
    TR_ASSERT(t, SessionSerializer::FromYaml("version: 1\n", older));
    TR_ASSERT(t, older.navigation.entries.empty());
    return kTR_Pass;
}

// The same steps a key would take, on real documents
DLL_EXPORT int test_codeindex_nav_real_documents(ITesting *t) {
    auto root = IndexerFolder("navdocuments");
    auto pathA = (root / "a.cpp").string();
    auto pathB = (root / "b.cpp").string();
    WriteTextFile(pathA, "line0\nline1\nline2\nline3\n");
    WriteTextFile(pathB, "other0\nother1\n");

    auto &editor = Editor::Instance();
    editor.GetNavigationStack().Clear();
    auto documentA = editor.LoadDocument(pathA);
    TR_ASSERT(t, documentA != nullptr);
    editor.SetActiveDocument(documentA);
    documentA->SetCursorPosition(2, 3);

    TR_ASSERT(t, ActionHelper::JumpTo(pathB, 1, 2));
    auto documentB = editor.GetActiveDocument();
    TR_ASSERT(t, documentB != documentA);
    TR_ASSERT(t, documentB->GetLineCursor().idxActiveLine == 1);

    ActionHelper::JumpBack();
    TR_ASSERT(t, editor.GetActiveDocument() == documentA);
    TR_ASSERT(t, documentA->GetLineCursor().idxActiveLine == 2);
    TR_ASSERT(t, documentA->GetLineCursor().cursor.position.x == 3);

    ActionHelper::JumpForward();
    TR_ASSERT(t, editor.GetActiveDocument() == documentB);

    // A line past the end of a shrunk file lands on its last one
    TR_ASSERT(t, ActionHelper::JumpTo(pathA, 400, 400));
    TR_ASSERT(t, editor.GetActiveDocument()->GetLineCursor().idxActiveLine == documentA->GetTextBuffer()->NumLines() - 1);
    editor.GetNavigationStack().Clear();
    return kTR_Pass;
}

// A file gone from disk is stepped over
DLL_EXPORT int test_codeindex_nav_missing_file(ITesting *t) {
    auto root = IndexerFolder("navmissing");
    auto pathA = (root / "a.cpp").string();
    auto pathGone = (root / "gone.cpp").string();
    auto pathC = (root / "c.cpp").string();
    WriteTextFile(pathA, "line0\n");
    WriteTextFile(pathC, "line0\n");

    auto &editor = Editor::Instance();
    auto &stack = editor.GetNavigationStack();
    stack.Clear();
    auto documentC = editor.LoadDocument(pathC);
    auto documentA = editor.LoadDocument(pathA);
    stack.Jump(Place(pathA, U"", 0), Place(pathGone, U"", 0));
    stack.Jump(Place(pathGone, U"", 0), Place(pathC, U"", 0));
    editor.SetActiveDocument(documentC);

    ActionHelper::JumpBack();
    TR_ASSERT(t, editor.GetActiveDocument() == documentA);
    TR_ASSERT(t, !ActionHelper::JumpTo(pathGone, 0, 0));
    stack.Clear();
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_goto_modal_list(ITesting *t) {
    auto index = GoToIndex();
    auto hits = Go(index, U"a.run();", U"run", 0, "/z.cpp", kMode::kDefinition).hits;
    TR_ASSERT(t, hits.size() == 2);
    auto modal = SymbolSearchModal::CreateForHits(hits, nullptr, "/z.cpp", {"/p"});
    modal->SetQuery(U"");
    TR_ASSERT(t, modal->GetHits().size() == 2);
    modal->SetQuery(U"BAR");
    TR_ASSERT(t, modal->GetHits().size() == 1);
    TR_ASSERT(t, modal->GetHits()[0].qualifiedName == U"other::Bar::run");
    modal->SetQuery(U"nothing");
    TR_ASSERT(t, modal->GetHits().empty());
    return kTR_Pass;
}

// The index holds canonical paths, the workspace what it was opened with. Through a symlink they differ, and a second
// document for the same file wouldn't have the first one's unsaved edits.
DLL_EXPORT int test_codeindex_nav_load_symlinked(ITesting *t) {
    auto root = IndexerFolder("navsymlinked");
    std::filesystem::create_directories(root / "real");
    auto realPath = root / "real" / "a.cpp";
    WriteTextFile(realPath, "line0\n");
    auto linkedRoot = root / "linked";
    std::error_code error;
    std::filesystem::remove(linkedRoot, error);
    std::filesystem::create_directory_symlink(root / "real", linkedRoot);

    auto &editor = Editor::Instance();
    auto document = editor.LoadDocument((linkedRoot / "a.cpp").string());
    TR_ASSERT(t, document != nullptr);
    auto again = editor.LoadDocument(std::filesystem::canonical(realPath).string());
    TR_ASSERT(t, again == document);
    return kTR_Pass;
}

// The point of the anchor, through the key's own path: lines added above, and back lands in the same code
DLL_EXPORT int test_codeindex_nav_back_after_edit(ITesting *t) {
    auto &editor = Editor::Instance();
    auto index = editor.GetCodeIndex();
    TR_ASSERT(t, index != nullptr);
    auto root = IndexerFolder("navedit");
    auto pathA = (root / "a.cpp").string();
    auto pathB = (root / "b.cpp").string();
    const std::string body = "void first() {\n}\nvoid second() {\n    int a;\n    int b;\n}\n";
    WriteTextFile(pathA, "// one\n// two\n// three\n" + body);
    WriteTextFile(pathB, "other\n");
    auto canonicalA = std::filesystem::canonical(pathA).string();
    index->ReplaceFile(canonicalA, ScanCPP(UnicodeHelper::utf8to32(body)));

    editor.GetNavigationStack().Clear();
    auto document = editor.LoadDocument(pathA);
    TR_ASSERT(t, document != nullptr);
    editor.SetActiveDocument(document);
    document->SetCursorPosition(4, 4);      // 'int b', with the index one edit behind the three comment lines below
    TR_ASSERT(t, ActionHelper::JumpTo(pathB, 0, 0));

    // The three comment lines are in the index now, as they would be after a pause
    index->ReplaceFile(canonicalA, ScanCPP(UnicodeHelper::utf8to32("// one\n// two\n// three\n" + body)));
    ActionHelper::JumpBack();
    TR_ASSERT(t, editor.GetActiveDocument() == document);
    TR_ASSERT(t, document->GetLineCursor().idxActiveLine == 7);

    index->RemoveFile(canonicalA);
    editor.GetNavigationStack().Clear();
    return kTR_Pass;
}

// Esc, then ']' to the next view: the switch leaves quick mode by policy, and the action then finishing leaves it again.
// The second leave took the root view off the run loop's handlers, and the next key - any key - crashed on an empty stack.
DLL_EXPORT int test_codeindex_nav_quick_mode_view_switch(ITesting *t) {
    auto mainConfig = Config::Instance()["quickmode"];
    auto wasAutoLeave = mainConfig.GetBool("leave_automatically", false);
    auto wasSwitchLeave = mainConfig.GetBool("leave_when_switching_view", false);
    mainConfig.SetBool("leave_automatically", true);
    mainConfig.SetBool("leave_when_switching_view", true);

    RootView rootView;
    ViewBase first;
    ViewBase second;
    rootView.AddTopView(&first, "first");
    rootView.AddTopView(&second, "second");
    rootView.Initialize();
    auto hadRootView = RuntimeConfig::Instance().HasRootView();
    auto *previousRoot = hadRootView ? &RuntimeConfig::Instance().GetRootView() : nullptr;
    RuntimeConfig::Instance().SetRootView(&rootView);
    Runloop::SetKeypressAndActionHook(&rootView);
    auto numHandlers = Runloop::NumKeypressHandlers();

    EditorAction enter;
    enter.action = kAction::kActionEnterCommandMode;
    Runloop::DispatchAction(enter);
    TR_ASSERT(t, Editor::Instance().GetState() == Editor::QuickCommandState);
    TR_ASSERT(t, Runloop::NumKeypressHandlers() == numHandlers + 1);

    EditorAction cycle;
    cycle.uiAction = kUIAction::kUIActionCycleActiveViewNext;
    Runloop::DispatchAction(cycle);
    auto numAfter = Runloop::NumKeypressHandlers();

    // Everything back as it was, whatever the outcome: the rest of the run shares these
    if (numAfter >= numHandlers) {
        Runloop::SetKeypressAndActionHook(nullptr);
    }
    RuntimeConfig::Instance().SetRootView(previousRoot);
    mainConfig.SetBool("leave_automatically", wasAutoLeave);
    mainConfig.SetBool("leave_when_switching_view", wasSwitchLeave);

    TR_ASSERT(t, Editor::Instance().GetState() == Editor::ViewState);
    TR_ASSERT(t, numAfter == numHandlers);
    return kTR_Pass;
}

static const CodeEdge *FindEdge(const FileStructure &structure, kEdgeKind kind, const std::u32string &to) {
    for (auto &edge : structure.edges) {
        if ((edge.kind == kind) && (edge.toName == to)) {
            return &edge;
        }
    }
    return nullptr;
}

// An open file gives the index its edges, not only its symbols
DLL_EXPORT int test_codeindex_edges_open_file(ITesting *t) {
    auto buffer = CPPBuffer({
        U"#include \"Base.h\"",
        U"namespace app {",
        U"    class Item : public Base {",
        U"        Part part;",
        U"        std::shared_ptr<Other> other;",
        U"    };",
        U"}",
    });
    auto structure = buffer->CollectStructure();
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kInherits, U"app::Item", U"Base"));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"app::Item", U"Part", kOwnership::kOwns));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"app::Item", U"Other", kOwnership::kShares));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kIncludes, U"", U"Base.h"));
    return kTR_Pass;
}

// A region scan starts its symbols over, and the owner of an edge is above the edit
DLL_EXPORT int test_codeindex_edges_region_scan(ITesting *t) {
    auto buffer = CPPBuffer({
        U"namespace app {",
        U"    class Item {",
        U"        Part part;",
        U"        int count;",
        U"    };",
        U"}",
    });
    EditRegion(buffer, 3, 4, [&buffer]() {
        auto line = buffer->LineAt(3);
        line->Delete(0, (int)line->Length());
        line->Append(std::u32string(U"        Extra *extra;"));
    });
    auto structure = buffer->CollectStructure();
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"app::Item", U"Extra", kOwnership::kBorrows));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"app::Item", U"Part", kOwnership::kOwns));
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_edges_follow_lines(ITesting *t) {
    auto buffer = CPPBuffer({
        U"class Item {",
        U"    Part part;",
        U"    Gone gone;",
        U"};",
    });
    EditRegion(buffer, 0, 1, [&buffer]() {
        buffer->Insert(0, U"// a comment");
    });
    auto structure = buffer->CollectStructure();
    auto edge = FindEdge(structure, kEdgeKind::kHolds, U"Part");
    TR_ASSERT(t, edge != nullptr);
    TR_ASSERT(t, edge->line == 2);

    EditRegion(buffer, 3, 3, [&buffer]() {
        buffer->DeleteLineAt(3);
    });
    structure = buffer->CollectStructure();
    TR_ASSERT(t, FindEdge(structure, kEdgeKind::kHolds, U"Gone") == nullptr);
    TR_ASSERT(t, FindEdge(structure, kEdgeKind::kHolds, U"Part") != nullptr);
    return kTR_Pass;
}

// After a series of edits the edges are what a fresh parse of the result gives
DLL_EXPORT int test_codeindex_edges_incremental(ITesting *t) {
    auto buffer = CPPBuffer({
        U"#include \"a.h\"",
        U"namespace app {",
        U"    class Item : public Base {",
        U"        Part part;",
        U"    };",
        U"    class Other {",
        U"        Item *item;",
        U"    };",
        U"}",
    });
    EditRegion(buffer, 2, 3, [&buffer]() {
        auto line = buffer->LineAt(2);
        line->Delete(0, (int)line->Length());
        line->Append(std::u32string(U"    class Item : public Newer {"));
    });
    EditRegion(buffer, 7, 8, [&buffer]() {
        buffer->Insert(7, U"        std::unique_ptr<Part> owned;");
    });
    EditRegion(buffer, 3, 3, [&buffer]() {
        buffer->DeleteLineAt(3);
    });
    EditRegion(buffer, 0, 1, [&buffer]() {
        buffer->Insert(0, U"#include \"b.h\"");
    });

    auto describe = [](const FileStructure &structure) {
        std::vector<std::tuple<kEdgeKind, std::u32string, std::u32string, kOwnership, uint32_t>> edges;
        for (auto &edge : structure.edges) {
            edges.emplace_back(edge.kind, edge.fromName, edge.toName, edge.ownership, edge.line);
        }
        std::sort(edges.begin(), edges.end());
        return edges;
    };
    auto fresh = CPPBuffer(BufferLines(buffer));
    TR_ASSERT(t, describe(buffer->CollectStructure()) == describe(fresh->CollectStructure()));
    return kTR_Pass;
}

// A nameless typedef has no name until the ';', so its edges get theirs there
DLL_EXPORT int test_codeindex_edges_typedef(ITesting *t) {
    CPPStructureScanner scanner(CPPTokenizer());
    auto structure = scanner.Scan(U"namespace app {\n"
                                  U"    struct Before { Part part; };\n"
                                  U"    typedef struct {\n"
                                  U"        Foo *foo;\n"
                                  U"        struct { Inner *inner; } nested;\n"
                                  U"    } Bar;\n"
                                  U"    typedef struct Named { Foo *foo; } Named_t;\n"
                                  U"}\n"
                                  U"typedef struct { Top *top; } TopLevel;\n");
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"app::Before", U"Part", kOwnership::kOwns));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"app::Bar", U"Foo", kOwnership::kBorrows));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"app::Bar", U"Inner", kOwnership::kBorrows));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"app::Named", U"Foo", kOwnership::kBorrows));
    TR_ASSERT(t, HasEdge(structure, kEdgeKind::kHolds, U"TopLevel", U"Top", kOwnership::kBorrows));

    auto anonymous = scanner.Scan(U"struct { Foo *foo; } global;\n");
    TR_ASSERT(t, FindEdge(anonymous, kEdgeKind::kHolds, U"Foo") == nullptr);   // nothing named holds it
    return kTR_Pass;
}

// A region scan inside a nameless typedef can't know the name. A missing edge is cheap, the wrong owner isn't.
DLL_EXPORT int test_codeindex_edges_typedef_region(ITesting *t) {
    auto buffer = CPPBuffer({
        U"namespace app {",
        U"    typedef struct {",
        U"        Foo *foo;",
        U"        int count;",
        U"    } Bar;",
        U"}",
    });
    EditRegion(buffer, 3, 4, [&buffer]() {
        auto line = buffer->LineAt(3);
        line->Delete(0, (int)line->Length());
        line->Append(std::u32string(U"        Extra *extra;"));
    });
    auto structure = buffer->CollectStructure();
    for (auto &edge : structure.edges) {
        TR_ASSERT(t, edge.fromName != U"app");
        TR_ASSERT(t, !edge.fromName.empty());
    }
    return kTR_Pass;
}

static std::vector<std::u32string> FromNames(const std::vector<CodeIndex::Relation> &relations) {
    std::vector<std::u32string> fromNames;
    for (auto &relation : relations) {
        fromNames.push_back(relation.fromName);
    }
    return fromNames;
}

DLL_EXPORT int test_codeindex_relations(ITesting *t) {
    CPPStructureScanner scanner(CPPTokenizer());
    auto index = CodeIndex::Create();
    index->ReplaceFile("/p/base.h", scanner.Scan(U"namespace app { class Base { }; class Part { }; }"));
    index->ReplaceFile("/p/item.h", scanner.Scan(U"namespace app {\n"
                                                 U"    class Item : public Base {\n"
                                                 U"        Part part;\n"
                                                 U"        Base *base;\n"
                                                 U"        gnilk::ILogger *logger;\n"
                                                 U"    };\n"
                                                 U"}\n"));
    index->ReplaceFile("/p/other.h", scanner.Scan(U"class Other : public app::Item { app::Part *part; };"));

    TR_ASSERT(t, index->TypesNamed(U"Item") == std::vector<std::u32string>{U"app::Item"});
    TR_ASSERT(t, index->TypesNamed(U"app::Item") == std::vector<std::u32string>{U"app::Item"});
    TR_ASSERT(t, index->TypesNamed(U"tem").empty());

    auto bases = index->BasesOf(U"app::Item");
    TR_ASSERT(t, bases.size() == 1);
    TR_ASSERT(t, bases[0].targets == std::vector<std::u32string>{U"app::Base"});
    TR_ASSERT(t, bases[0].path == "/p/item.h");
    TR_ASSERT(t, bases[0].line == 1);

    TR_ASSERT(t, FromNames(index->DerivedFrom(U"app::Base")) == std::vector<std::u32string>{U"app::Item"});
    TR_ASSERT(t, FromNames(index->DerivedFrom(U"app::Item")) == std::vector<std::u32string>{U"Other"});

    auto holds = index->HoldsOf(U"app::Item");
    TR_ASSERT(t, holds.size() == 3);
    bool sawOutside = false;
    for (auto &relation : holds) {
        sawOutside = sawOutside || ((relation.toName == U"gnilk::ILogger") && relation.targets.empty());
    }
    TR_ASSERT(t, sawOutside);

    auto heldBy = index->HeldBy(U"app::Part");
    TR_ASSERT(t, FromNames(heldBy) == (std::vector<std::u32string>{U"Other", U"app::Item"}));
    TR_ASSERT(t, heldBy[1].ownership == kOwnership::kOwns);
    TR_ASSERT(t, heldBy[0].ownership == kOwnership::kBorrows);
    TR_ASSERT(t, index->GetStats().numEdges == 6);
    return kTR_Pass;
}

// §5.2: the scopes around the edge first, then the name anywhere, and several stay several
DLL_EXPORT int test_codeindex_relations_resolve(ITesting *t) {
    CPPStructureScanner scanner(CPPTokenizer());
    auto index = CodeIndex::Create();
    index->ReplaceFile("/p/a.h", scanner.Scan(U"namespace a { class Node { }; class User : public Node { }; }"));
    index->ReplaceFile("/p/b.h", scanner.Scan(U"namespace b { class Node { }; class User : public Node { }; }"));
    index->ReplaceFile("/p/c.h", scanner.Scan(U"class Stray : public Node { };"));
    index->ReplaceFile("/p/d.h", scanner.Scan(U"class Node { }; namespace c { class Global : public Node { }; }"));
    index->ReplaceFile("/p/e.h", scanner.Scan(U"class Outer { class Node { }; class Inner : public Node { }; };"));

    TR_ASSERT(t, FromNames(index->DerivedFrom(U"a::Node")) == std::vector<std::u32string>{U"a::User"});
    TR_ASSERT(t, FromNames(index->DerivedFrom(U"b::Node")) == std::vector<std::u32string>{U"b::User"});
    // The nested type goes first, and the global one is what the others reach
    TR_ASSERT(t, FromNames(index->DerivedFrom(U"Outer::Node")) == std::vector<std::u32string>{U"Outer::Inner"});
    TR_ASSERT(t, FromNames(index->DerivedFrom(U"Node")) == (std::vector<std::u32string>{U"Stray", U"c::Global"}));

    index->RemoveFile("/p/d.h");
    auto bases = index->BasesOf(U"Stray");
    TR_ASSERT(t, bases.size() == 1);
    TR_ASSERT(t, bases[0].targets == (std::vector<std::u32string>{U"Outer::Node", U"a::Node", U"b::Node"}));   // ambiguous, never a guess
    return kTR_Pass;
}

DLL_EXPORT int test_codeindex_relations_replace(ITesting *t) {
    CPPStructureScanner scanner(CPPTokenizer());
    auto index = CodeIndex::Create();
    index->ReplaceFile("/p/base.h", scanner.Scan(U"class Base { }; class Newer { };"));
    index->ReplaceFile("/p/item.h", scanner.Scan(U"class Item : public Base { Base *a; Base *b; };"));
    TR_ASSERT(t, index->GetStats().numEdges == 3);

    index->ReplaceFile("/p/item.h", scanner.Scan(U"class Item : public Newer { };"));
    TR_ASSERT(t, index->DerivedFrom(U"Base").empty());
    TR_ASSERT(t, index->HeldBy(U"Base").empty());
    TR_ASSERT(t, index->HoldsOf(U"Item").empty());
    TR_ASSERT(t, FromNames(index->DerivedFrom(U"Newer")) == std::vector<std::u32string>{U"Item"});
    TR_ASSERT(t, index->GetStats().numEdges == 1);

    index->RemoveFile("/p/item.h");
    TR_ASSERT(t, index->DerivedFrom(U"Newer").empty());
    TR_ASSERT(t, index->BasesOf(U"Item").empty());
    TR_ASSERT(t, index->GetStats().numEdges == 0);

    // A file with no target left: what it pointed to resolves to nothing, and nothing dangles
    index->ReplaceFile("/p/item.h", scanner.Scan(U"class Item : public Newer { };"));
    index->RemoveFile("/p/base.h");
    auto bases = index->BasesOf(U"Item");
    TR_ASSERT(t, bases.size() == 1);
    TR_ASSERT(t, bases[0].targets.empty());
    return kTR_Pass;
}

// '#define _u8 unsigned char' names a type too, and a real type of that name goes first
DLL_EXPORT int test_codeindex_relations_macro_type(ITesting *t) {
    CPPStructureScanner scanner(CPPTokenizer());
    auto index = CodeIndex::Create();
    index->ReplaceFile("/p/a.h", scanner.Scan(U"#define _u8 unsigned char\n#define Part int\nstruct A { _u8 count; Part part; Lost *lost; };\n"));
    index->ReplaceFile("/p/b.h", scanner.Scan(U"namespace b { class Part { }; }"));

    auto holds = index->HoldsOf(U"A");
    TR_ASSERT(t, holds.size() == 3);
    for (auto &relation : holds) {
        if (relation.toName == U"_u8") {
            TR_ASSERT(t, relation.targets == std::vector<std::u32string>{U"_u8"});
        } else if (relation.toName == U"Part") {
            TR_ASSERT(t, relation.targets == std::vector<std::u32string>{U"b::Part"});
        } else {
            TR_ASSERT(t, relation.targets.empty());
        }
    }
    return kTR_Pass;
}
