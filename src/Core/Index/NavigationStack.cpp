//
// Back and forward through jumps - see NavigationStack.h
//

#include <limits>

#include "NavigationStack.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

// Two overloads share a name, so the symbol's own line counts too. Edits between the two can make the same place look
// like two: an entry too many, which is harmless.
static bool IsSamePlace(const NavigationStack::Entry &a, const NavigationStack::Entry &b) {
    if (a.path != b.path) {
        return false;
    }
    if (a.symbol.empty() || b.symbol.empty()) {
        return a.line == b.line;
    }
    return (a.symbol == b.symbol) && (((int64_t)a.line - a.lineOffset) == ((int64_t)b.line - b.lineOffset));
}

NavigationStack::Entry NavigationStack::MakeEntry(const CodeIndex *index, const std::string &path, uint32_t line, uint32_t column) {
    Entry entry;
    entry.path = path;
    entry.line = line;
    entry.column = column;
    if (index == nullptr) {
        return entry;
    }
    auto nearest = index->NearestSymbol(path, line);
    if (nearest.has_value()) {
        entry.symbol = nearest->qualifiedName;
        entry.lineOffset = (int32_t)line - (int32_t)nearest->line;
    }
    return entry;
}

uint32_t NavigationStack::CurrentLine(const CodeIndex *index, const Entry &entry) {
    if ((index == nullptr) || entry.symbol.empty()) {
        return entry.line;
    }
    auto idxSeparator = entry.symbol.rfind(U"::");
    auto name = (idxSeparator == std::u32string::npos) ? entry.symbol : entry.symbol.substr(idxSeparator + 2);
    auto wasAt = (int64_t)entry.line - entry.lineOffset;

    // An overload keeps a name too: the closest one to where it was
    auto bestDistance = std::numeric_limits<int64_t>::max();
    auto line = entry.line;
    for (auto &hit : index->FindExact(name)) {
        if ((hit.path != entry.path) || (hit.qualifiedName != entry.symbol)) {
            continue;
        }
        auto distance = std::abs((int64_t)hit.line - wasAt);
        if (distance < bestDistance) {
            bestDistance = distance;
            line = (uint32_t)std::max((int64_t)0, (int64_t)hit.line + entry.lineOffset);
        }
    }
    return line;
}

void NavigationStack::Jump(const Entry &from, const Entry &to) {
    bool hasFrom = !from.path.empty();
    if (hasFrom && IsSamePlace(from, to)) {
        if (!entries.empty()) {
            entries[idxCurrent] = to;
        }
        return;
    }
    if (hasFrom && (idxCurrent > 0) && IsSamePlace(entries[idxCurrent - 1], to)) {
        entries[idxCurrent] = from;
        idxCurrent--;
        entries[idxCurrent] = to;
        return;
    }
    if (!entries.empty()) {
        entries.resize(idxCurrent + 1);
    }
    if (hasFrom) {
        if (entries.empty()) {
            entries.push_back(from);
        } else {
            entries.back() = from;
        }
    }
    entries.push_back(to);
    if (entries.size() > MAX_ENTRIES) {
        entries.erase(entries.begin());
    }
    idxCurrent = entries.size() - 1;
}

std::optional<NavigationStack::Entry> NavigationStack::Back(const Entry &current) {
    if (entries.empty() || (idxCurrent == 0)) {
        return {};
    }
    entries[idxCurrent] = current;
    idxCurrent--;
    return entries[idxCurrent];
}

std::optional<NavigationStack::Entry> NavigationStack::Forward(const Entry &current) {
    if (entries.empty() || ((idxCurrent + 1) >= entries.size())) {
        return {};
    }
    entries[idxCurrent] = current;
    idxCurrent++;
    return entries[idxCurrent];
}

void NavigationStack::Clear() {
    entries.clear();
    idxCurrent = 0;
}

NavigationSession NavigationStack::ToSession() const {
    NavigationSession session;
    session.idxCurrent = idxCurrent;
    for (auto &entry : entries) {
        session.entries.push_back({entry.path, UnicodeHelper::utf32to8(entry.symbol), entry.lineOffset, entry.line, entry.column});
    }
    return session;
}

void NavigationStack::FromSession(const NavigationSession &session) {
    Clear();
    for (auto &entry : session.entries) {
        entries.push_back({entry.path, UnicodeHelper::utf8to32(entry.symbol), entry.lineOffset, entry.line, entry.column});
    }
    idxCurrent = entries.empty() ? 0 : std::min(session.idxCurrent, entries.size() - 1);
}
