//
// The rows of a relationship tree - see RelationshipTree.h
//

#include <algorithm>
#include <filesystem>
#include <tuple>

#include "RelationshipTree.h"

using namespace gedit;

static bool IsTypeKind(kSymbolKind kind) {
    return (kind == kSymbolKind::kClass) || (kind == kSymbolKind::kStruct) || (kind == kSymbolKind::kUnion) ||
           (kind == kSymbolKind::kEnum) || (kind == kSymbolKind::kAlias);
}

static std::u32string LastComponent(const std::u32string &name) {
    auto idxSeparator = name.rfind(U"::");
    return (idxSeparator == std::u32string::npos) ? name : name.substr(idxSeparator + 2);
}

std::vector<std::u32string> RelationshipTree::TypesAt(const CodeIndex &index, const std::u32string &name, const std::string &path, uint32_t line) {
    if (!name.empty()) {
        auto types = index.TypesNamed(name);
        if (!types.empty()) {
            return types;
        }
    }
    auto nearest = index.NearestSymbol(path, line);
    if (!nearest.has_value()) {
        return {};
    }
    if (IsTypeKind(nearest->kind)) {
        return {nearest->qualifiedName};
    }
    auto idxScopeEnd = nearest->qualifiedName.rfind(U"::");
    if (idxScopeEnd == std::u32string::npos) {
        return {};
    }
    // Out of line it is written 'EditorView::Draw', and the type is 'gedit::EditorView'
    return index.TypesNamed(nearest->qualifiedName.substr(0, idxScopeEnd));
}

RelationshipTree::Row RelationshipTree::TypeRow(const CodeIndex &index, const std::u32string &type) {
    Row row;
    row.text = type;
    row.types = {type};
    for (auto &hit : index.FindExact(LastComponent(type))) {
        if ((hit.qualifiedName != type) || !IsTypeKind(hit.kind)) {
            continue;
        }
        if (row.path.empty() || hit.isDefinition) {
            row.path = hit.path;
            row.line = hit.line;
            row.column = hit.column;
        }
        if (hit.isDefinition) {
            break;
        }
    }
    return row;
}

static RelationshipTree::Row RelationRow(const CodeIndex::Relation &relation, bool isToTarget, const std::set<std::u32string> &ancestors) {
    RelationshipTree::Row row;
    row.text = isToTarget ? relation.toName : relation.fromName;
    row.ownership = relation.ownership;
    row.path = relation.path;
    row.line = relation.line;
    row.places.push_back({relation.path, relation.line});
    if (!isToTarget) {
        row.types = {relation.fromName};
    } else if (!relation.isMacro) {
        row.types = relation.targets;
    }
    row.isOutside = isToTarget && relation.targets.empty();
    row.isMacro = isToTarget && relation.isMacro;
    if ((row.types.size() == 1) && (ancestors.count(row.types[0]) != 0)) {
        row.isRepeat = true;
        row.types.clear();
    }
    return row;
}

// What says something about the architecture first: a macro type or a name from outside the index goes last
static std::vector<RelationshipTree::Row> MergedRows(const std::vector<CodeIndex::Relation> &relations, bool isToTarget, const std::set<std::u32string> &ancestors) {
    std::vector<RelationshipTree::Row> rows;
    for (auto &relation : relations) {
        auto row = RelationRow(relation, isToTarget, ancestors);
        auto it = std::find_if(rows.begin(), rows.end(), [&row](const RelationshipTree::Row &other) {
            return (other.text == row.text) && (other.ownership == row.ownership) && (other.types == row.types) && (other.isRepeat == row.isRepeat);
        });
        if (it != rows.end()) {
            it->places.push_back(row.places[0]);
        } else {
            rows.push_back(std::move(row));
        }
    }
    std::stable_sort(rows.begin(), rows.end(), [](const RelationshipTree::Row &a, const RelationshipTree::Row &b) {
        return std::make_tuple(a.isOutside || a.isMacro, (int)a.ownership) < std::make_tuple(b.isOutside || b.isMacro, (int)b.ownership);
    });
    return rows;
}

std::vector<RelationshipTree::Group> RelationshipTree::RelationsOf(const CodeIndex &index, const std::u32string &type, const std::set<std::u32string> &ancestors) {
    std::vector<Group> groups;
    auto addGroup = [&groups, &ancestors](const char32_t *title, const std::vector<CodeIndex::Relation> &relations, bool isToTarget) {
        if (relations.empty()) {
            return;
        }
        Group group;
        group.title.text = title;
        group.rows = MergedRows(relations, isToTarget, ancestors);
        groups.push_back(std::move(group));
    };
    addGroup(U"Bases", index.BasesOf(type), true);
    addGroup(U"Derived", index.DerivedFrom(type), false);
    addGroup(U"Holds", index.HoldsOf(type), true);
    addGroup(U"Held by", index.HeldBy(type), false);
    return groups;
}

RelationshipTree::Row RelationshipTree::SourceRow(const Place &place, const std::u32string &lineText) {
    Row row;
    row.isSource = true;
    row.path = place.path;
    row.line = place.line;
    auto location = std::filesystem::path(place.path).filename().string() + ":" + std::to_string(place.line + 1);
    row.text.assign(location.begin(), location.end());
    auto idxText = lineText.find_first_not_of(U" \t");
    if (idxText != std::u32string::npos) {
        row.text += U"  ";
        row.text += lineText.substr(idxText);
    }
    return row;
}

std::u32string RelationshipTree::Label(const Row &row) {
    std::u32string label;
    if (row.ownership != kOwnership::kNone) {
        std::string ownership = OwnershipName(row.ownership);
        label.assign(ownership.begin(), ownership.end());
        label += U" ";
    }
    label += row.text;
    if (row.places.size() > 1) {
        auto count = std::to_string(row.places.size());
        label += U" x";
        label.append(count.begin(), count.end());
    }
    if (row.isRepeat) {
        label += U" (above)";
    } else if (row.isOutside) {
        label += U" (outside the index)";
    } else if (row.types.size() > 1) {
        auto count = std::to_string(row.types.size());
        label += U" (";
        label.append(count.begin(), count.end());
        label += U" candidates)";
    }
    return label;
}
