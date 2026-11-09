//
// Parameters and locals of a function - see LocalScanner.h
//

#include <algorithm>

#include "LocalScanner.h"
#include "TypeRef.h"

using namespace gedit;

namespace {
    using Tokens = std::vector<LocalScanner::Token>;

    // A statement starting with one of these declares nothing
    const std::vector<std::u32string> kNotDeclarations = {
        U"return", U"delete", U"throw", U"case", U"goto", U"break", U"continue", U"using", U"typedef", U"co_return",
        U"co_yield", U"co_await", U"default", U"new", U"sizeof", U"static_assert", U"public", U"private", U"protected",
    };
    // Whatever these head, the variable they declare lives in the block that follows
    const std::vector<std::u32string> kControls = { U"for", U"if", U"while", U"switch", U"catch" };
    // Between a parameter list and its body: 'void f() const override {', '[&]() mutable -> int {'
    const std::vector<std::u32string> kTrailers = { U"const", U"override", U"final", U"noexcept", U"mutable", U"volatile" };
    // 'auto x = std::make_shared<T>(...)' - T, not a chain
    const std::vector<std::u32string> kMakers = { U"make_shared", U"make_unique", U"allocate_shared" };
    const std::vector<std::u32string> kCasts = {
        U"static_cast", U"dynamic_cast", U"reinterpret_cast", U"const_cast", U"static_pointer_cast", U"dynamic_pointer_cast",
    };

    bool Contains(const std::vector<std::u32string> &list, const std::u32string &text) {
        return std::find(list.begin(), list.end(), text) != list.end();
    }

    bool IsWordChar(char32_t ch) {
        return ((ch >= U'a') && (ch <= U'z')) || ((ch >= U'A') && (ch <= U'Z')) || ((ch >= U'0') && (ch <= U'9')) || (ch == U'_') || (ch > 127);
    }

    bool IsIdentifier(const LocalScanner::Token &token) {
        if (token.text.empty() || (token.classification == kLanguageTokenClass::kKeyword)) {
            return false;
        }
        auto ch = token.text[0];
        return ((ch >= U'a') && (ch <= U'z')) || ((ch >= U'A') && (ch <= U'Z')) || (ch == U'_') || (ch > 127);
    }

    bool IsText(const Tokens &tokens, size_t idx, const char32_t *text) {
        return (idx < tokens.size()) && (tokens[idx].text == text);
    }

    // The index of the bracket matching the opening one at 'idxOpen', or tokens.size()
    size_t MatchingClose(const Tokens &tokens, size_t idxOpen) {
        auto open = tokens[idxOpen].text;
        auto close = (open == U"(") ? U")" : (open == U"[") ? U"]" : (open == U"{") ? U"}" : U">";
        int depth = 0;
        for (size_t i = idxOpen; i < tokens.size(); i++) {
            if (tokens[i].text == open) {
                depth++;
            } else if (tokens[i].text == close) {
                depth--;
            } else if ((open == U"<") && (tokens[i].text == U">>")) {
                depth -= 2;
            }
            if (depth <= 0) {
                return i;
            }
        }
        return tokens.size();
    }

    // Tokens back to text, a space only where two words would run together ('new Foo')
    std::u32string Join(const Tokens &tokens, size_t idxBegin, size_t idxEnd) {
        std::u32string text;
        for (size_t i = idxBegin; i < std::min(idxEnd, tokens.size()); i++) {
            auto &part = tokens[i].text;
            if (!text.empty() && !part.empty() && IsWordChar(text.back()) && IsWordChar(part[0])) {
                text += U' ';
            }
            text += part;
        }
        return text;
    }

    TypeRef ParseType(const Tokens &tokens, size_t &idx, size_t idxEnd) {
        std::vector<TypeRef::Token> typeTokens;
        for (size_t i = idx; i < std::min(idxEnd, tokens.size()); i++) {
            if (tokens[i].text == U">>") {
                typeTokens.push_back({U">"});
                typeTokens.push_back({U">"});
                continue;
            }
            typeTokens.push_back({tokens[i].text, tokens[i].classification == kLanguageTokenClass::kKnownType});
        }
        size_t idxType = 0;
        auto type = TypeRef::Parse(typeTokens, idxType);
        // Back to an index into 'tokens': a '>>' was two
        size_t consumed = 0;
        for (size_t i = idx; (i < std::min(idxEnd, tokens.size())) && (consumed < idxType); i++) {
            consumed += (tokens[i].text == U">>") ? 2 : 1;
            idx = i + 1;
        }
        return type;
    }

    // A type that is all of [idxBegin, idxEnd) - anything left over and it wasn't a type: 'a.b', 'Foo::Bar', 'std::cout <<'
    bool ParseWholeType(const Tokens &tokens, size_t idxBegin, size_t idxEnd, TypeRef &outType) {
        auto idx = idxBegin;
        outType = ParseType(tokens, idx, idxEnd);
        return (idx == idxEnd) && !outType.name.empty() && (outType.name.back() != U':');
    }

    // The name a declaration declares: the last identifier, not a qualifier, before what may follow a name at the top level
    size_t DeclaredName(const Tokens &tokens, size_t idxBegin, size_t idxEnd) {
        size_t idxName = idxEnd;
        int angleDepth = 0;
        for (size_t i = idxBegin; i < idxEnd; i++) {
            auto &text = tokens[i].text;
            bool isEnd = (text == U"=") || (text == U"(") || (text == U"{") || (text == U"[") || (text == U",") || (text == U":") || (text == U";");
            if (isEnd && (angleDepth == 0)) {
                break;
            }
            if (text == U"<") {
                angleDepth++;
            } else if (text == U">") {
                angleDepth--;
            } else if (text == U">>") {
                angleDepth -= 2;
            } else if ((angleDepth == 0) && IsIdentifier(tokens[i]) && !IsText(tokens, i + 1, U"::")) {
                idxName = i;
            }
        }
        return idxName;
    }

    // 'std::make_shared<T>(...)', 'static_cast<T *>(p)', 'new T(...)': the type the initializer says outright
    std::u32string WrittenTypeOfInitializer(const Tokens &tokens, size_t idxBegin, size_t idxEnd) {
        auto idx = idxBegin;
        if (IsText(tokens, idx, U"new")) {
            idx++;
            auto type = ParseType(tokens, idx, idxEnd);
            if (type.name.empty()) {
                return {};
            }
            type.numPointers++;
            return type.Spelling();
        }
        if (IsText(tokens, idx, U"std") && IsText(tokens, idx + 1, U"::")) {
            idx += 2;
        }
        if ((idx < idxEnd) && (Contains(kMakers, tokens[idx].text) || Contains(kCasts, tokens[idx].text)) && IsText(tokens, idx + 1, U"<")) {
            idx += 2;
            auto type = ParseType(tokens, idx, idxEnd);
            return type.name.empty() ? std::u32string{} : type.Spelling();
        }
        return {};
    }

    class FunctionScan {
    public:
        explicit FunctionScan(const Tokens &tokens) : tokens(tokens) {
        }

        std::vector<LocalVariable> Run() {
            for (size_t i = 0; i < tokens.size(); i++) {
                OnToken(i);
            }
            std::vector<LocalVariable> visible;
            for (auto &level : levels) {
                visible.insert(visible.end(), level.locals.begin(), level.locals.end());
            }
            return visible;
        }
    private:
        struct Level {
            bool isInitializer = false;     // '= {...}', 'Foo x{1}' - the statement goes on after it
            std::vector<LocalVariable> locals;
            int outerParenDepth = 0;        // a lambda's body inside a call: 'f([](int x) { ... })'
        };
    private:
        void OnToken(size_t idx) {
            auto &text = tokens[idx].text;
            if ((text == U"{") && ((parenDepth == 0) || IsLambdaBody())) {
                OnOpenBrace(idx);
                return;
            }
            if ((text == U"}") && (parenDepth == 0)) {
                OnCloseBrace(idx);
                return;
            }
            if (text == U"(") {
                parenDepth++;
            } else if ((text == U")") && (parenDepth > 0)) {
                parenDepth--;
            }
            if ((text == U";") && (parenDepth == 0) && !levels.back().isInitializer) {
                OnStatementEnd();
                return;
            }
            statement.push_back(tokens[idx]);
        }

        void OnOpenBrace(size_t idx) {
            if (IsInitializerBrace()) {
                levels.push_back({true, {}, 0});
                statement.push_back(tokens[idx]);
                return;
            }
            auto pending = DeclaredByHeader();
            levels.push_back({false, std::move(pending), parenDepth});
            parenDepth = 0;
            statement.clear();
        }

        void OnCloseBrace(size_t idx) {
            if (levels.size() == 1) {
                // Scanned from before the function: the end of what came before it
                statement.clear();
                return;
            }
            bool isInitializer = levels.back().isInitializer;
            if (!isInitializer) {
                parenDepth = levels.back().outerParenDepth;
            }
            levels.pop_back();
            if (isInitializer) {
                statement.push_back(tokens[idx]);
            } else {
                statement.clear();
            }
        }

        void OnStatementEnd() {
            // Outside the body there are no locals: only the parameter list counts, at its '{'
            if (levels.size() > 1) {
                AddDeclarations(statement, 0, statement.size(), levels.back().locals, false);
            }
            statement.clear();
        }

        // A block opens after nothing, after 'else' / 'do' / 'try', or after a ')' and its trailers; else it's an initializer
        bool IsInitializerBrace() const {
            if (levels.back().isInitializer) {
                return true;
            }
            if (statement.empty()) {
                return false;
            }
            auto &last = statement.back().text;
            // 'case A: {'
            if ((last == U"else") || (last == U"do") || (last == U"try") || (last == U":")) {
                return false;
            }
            return IndexOfLastParen() == statement.size();
        }

        // The '(' of the parameter list or header right before a body - statement.size() when there is none
        size_t IndexOfLastParen() const {
            auto idx = statement.size();
            // Trailers - and a trailing return type, '-> int'
            while (idx > 0) {
                auto &text = statement[idx - 1].text;
                if (Contains(kTrailers, text)) {
                    idx--;
                    continue;
                }
                size_t idxArrow = idx;
                while ((idxArrow > 0) && (statement[idxArrow - 1].text != U"->") && (statement[idxArrow - 1].text != U")")) {
                    idxArrow--;
                }
                if ((idxArrow > 0) && (statement[idxArrow - 1].text == U"->") && (idxArrow < idx)) {
                    idx = idxArrow - 1;
                    continue;
                }
                break;
            }
            if ((idx == 0) || (statement[idx - 1].text != U")")) {
                return statement.size();
            }
            int depth = 0;
            for (size_t i = idx; i > 0; i--) {
                auto &text = statement[i - 1].text;
                depth += (text == U")") ? 1 : (text == U"(") ? -1 : 0;
                if (depth == 0) {
                    return i - 1;
                }
            }
            return statement.size();
        }

        bool IsLambdaBody() const {
            auto idxParen = IndexOfLastParen();
            return (idxParen != statement.size()) && (idxParen > 0) && (statement[idxParen - 1].text == U"]");
        }

        // What the block about to open starts with: the function's parameters, a lambda's, a catch's, a header's variable
        std::vector<LocalVariable> DeclaredByHeader() {
            std::vector<LocalVariable> declared;
            if (levels.size() == 1) {
                // The function's own '{': the first parameter list - a constructor's initializer list comes after it
                for (size_t i = 1; i < statement.size(); i++) {
                    if ((statement[i].text == U"(") && IsIdentifier(statement[i - 1])) {
                        AddParameters(i, declared);
                        break;
                    }
                }
                return declared;
            }
            auto idxParen = IndexOfLastParen();
            if ((idxParen == statement.size()) || (idxParen == 0)) {
                return declared;
            }
            auto &before = statement[idxParen - 1].text;
            if ((before == U"]") || (before == U"catch")) {
                AddParameters(idxParen, declared);
            } else if (Contains(kControls, before)) {
                AddHeader(idxParen, declared);
            }
            return declared;
        }

        // '(const Foo &a, int b = 2)'
        void AddParameters(size_t idxOpen, std::vector<LocalVariable> &declared) {
            auto idxClose = MatchingClose(statement, idxOpen);
            auto idxBegin = idxOpen + 1;
            int depth = 0;
            for (size_t i = idxBegin; i <= idxClose && (i < statement.size()); i++) {
                auto &text = statement[i].text;
                if ((text == U"(") || (text == U"<") || (text == U"[") || (text == U"{")) {
                    depth++;
                } else if (((text == U")") || (text == U">") || (text == U"]") || (text == U"}")) && (i != idxClose)) {
                    depth--;
                }
                if (((text == U",") && (depth == 0)) || (i == idxClose)) {
                    AddDeclarations(statement, idxBegin, i, declared, true);
                    idxBegin = i + 1;
                }
            }
        }

        // 'for (auto &x : xs)', 'for (int i = 0; ...)', 'if (auto x = f(); x)'
        void AddHeader(size_t idxOpen, std::vector<LocalVariable> &declared) {
            auto idxClose = MatchingClose(statement, idxOpen);
            int depth = 0;
            for (size_t i = idxOpen + 1; i < idxClose; i++) {
                auto &text = statement[i].text;
                depth += ((text == U"(") || (text == U"[") || (text == U"{") || (text == U"<")) ? 1 : 0;
                depth -= ((text == U")") || (text == U"]") || (text == U"}") || (text == U">")) ? 1 : 0;
                if ((depth == 0) && ((text == U";") || (text == U":"))) {
                    if (text == U":") {
                        AddRangeFor(idxOpen + 1, i, idxClose, declared);
                    } else {
                        AddDeclarations(statement, idxOpen + 1, i, declared, false);
                    }
                    return;
                }
            }
            AddDeclarations(statement, idxOpen + 1, idxClose, declared, false);
        }

        void AddRangeFor(size_t idxBegin, size_t idxColon, size_t idxEnd, std::vector<LocalVariable> &declared) {
            // 'auto &[key, value]' - a structured binding has no single type
            TypeRef type;
            if ((idxColon <= idxBegin + 1) || !IsIdentifier(statement[idxColon - 1]) || !ParseWholeType(statement, idxBegin, idxColon - 1, type)) {
                return;
            }
            auto idx = idxColon - 1;
            LocalVariable local;
            local.name = statement[idx].text;
            local.line = statement[idx].line;
            local.column = statement[idx].column;
            if (type.name == U"auto") {
                local.initializer = Join(statement, idxColon + 1, idxEnd);
                local.isElement = true;
            } else {
                local.type = type.Spelling();
            }
            declared.push_back(std::move(local));
        }

        // 'Type a = x, *b, c(1), d{2}' in [idxBegin, idxEnd). A parameter may have no name.
        static void AddDeclarations(const Tokens &stmt, size_t idxBegin, size_t idxEnd, std::vector<LocalVariable> &declared, bool isParameter) {
            while ((idxBegin < idxEnd) && ((stmt[idxBegin].text == U"else") || (stmt[idxBegin].text == U"static") || (stmt[idxBegin].text == U"constexpr"))) {
                idxBegin++;
            }
            if ((idxBegin >= idxEnd) || Contains(kNotDeclarations, stmt[idxBegin].text) || Contains(kControls, stmt[idxBegin].text)) {
                return;
            }
            auto idx = DeclaredName(stmt, idxBegin, idxEnd);
            TypeRef type;
            if ((idx == idxEnd) || !ParseWholeType(stmt, idxBegin, idx, type)) {
                return;
            }
            bool isFirst = true;
            while (idx < idxEnd) {
                auto declaratorType = type;
                if (!isFirst) {
                    declaratorType.numPointers = 0;
                    declaratorType.isReference = false;
                    while ((idx < idxEnd) && ((stmt[idx].text == U"*") || (stmt[idx].text == U"&") || (stmt[idx].text == U"&&") || (stmt[idx].text == U"const"))) {
                        declaratorType.numPointers += (stmt[idx].text == U"*") ? 1 : 0;
                        declaratorType.isReference = declaratorType.isReference || (stmt[idx].text != U"*");
                        idx++;
                    }
                }
                if ((idx >= idxEnd) || !IsIdentifier(stmt[idx])) {
                    return;
                }
                auto &nameToken = stmt[idx];
                idx++;
                // What may follow a declared name - anything else and this was an expression: 'a.b = c', 'f(x)'
                bool isEnd = (idx >= idxEnd);
                auto next = isEnd ? std::u32string{} : stmt[idx].text;
                if (!isEnd && (next != U"=") && (next != U"(") && (next != U"{") && (next != U"[") && (next != U",") && (next != U":")) {
                    return;
                }
                // 'x(1)' in a parameter list is not a declaration; 'Foo x(1)' in a body is
                if (isParameter && (next == U"(")) {
                    return;
                }
                // The declarator ends at a top level ','
                size_t idxDeclaratorEnd = idx;
                int depth = 0;
                for (; idxDeclaratorEnd < idxEnd; idxDeclaratorEnd++) {
                    auto &text = stmt[idxDeclaratorEnd].text;
                    depth += ((text == U"(") || (text == U"[") || (text == U"{") || (text == U"<")) ? 1 : 0;
                    depth -= ((text == U")") || (text == U"]") || (text == U"}") || (text == U">")) ? 1 : 0;
                    if ((text == U",") && (depth == 0)) {
                        break;
                    }
                }
                LocalVariable local;
                local.name = nameToken.text;
                local.line = nameToken.line;
                local.column = nameToken.column;
                if (declaratorType.name == U"auto") {
                    if (next == U"=") {
                        local.type = WrittenTypeOfInitializer(stmt, idx + 1, idxDeclaratorEnd);
                        if (local.type.empty()) {
                            local.initializer = Join(stmt, idx + 1, idxDeclaratorEnd);
                        }
                    }
                } else {
                    local.type = declaratorType.Spelling();
                }
                if (!local.type.empty() || !local.initializer.empty()) {
                    declared.push_back(std::move(local));
                }
                if (isParameter) {
                    return;
                }
                isFirst = false;
                idx = idxDeclaratorEnd + 1;
            }
        }
    private:
        const Tokens &tokens;
        std::vector<Level> levels = { Level{} };    // levels[0]: outside the function's body
        Tokens statement;
        int parenDepth = 0;
    };
}

void LocalScanner::AppendLineTokens(const Line &line, uint32_t idxLine, size_t idxEndColumn, std::vector<Token> &tokens) {
    auto &text = line.Buffer();
    auto &attributes = line.Attributes();
    auto idxEnd = std::min(idxEndColumn, text.size());
    std::vector<Token> lineTokens;
    for (size_t i = 0; i < attributes.size(); i++) {
        auto idxStart = (size_t)std::max(0, attributes[i].idxOrigString);
        auto idxTokenEnd = (i + 1 < attributes.size()) ? (size_t)attributes[i + 1].idxOrigString : text.size();
        idxTokenEnd = std::min(idxTokenEnd, idxEnd);
        while ((idxTokenEnd > idxStart) && ((text[idxTokenEnd - 1] == U' ') || (text[idxTokenEnd - 1] == U'\t'))) {
            idxTokenEnd--;
        }
        if (idxTokenEnd <= idxStart) {
            continue;
        }
        auto classification = attributes[i].tokenClass;
        switch (classification) {
            case kLanguageTokenClass::kLineComment :
            case kLanguageTokenClass::kBlockComment :
            case kLanguageTokenClass::kCommentedText :
            case kLanguageTokenClass::kString :
            case kLanguageTokenClass::kChar :
                continue;
            case kLanguageTokenClass::kPreProcessor :
                // '#if ...' and the like: the whole line
                if (lineTokens.empty()) {
                    return;
                }
                break;
            default:
                break;
        }
        auto tokenText = text.substr(idxStart, idxTokenEnd - idxStart);
        if ((tokenText == U"\"") || (tokenText == U"'")) {
            continue;
        }
        lineTokens.push_back({std::move(tokenText), idxLine, (uint32_t)idxStart, classification});
    }
    tokens.insert(tokens.end(), lineTokens.begin(), lineTokens.end());
}

std::vector<LocalVariable> LocalScanner::Scan(const std::vector<Token> &tokens) {
    return FunctionScan(tokens).Run();
}
