//
// Code index (docs/code-index.md). Step 0: the lexer fast path 'LangLineTokenizer::TokenizeText' - same
// tokens as the editor's parse, line numbers, line endings. Throughput is measured by 'gedit-bench'
// (utests/bench/), not here: trun links the ASan runtime, which skews anything allocation-heavy.
//
#include <testinterface.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

#include "Core/Editor.h"
#include "Core/Config/Config.h"
#include "Core/TextBuffer.h"
#include "Core/UnicodeHelper.h"
#include "Core/Language/LangLineTokenizer.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_codeindex(ITesting *t);
    DLL_EXPORT int test_codeindex_tokenize_lines(ITesting *t);
    DLL_EXPORT int test_codeindex_tokenize_matches_editor(ITesting *t);
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
