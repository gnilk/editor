//
// Smart go-to - see GoToResolver.h
//

#include <algorithm>
#include <filesystem>
#include <tuple>

#include "GoToResolver.h"
#include "LibraryNames.h"
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

GoToResolver::Result GoToResolver::Resolve(const CodeIndex &index, const std::u32string &text, size_t idxChar, uint32_t idxLine, const std::string &path, kMode mode) {
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
        NarrowByCode(hits, index, text, idxStart, result.name, idxLine, path);
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

static bool IsTypeHit(const CodeIndex::Hit &hit) {
    return IsClassLike(hit) || (hit.kind == kSymbolKind::kEnum) || (hit.kind == kSymbolKind::kAlias);
}

static std::u32string LastComponent(const std::u32string &name) {
    auto idxSeparator = name.rfind(U"::");
    return (idxSeparator == std::u32string::npos) ? name : name.substr(idxSeparator + 2);
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

GoToResolver::Result GoToResolver::ResolveRelated(const CodeIndex &index, const std::u32string &text, size_t idxChar, uint32_t idxLine, const std::string &path, kRelated related) {
    auto atCursor = Resolve(index, text, idxChar, idxLine, path, kMode::kDefinition);
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
