//
// Smart go-to - see GoToResolver.h
//

#include <algorithm>
#include <filesystem>
#include <tuple>

#include "GoToResolver.h"
#include "LibraryNames.h"
#include "TypeRef.h"
#include "WorkspaceNames.h"

using namespace gedit;

using Hits = std::vector<CodeIndex::Hit>;

static bool IsIdentifierChar(char32_t ch) {
    return ((ch >= U'a') && (ch <= U'z')) || ((ch >= U'A') && (ch <= U'Z')) || ((ch >= U'0') && (ch <= U'9')) || (ch == U'_');
}

template<typename Predicate>
static void NarrowTo(Hits &hits, Predicate isKept) {
    Hits kept;
    std::copy_if(hits.begin(), hits.end(), std::back_inserter(kept), isKept);
    if (!kept.empty()) {
        hits = std::move(kept);
    }
}

static bool IsCallable(const CodeIndex::Hit &hit) {
    return (hit.kind == kSymbolKind::kFunction) || (hit.kind == kSymbolKind::kMethod);
}

static bool IsClassLike(const CodeIndex::Hit &hit) {
    return (hit.kind == kSymbolKind::kClass) || (hit.kind == kSymbolKind::kStruct) || (hit.kind == kSymbolKind::kUnion);
}

// 'gedit::Foo::bar' -> 'gedit::Foo', '' at the outermost
static std::u32string OuterScope(const std::u32string &qualifiedName) {
    auto idxSeparator = qualifiedName.rfind(U"::");
    return (idxSeparator == std::u32string::npos) ? std::u32string{} : qualifiedName.substr(0, idxSeparator);
}

// A constructor is its class, not a second meaning (§11.6)
static bool IsConstructor(const CodeIndex::Hit &hit) {
    auto scope = OuterScope(hit.qualifiedName);
    auto idxInner = scope.rfind(U"::");
    return IsCallable(hit) && (((idxInner == std::u32string::npos) ? scope : scope.substr(idxInner + 2)) == hit.name);
}

// 'Foo::bar' written, 'gedit::Foo::bar' in the index
static bool EndsWithScope(const std::u32string &qualifiedName, const std::u32string &written) {
    if (qualifiedName.size() < written.size()) {
        return false;
    }
    auto idxStart = qualifiedName.size() - written.size();
    return (qualifiedName.compare(idxStart, written.size(), written) == 0) && ((idxStart == 0) || (qualifiedName.compare(idxStart - 2, 2, U"::") == 0));
}

// Inside a method its class comes first, then the namespaces around it. The nearest symbol at or above the line says
// where that is: a function body holds none of its own.
static void NarrowToEnclosingScope(Hits &hits, const CodeIndex &index, const std::string &path, uint32_t idxLine) {
    auto nearest = index.NearestSymbol(path, idxLine);
    if (!nearest.has_value()) {
        return;
    }
    for (auto scope = nearest->qualifiedName; !scope.empty(); scope = OuterScope(scope)) {
        bool isInScope = std::any_of(hits.begin(), hits.end(), [&scope](const CodeIndex::Hit &hit) {
            return OuterScope(hit.qualifiedName) == scope;
        });
        if (isInScope) {
            NarrowTo(hits, [&scope](const CodeIndex::Hit &hit) {
                return OuterScope(hit.qualifiedName) == scope;
            });
            return;
        }
    }
}

static void NarrowByCode(Hits &hits, const CodeIndex &index, const std::u32string &text, size_t idxName, const std::u32string &name, uint32_t idxLine, const std::string &path) {
    auto written = LibraryNames::WrittenQualifier(text, idxName);
    if (!written.empty()) {
        auto qualified = written + U"::" + name;
        NarrowTo(hits, [&qualified](const CodeIndex::Hit &hit) {
            return EndsWithScope(hit.qualifiedName, qualified);
        });
        return;
    }
    auto fromSyntax = WorkspaceNames::ClassFromSyntax(text, idxName, idxName + name.size());
    if (fromSyntax == kLanguageTokenClass::kFunction) {
        NarrowTo(hits, IsCallable);
    } else if (fromSyntax == kLanguageTokenClass::kMember) {
        NarrowTo(hits, [](const CodeIndex::Hit &hit) {
            return hit.kind == kSymbolKind::kMember;
        });
    } else {
        // In its own class a constructor is closer than the class, which sits one scope out, and would win. A constructor
        // written out ('Foo::Foo') took the branch above.
        if (std::any_of(hits.begin(), hits.end(), IsClassLike)) {
            NarrowTo(hits, [](const CodeIndex::Hit &hit) {
                return !IsConstructor(hit);
            });
        }
        NarrowToEnclosingScope(hits, index, path, idxLine);
    }
}

// Same file, the file's other half ('a.h' for 'a.cpp'), same folder, the rest
static int ProximityOf(const CodeIndex::Hit &hit, const std::string &path) {
    if (hit.path == path) {
        return 0;
    }
    std::filesystem::path hitPath(hit.path);
    std::filesystem::path activePath(path);
    if (hitPath.stem() == activePath.stem()) {
        return 1;
    }
    return (hitPath.parent_path() == activePath.parent_path()) ? 2 : 3;
}

static bool IsTypeHit(const CodeIndex::Hit &hit) {
    return IsClassLike(hit) || (hit.kind == kSymbolKind::kEnum) || (hit.kind == kSymbolKind::kAlias);
}

static std::u32string LastComponent(const std::u32string &name) {
    auto idxSeparator = name.rfind(U"::");
    return (idxSeparator == std::u32string::npos) ? name : name.substr(idxSeparator + 2);
}

//
// Chains: 'a.b->c()[i].name' (§11.11)
//
namespace {
    enum class kPostfix {
        kCall,
        kIndex,
        kRange,     // 'for (auto x : <chain>)' - not written, added for a range-for's local
    };

    struct Link {
        std::u32string name;
        std::u32string qualifier;           // 'Editor' of 'Editor::Instance()' - the first link only
        std::vector<kPostfix> postfixes;    // as written: 'a()[i]' is a call, then an index
    };

    // What a receiver can be: project types, qualified - or a library type. Neither: nobody knows.
    struct Receiver {
        std::vector<std::u32string> types;
        std::u32string libraryType;

        bool IsKnown() const {
            return !types.empty() || !libraryType.empty();
        }
        void Add(const Receiver &other) {
            for (auto &type : other.types) {
                if (std::find(types.begin(), types.end(), type) == types.end()) {
                    types.push_back(type);
                }
            }
            if (libraryType.empty()) {
                libraryType = other.libraryType;
            }
        }
    };

    // Where the cursor is, and the parameters and locals it sees (slice C)
    struct Context {
        const CodeIndex &index;
        uint32_t idxLine = 0;
        const std::string &path;
        const GoToResolver::Locals &locals;
        size_t numVisible = 0;          // of 'locals' - an initializer sees only what was declared before it
        std::u32string functionScope;   // the function the cursor is in: where a local's written type resolves from
    };

    // An alias of an alias of ... - and 'typedef struct X X;' names itself
    const int kMaxAliasDepth = 8;
}

static Context MakeContext(const CodeIndex &index, uint32_t idxLine, const std::string &path, const GoToResolver::Locals &locals);
static Receiver ReceiverBefore(const Context &context, const std::u32string &text, size_t idxName);
static std::vector<Link> ChainBefore(const std::u32string &text, size_t idxName, size_t *outIdxStart = nullptr);
static Receiver ReceiverOfChain(const Context &context, const std::vector<Link> &links);
static int FindLocal(const Context &context, const std::u32string &name);
static Receiver TypeOfLocal(const Context &context, size_t idxLocal, const std::vector<kPostfix> &postfixes);
static Hits FirstLinkHits(const Context &context, const Link &link);
static std::vector<std::u32string> EnclosingClass(const Context &context);
static Hits MembersOf(const CodeIndex &index, const std::vector<std::u32string> &types, const std::u32string &name);
static std::vector<std::u32string> WithBases(const CodeIndex &index, const std::vector<std::u32string> &types);
static Receiver TypeOfHit(const CodeIndex &index, const CodeIndex::Hit &hit, const std::vector<kPostfix> &postfixes);
static Receiver ResolveWritten(const CodeIndex &index, const TypeRef &type, const std::u32string &fromScope, int depth);

static Context MakeContext(const CodeIndex &index, uint32_t idxLine, const std::string &path, const GoToResolver::Locals &locals) {
    Context context{index, idxLine, path, locals, locals.size(), {}};
    auto nearest = index.NearestSymbol(path, idxLine);
    if (nearest.has_value() && IsCallable(*nearest)) {
        context.functionScope = nearest->qualifiedName;
    }
    return context;
}

// What the name at 'idxName' is a member of, from the code before it. Unknown when there is no '.' or '->' before it.
static Receiver ReceiverBefore(const Context &context, const std::u32string &text, size_t idxName) {
    auto links = ChainBefore(text, idxName);
    if (links.empty()) {
        return {};
    }
    return ReceiverOfChain(context, links);
}

static size_t SkipSpacesBack(const std::u32string &text, size_t idx) {
    while ((idx > 0) && ((text[idx - 1] == U' ') || (text[idx - 1] == U'\t'))) {
        idx--;
    }
    return idx;
}

// '.' or '->' right before 'idx' (spaces aside): where it starts. 'idx' itself when there is none.
static size_t AccessBefore(const std::u32string &text, size_t idx) {
    auto idxEnd = SkipSpacesBack(text, idx);
    if ((idxEnd >= 1) && (text[idxEnd - 1] == U'.')) {
        return idxEnd - 1;
    }
    if ((idxEnd >= 2) && (text[idxEnd - 2] == U'-') && (text[idxEnd - 1] == U'>')) {
        return idxEnd - 2;
    }
    return idx;
}

// The links before the name, first to last - empty when the name isn't reached through '.' or '->'
static std::vector<Link> ChainBefore(const std::u32string &text, size_t idxName, size_t *outIdxStart) {
    std::vector<Link> links;
    auto idx = AccessBefore(text, idxName);
    if (idx == idxName) {
        return {};
    }
    while (true) {
        Link link;
        idx = SkipSpacesBack(text, idx);
        while ((idx > 0) && ((text[idx - 1] == U')') || (text[idx - 1] == U']'))) {
            auto close = text[idx - 1];
            auto open = (close == U')') ? U'(' : U'[';
            int depth = 0;
            do {
                idx--;
                depth += (text[idx] == close) ? 1 : (text[idx] == open) ? -1 : 0;
            } while ((idx > 0) && (depth > 0));
            if (depth != 0) {
                return {};
            }
            link.postfixes.insert(link.postfixes.begin(), (close == U')') ? kPostfix::kCall : kPostfix::kIndex);
            idx = SkipSpacesBack(text, idx);
        }
        auto idxEnd = idx;
        while ((idx > 0) && IsIdentifierChar(text[idx - 1])) {
            idx--;
        }
        if ((idx == idxEnd) || ((text[idx] >= U'0') && (text[idx] <= U'9'))) {
            return {};
        }
        link.name = text.substr(idx, idxEnd - idx);
        auto idxBefore = SkipSpacesBack(text, idx);
        if ((idxBefore >= 2) && (text[idxBefore - 2] == U':') && (text[idxBefore - 1] == U':')) {
            link.qualifier = LibraryNames::WrittenQualifier(text, idx);
            if (link.qualifier.empty()) {
                return {};
            }
            links.push_back(std::move(link));
            break;
        }
        links.push_back(std::move(link));
        auto idxAccess = AccessBefore(text, idx);
        if (idxAccess == idx) {
            break;
        }
        idx = idxAccess;
    }
    std::reverse(links.begin(), links.end());
    if (outIdxStart != nullptr) {
        *outIdxStart = links.front().qualifier.empty() ? idx : SkipSpacesBack(text, idx) - links.front().qualifier.size() - 2;
    }
    return links;
}

static Receiver ReceiverOfChain(const Context &context, const std::vector<Link> &links) {
    auto &index = context.index;
    Receiver receiver;
    for (size_t i = 0; i < links.size(); i++) {
        auto &link = links[i];
        bool isBare = (i == 0) && link.qualifier.empty();
        if (isBare && (link.name == U"this")) {
            receiver.types = EnclosingClass(context);
            if (receiver.types.empty()) {
                return {};
            }
            continue;
        }
        // A parameter or local hides a member of that name, as it does for the compiler
        auto idxLocal = isBare ? FindLocal(context, link.name) : -1;
        Receiver next;
        if (idxLocal >= 0) {
            next = TypeOfLocal(context, (size_t)idxLocal, link.postfixes);
        } else {
            auto hits = (i == 0) ? FirstLinkHits(context, link) : MembersOf(index, receiver.types, link.name);
            for (auto &hit : hits) {
                next.Add(TypeOfHit(index, hit, link.postfixes));
            }
        }
        // A library type has no members the index knows; only the last link may be one
        if (!next.types.empty() || (!next.libraryType.empty() && (i + 1 == links.size()))) {
            receiver = std::move(next);
            continue;
        }
        return {};
    }
    return receiver;
}

// The last one declared with the name, of those visible - -1 if none
static int FindLocal(const Context &context, const std::u32string &name) {
    for (size_t i = context.numVisible; i > 0; i--) {
        if (context.locals[i - 1].name == name) {
            return (int)(i - 1);
        }
    }
    return -1;
}

// A written type resolves from the function; 'auto' is the type of its initializer, typed as a chain with only what was
// declared before it in sight
static Receiver TypeOfLocal(const Context &context, size_t idxLocal, const std::vector<kPostfix> &postfixes) {
    auto &local = context.locals[idxLocal];
    if (!local.type.empty()) {
        auto type = TypeRef::FromSpelling(local.type);
        for (auto postfix : postfixes) {
            if (postfix == kPostfix::kCall) {
                return {};
            }
            auto element = (postfix == kPostfix::kRange) ? type.RangeElement() : type.Element();
            if (!element.has_value()) {
                return {};
            }
            type = *element;
        }
        return ResolveWritten(context.index, type.Accessed(), context.functionScope, 0);
    }
    // The whole initializer must be one chain: 'a + b' is not the type of 'b'
    auto expression = local.initializer + U".x";
    size_t idxStart = 0;
    auto links = ChainBefore(expression, expression.size() - 1, &idxStart);
    if (links.empty() || (idxStart != 0)) {
        return {};
    }
    if (local.isElement) {
        links.back().postfixes.push_back(kPostfix::kRange);
    }
    links.back().postfixes.insert(links.back().postfixes.end(), postfixes.begin(), postfixes.end());
    auto inner = context;
    inner.numVisible = idxLocal;
    return ReceiverOfChain(inner, links);
}

// The first link, by the scopes around the cursor from the inside out - a class's bases at its level. A name found nowhere
// around the cursor is a local or a parameter, and nobody knows its type: a member of an unrelated class isn't a guess worth making.
static Hits FirstLinkHits(const Context &context, const Link &link) {
    auto &index = context.index;
    auto hits = index.FindExact(link.name);
    std::erase_if(hits, [](const CodeIndex::Hit &hit) {
        return (hit.kind == kSymbolKind::kNamespace) || (hit.kind == kSymbolKind::kEnumerator) || (hit.kind == kSymbolKind::kMacro);
    });
    if (!link.qualifier.empty()) {
        auto qualified = link.qualifier + U"::" + link.name;
        std::erase_if(hits, [&qualified](const CodeIndex::Hit &hit) {
            return !EndsWithScope(hit.qualifiedName, qualified);
        });
        return hits;
    }
    auto nearest = index.NearestSymbol(context.path, context.idxLine);
    std::u32string scope = nearest.has_value() ? nearest->qualifiedName : U"";
    while (true) {
        auto family = scope.empty() ? std::vector<std::u32string>{U""} : WithBases(index, {scope});
        Hits inScope;
        std::copy_if(hits.begin(), hits.end(), std::back_inserter(inScope), [&family](const CodeIndex::Hit &hit) {
            return std::find(family.begin(), family.end(), OuterScope(hit.qualifiedName)) != family.end();
        });
        if (!inScope.empty()) {
            return inScope;
        }
        if (scope.empty()) {
            return {};
        }
        scope = OuterScope(scope);
    }
}

// The innermost scope around the cursor that is a type
static std::vector<std::u32string> EnclosingClass(const Context &context) {
    auto nearest = context.index.NearestSymbol(context.path, context.idxLine);
    if (!nearest.has_value()) {
        return {};
    }
    for (auto scope = nearest->qualifiedName; !scope.empty(); scope = OuterScope(scope)) {
        auto types = context.index.TypesNamed(scope);
        if (std::find(types.begin(), types.end(), scope) != types.end()) {
            return {scope};
        }
    }
    return {};
}

// Members and methods of the types, else of their bases - the nearest level that has the name, as an override hides what it
// overrides
static Hits MembersOf(const CodeIndex &index, const std::vector<std::u32string> &types, const std::u32string &name) {
    auto hits = index.FindExact(name);
    std::erase_if(hits, [](const CodeIndex::Hit &hit) {
        return (hit.kind != kSymbolKind::kMember) && !IsCallable(hit) && (hit.kind != kSymbolKind::kVariable);
    });
    std::vector<std::u32string> seen = types;
    auto level = types;
    while (!level.empty()) {
        Hits found;
        std::copy_if(hits.begin(), hits.end(), std::back_inserter(found), [&level](const CodeIndex::Hit &hit) {
            return std::find(level.begin(), level.end(), OuterScope(hit.qualifiedName)) != level.end();
        });
        if (!found.empty()) {
            return found;
        }
        std::vector<std::u32string> nextLevel;
        for (auto &type : level) {
            for (auto &relation : index.BasesOf(type)) {
                for (auto &target : relation.targets) {
                    if (std::find(seen.begin(), seen.end(), target) == seen.end()) {
                        seen.push_back(target);
                        nextLevel.push_back(target);
                    }
                }
            }
        }
        level = std::move(nextLevel);
    }
    return {};
}

// The types and all their bases. A cycle in bad code ends at what's been seen.
static std::vector<std::u32string> WithBases(const CodeIndex &index, const std::vector<std::u32string> &types) {
    std::vector<std::u32string> family = types;
    for (size_t i = 0; i < family.size(); i++) {
        for (auto &relation : index.BasesOf(family[i])) {
            for (auto &target : relation.targets) {
                if (std::find(family.begin(), family.end(), target) == family.end()) {
                    family.push_back(target);
                }
            }
        }
    }
    return family;
}

// What '.' or '->' after the symbol reaches, through its calls and indexes. A type is its own: 'Foo().x'.
static Receiver TypeOfHit(const CodeIndex &index, const CodeIndex::Hit &hit, const std::vector<kPostfix> &postfixes) {
    if (IsClassLike(hit)) {
        return {{hit.qualifiedName}, {}};
    }
    if (hit.type.empty()) {
        return {};
    }
    auto type = TypeRef::FromSpelling(hit.type);
    for (auto postfix : postfixes) {
        if ((postfix == kPostfix::kIndex) || (postfix == kPostfix::kRange)) {
            auto element = (postfix == kPostfix::kRange) ? type.RangeElement() : type.Element();
            if (!element.has_value()) {
                return {};
            }
            type = *element;
        } else if (!IsCallable(hit)) {
            // A call on a variable: a function object, whose result isn't known
            return {};
        }
    }
    return ResolveWritten(index, type.Accessed(), OuterScope(hit.qualifiedName), 0);
}

// A type as written, from where it was written, through aliases to what they stand for
static Receiver ResolveWritten(const CodeIndex &index, const TypeRef &type, const std::u32string &fromScope, int depth) {
    Receiver receiver;
    if (type.name.empty() || type.isBuiltin || (depth > kMaxAliasDepth)) {
        return receiver;
    }
    if (type.name.rfind(U"std::", 0) == 0) {
        receiver.libraryType = type.name;
        return receiver;
    }
    for (auto &qualified : index.ResolveType(type.name, fromScope)) {
        auto hits = index.FindExact(LastComponent(qualified));
        std::erase_if(hits, [&qualified](const CodeIndex::Hit &hit) {
            return (hit.qualifiedName != qualified) || !IsTypeHit(hit);
        });
        bool isAlias = !hits.empty() && std::all_of(hits.begin(), hits.end(), [](const CodeIndex::Hit &hit) {
            return hit.kind == kSymbolKind::kAlias;
        });
        if (!isAlias) {
            receiver.Add({{qualified}, {}});
            continue;
        }
        for (auto &alias : hits) {
            if (!alias.type.empty()) {
                receiver.Add(ResolveWritten(index, TypeRef::FromSpelling(alias.type).Accessed(), OuterScope(qualified), depth + 1));
            }
        }
    }
    return receiver;
}

GoToResolver::Result GoToResolver::Resolve(const CodeIndex &index, const std::u32string &text, size_t idxChar, uint32_t idxLine, const std::string &path, kMode mode, const Locals &locals) {
    Result result;
    // Right behind a name counts: that's where the cursor is after typing it
    if ((idxChar >= text.size()) || !IsIdentifierChar(text[idxChar])) {
        if ((idxChar == 0) || (idxChar > text.size()) || !IsIdentifierChar(text[idxChar - 1])) {
            return result;
        }
        idxChar--;
    }
    auto idxStart = idxChar;
    while ((idxStart > 0) && IsIdentifierChar(text[idxStart - 1])) {
        idxStart--;
    }
    auto idxEnd = idxChar;
    while ((idxEnd < text.size()) && IsIdentifierChar(text[idxEnd])) {
        idxEnd++;
    }
    result.name = text.substr(idxStart, idxEnd - idxStart);
    result.idxName = idxStart;
    if (LibraryNames::IsQualifiedStd(text, idxStart)) {
        return result;
    }

    auto context = MakeContext(index, idxLine, path, locals);
    // A parameter or local, written bare: its declaration. On the declaration itself there is nowhere else to go.
    bool isBare = ChainBefore(text, idxStart).empty() && LibraryNames::WrittenQualifier(text, idxStart).empty();
    auto idxLocal = isBare ? FindLocal(context, result.name) : -1;
    if (idxLocal >= 0) {
        auto &local = locals[(size_t)idxLocal];
        if ((local.line == idxLine) && (local.column == idxStart)) {
            result.isAlreadyThere = true;
            return result;
        }
        CodeIndex::Hit hit;
        hit.name = local.name;
        hit.qualifiedName = context.functionScope.empty() ? local.name : context.functionScope + U"::" + local.name;
        hit.path = path;
        hit.kind = kSymbolKind::kVariable;
        hit.line = local.line;
        hit.column = local.column;
        hit.isDefinition = true;
        hit.type = local.type;
        result.hits.push_back(std::move(hit));
        return result;
    }

    auto hits = index.FindExact(result.name);
    auto self = std::find_if(hits.begin(), hits.end(), [&](const CodeIndex::Hit &hit) {
        return (hit.path == path) && (hit.line == idxLine) && (hit.column == idxStart);
    });
    if ((mode == kMode::kToggle) && (self != hits.end())) {
        auto here = *self;
        std::erase_if(hits, [&here](const CodeIndex::Hit &hit) {
            return (hit.qualifiedName != here.qualifiedName) || (hit.isDefinition == here.isDefinition);
        });
        result.isAlreadyThere = hits.empty();
    } else {
        auto receiver = ReceiverBefore(context, text, idxStart);
        auto members = receiver.types.empty() ? Hits{} : MembersOf(index, receiver.types, result.name);
        if (!members.empty()) {
            hits = std::move(members);
        } else if (!receiver.libraryType.empty()) {
            result.libraryType = receiver.libraryType;
            hits.clear();
        } else {
            NarrowByCode(hits, index, text, idxStart, result.name, idxLine, path);
        }
        if (std::any_of(hits.begin(), hits.end(), IsClassLike)) {
            NarrowTo(hits, [](const CodeIndex::Hit &hit) {
                return !IsConstructor(hit);
            });
        }
        bool isDeclaration = mode == kMode::kDeclaration;
        NarrowTo(hits, [isDeclaration](const CodeIndex::Hit &hit) {
            return hit.isDefinition != isDeclaration;
        });
    }
    std::sort(hits.begin(), hits.end(), [&path](const CodeIndex::Hit &a, const CodeIndex::Hit &b) {
        return std::make_tuple(ProximityOf(a, path), std::cref(a.path), a.line) < std::make_tuple(ProximityOf(b, path), std::cref(b.path), b.line);
    });
    result.hits = std::move(hits);
    return result;
}

namespace {
    // What the relations are asked about
    struct Subject {
        std::u32string name;
        std::vector<std::u32string> types;  // a type, or the types the method is in
        bool isMethod = false;
    };
}

// Out of line a method is written 'EditorView::Draw' and its type is 'gedit::EditorView'
static Subject SubjectFromHits(const CodeIndex &index, const Hits &hits) {
    Subject subject;
    for (auto &hit : hits) {
        if (IsTypeHit(hit)) {
            subject.name = hit.name;
            subject.isMethod = false;
            subject.types.push_back(hit.qualifiedName);
        } else if (IsCallable(hit) && subject.types.empty() && !OuterScope(hit.qualifiedName).empty()) {
            subject.name = hit.name;
            subject.isMethod = true;
            subject.types = index.TypesNamed(OuterScope(hit.qualifiedName));
        }
    }
    return subject;
}

static std::vector<std::u32string> DirectlyRelated(const CodeIndex &index, const std::u32string &type, GoToResolver::kRelated related) {
    std::vector<std::u32string> types;
    if (related == GoToResolver::kRelated::kImplementation) {
        for (auto &relation : index.DerivedFrom(type)) {
            types.push_back(relation.fromName);
        }
    } else {
        for (auto &relation : index.BasesOf(type)) {
            types.insert(types.end(), relation.targets.begin(), relation.targets.end());
        }
    }
    return types;
}

static void AddTypeHits(Hits &hits, const CodeIndex &index, const std::u32string &type) {
    Hits found = index.FindExact(LastComponent(type));
    std::erase_if(found, [&type](const CodeIndex::Hit &hit) {
        return (hit.qualifiedName != type) || !IsTypeHit(hit);
    });
    NarrowTo(found, [](const CodeIndex::Hit &hit) {
        return hit.isDefinition;
    });
    hits.insert(hits.end(), found.begin(), found.end());
}

// An implementation is a body. A base is where the method is declared, which is all an interface has.
static void AddMethodHits(Hits &hits, const CodeIndex &index, const std::u32string &type, const std::u32string &method, GoToResolver::kRelated related) {
    Hits found = index.FindExact(method);
    std::erase_if(found, [&type](const CodeIndex::Hit &hit) {
        return !IsCallable(hit) || !EndsWithScope(type, OuterScope(hit.qualifiedName));
    });
    bool isDefinition = related == GoToResolver::kRelated::kImplementation;
    NarrowTo(found, [isDefinition](const CodeIndex::Hit &hit) {
        return hit.isDefinition == isDefinition;
    });
    hits.insert(hits.end(), found.begin(), found.end());
}

GoToResolver::Result GoToResolver::ResolveRelated(const CodeIndex &index, const std::u32string &text, size_t idxChar, uint32_t idxLine, const std::string &path, kRelated related, const Locals &locals) {
    auto atCursor = Resolve(index, text, idxChar, idxLine, path, kMode::kDefinition, locals);
    auto subject = SubjectFromHits(index, atCursor.hits);
    if (subject.types.empty()) {
        auto nearest = index.NearestSymbol(path, idxLine);
        if (nearest.has_value()) {
            subject = SubjectFromHits(index, {*nearest});
        }
    }
    Result result;
    result.name = subject.types.empty() ? atCursor.name : subject.name;
    result.idxName = atCursor.idxName;

    Hits hits;
    std::vector<std::u32string> seen = subject.types;
    auto level = subject.types;
    // Level by level, as the nearest base is the one a method overrides. A cycle in bad code ends at what's been seen.
    while (!level.empty()) {
        std::vector<std::u32string> nextLevel;
        for (auto &type : level) {
            for (auto &relatedType : DirectlyRelated(index, type, related)) {
                if (std::find(seen.begin(), seen.end(), relatedType) == seen.end()) {
                    seen.push_back(relatedType);
                    nextLevel.push_back(relatedType);
                }
            }
        }
        for (auto &type : nextLevel) {
            if (subject.isMethod) {
                AddMethodHits(hits, index, type, subject.name, related);
            } else {
                AddTypeHits(hits, index, type);
            }
        }
        bool isDone = (related == kRelated::kBase) && (!subject.isMethod || !hits.empty());
        level = isDone ? std::vector<std::u32string>{} : std::move(nextLevel);
    }
    std::sort(hits.begin(), hits.end(), [&path](const CodeIndex::Hit &a, const CodeIndex::Hit &b) {
        return std::make_tuple(ProximityOf(a, path), std::cref(a.path), a.line) < std::make_tuple(ProximityOf(b, path), std::cref(b.path), b.line);
    });
    result.hits = std::move(hits);
    return result;
}

GoToResolver::Result GoToResolver::ResolveTypeOf(const CodeIndex &index, const std::u32string &text, size_t idxChar, uint32_t idxLine, const std::string &path, const Locals &locals) {
    auto atCursor = Resolve(index, text, idxChar, idxLine, path, kMode::kDefinition, locals);
    Result result;
    result.name = atCursor.name;
    result.idxName = atCursor.idxName;
    result.libraryType = atCursor.libraryType;
    // A local, on its declaration or a use: 'auto' has no written type, its initializer has one
    auto context = MakeContext(index, idxLine, path, locals);
    bool isBare = !atCursor.name.empty() && ChainBefore(text, atCursor.idxName).empty() && LibraryNames::WrittenQualifier(text, atCursor.idxName).empty();
    auto idxLocal = isBare ? FindLocal(context, atCursor.name) : -1;
    if ((idxLocal >= 0) && locals[(size_t)idxLocal].type.empty()) {
        auto receiver = TypeOfLocal(context, (size_t)idxLocal, {});
        Hits hits;
        for (auto &type : receiver.types) {
            AddTypeHits(hits, index, type);
        }
        result.libraryType = hits.empty() ? receiver.libraryType : U"";
        result.hits = std::move(hits);
        return result;
    }
    if (idxLocal >= 0) {
        CodeIndex::Hit hit;
        hit.qualifiedName = context.functionScope.empty() ? atCursor.name : context.functionScope + U"::" + atCursor.name;
        hit.type = locals[(size_t)idxLocal].type;
        atCursor.hits = {hit};
    }
    if (std::any_of(atCursor.hits.begin(), atCursor.hits.end(), IsTypeHit)) {
        std::erase_if(atCursor.hits, [](const CodeIndex::Hit &hit) {
            return !IsTypeHit(hit);
        });
        result.hits = std::move(atCursor.hits);
        return result;
    }
    // A container's element is the type worth going to: 'std::vector<Line::Ref>' is about 'Line'
    Receiver receiver;
    for (auto &hit : atCursor.hits) {
        auto type = TypeRef::FromSpelling(hit.type);
        auto element = type.Element();
        if ((type.numPointers == 0) && element.has_value()) {
            type = *element;
        }
        receiver.Add(ResolveWritten(index, type.Accessed(), OuterScope(hit.qualifiedName), 0));
    }
    Hits hits;
    for (auto &type : receiver.types) {
        AddTypeHits(hits, index, type);
    }
    if (hits.empty()) {
        result.libraryType = receiver.libraryType;
    }
    std::sort(hits.begin(), hits.end(), [&path](const CodeIndex::Hit &a, const CodeIndex::Hit &b) {
        return std::make_tuple(ProximityOf(a, path), std::cref(a.path), a.line) < std::make_tuple(ProximityOf(b, path), std::cref(b.path), b.line);
    });
    result.hits = std::move(hits);
    return result;
}
