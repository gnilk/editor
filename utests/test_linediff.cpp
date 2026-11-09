//
// LineDiff (docs/usability-round.md §0c) - the change marks of a text against its base, on strings
//
#include <testinterface.h>

#include <string>
#include <vector>

#include "Core/Line.h"
#include "Core/LineDiff.h"

using namespace gedit;

extern "C" {
    DLL_EXPORT int test_linediff(ITesting *t);
    DLL_EXPORT int test_linediff_same(ITesting *t);
    DLL_EXPORT int test_linediff_added(ITesting *t);
    DLL_EXPORT int test_linediff_removed(ITesting *t);
    DLL_EXPORT int test_linediff_changed(ITesting *t);
    DLL_EXPORT int test_linediff_moved(ITesting *t);
    DLL_EXPORT int test_linediff_empty(ITesting *t);
    DLL_EXPORT int test_linediff_large(ITesting *t);
    DLL_EXPORT int test_linediff_limit(ITesting *t);
    DLL_EXPORT int test_linediff_sweep(ITesting *t);
}

namespace {
    std::vector<uint64_t> Hashes(const std::vector<std::u32string> &lines) {
        std::vector<uint64_t> hashes;
        for (auto &line : lines) {
            hashes.push_back(Line::HashOf(line));
        }
        return hashes;
    }

    // The marks as a string, one character per line: '.' none, 'A' added, 'M' modified; '^' / 'v' after it for a
    // removal above / below
    std::string Marks(const std::vector<std::u32string> &base, const std::vector<std::u32string> &current,
                      size_t maxEdits = LineDiff::kDefaultMaxEdits) {
        auto changes = LineDiff::Compute(Hashes(base), Hashes(current), maxEdits);
        std::string out;
        for (auto mark : changes.marks) {
            out += (mark & LineDiff::kAdded) ? 'A' : (mark & LineDiff::kModified) ? 'M' : '.';
            if (mark & LineDiff::kRemovedAbove) {
                out += '^';
            }
            if (mark & LineDiff::kRemovedBelow) {
                out += 'v';
            }
        }
        return out;
    }

    const std::vector<std::u32string> kBase = {U"a", U"b", U"c", U"d", U"e"};
}

DLL_EXPORT int test_linediff(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_linediff_same(ITesting *t) {
    TR_ASSERT(t, Marks(kBase, kBase) == ".....");
    TR_ASSERT(t, LineDiff::Compute(Hashes(kBase), Hashes(kBase)).IsEmpty());
    return kTR_Pass;
}

DLL_EXPORT int test_linediff_added(ITesting *t) {
    TR_ASSERT(t, Marks(kBase, {U"x", U"a", U"b", U"c", U"d", U"e"}) == "A.....");
    TR_ASSERT(t, Marks(kBase, {U"a", U"b", U"x", U"y", U"c", U"d", U"e"}) == "..AA...");
    TR_ASSERT(t, Marks(kBase, {U"a", U"b", U"c", U"d", U"e", U"x"}) == ".....A");
    return kTR_Pass;
}

DLL_EXPORT int test_linediff_removed(ITesting *t) {
    // The mark is on the line after the removed ones
    TR_ASSERT(t, Marks(kBase, {U"b", U"c", U"d", U"e"}) == ".^...");
    TR_ASSERT(t, Marks(kBase, {U"a", U"b", U"e"}) == "...^");
    // The end removed: on the last line
    TR_ASSERT(t, Marks(kBase, {U"a", U"b", U"c"}) == "...v");
    return kTR_Pass;
}

DLL_EXPORT int test_linediff_changed(ITesting *t) {
    TR_ASSERT(t, Marks(kBase, {U"a", U"B", U"c", U"d", U"e"}) == ".M...");
    // Two changed, one more added
    TR_ASSERT(t, Marks(kBase, {U"a", U"B", U"C", U"X", U"d", U"e"}) == ".MMA..");
    // Three replaced by one: changed, and removed below it
    TR_ASSERT(t, Marks(kBase, {U"a", U"X", U"e"}) == ".M.^");
    return kTR_Pass;
}

DLL_EXPORT int test_linediff_moved(ITesting *t) {
    // A block moved down: removed where it was, added where it is
    auto marks = Marks(kBase, {U"c", U"d", U"a", U"b", U"e"});
    TR_ASSERT(t, marks.length() >= 5);
    TR_ASSERT(t, marks.find('A') != std::string::npos);
    TR_ASSERT(t, marks.find('^') != std::string::npos);
    TR_ASSERT(t, marks.back() == '.');
    return kTR_Pass;
}

DLL_EXPORT int test_linediff_empty(ITesting *t) {
    // No base: all added
    TR_ASSERT(t, Marks({}, {U"a", U"b"}) == "AA");
    TR_ASSERT(t, LineDiff::AllAdded(3).marks.size() == 3);
    // Everything removed - an empty buffer still has its one line
    TR_ASSERT(t, Marks(kBase, {U""}) == "Mv");
    TR_ASSERT(t, Marks(kBase, {}) == "");
    return kTR_Pass;
}

DLL_EXPORT int test_linediff_large(ITesting *t) {
    // A large file with one change: found exactly
    std::vector<std::u32string> base;
    for (int i = 0; i < 100000; i++) {
        base.push_back(U"line " + std::u32string(1, (char32_t)(U'a' + i % 26)) + std::u32string(i % 7, U'x') + U"#" +
                       std::u32string(1, (char32_t)(0x4e00 + i)));
    }
    auto current = base;
    current[50000] = U"changed";
    current.insert(current.begin() + 70000, U"new");
    auto changes = LineDiff::Compute(Hashes(base), Hashes(current));
    for (size_t i = 0; i < changes.marks.size(); i++) {
        uint8_t expected = (i == 50000) ? LineDiff::kModified : (i == 70000) ? LineDiff::kAdded : LineDiff::kNone;
        TR_ASSERT(t, changes.marks[i] == expected);
    }
    return kTR_Pass;
}

DLL_EXPORT int test_linediff_limit(ITesting *t) {
    // Past the limit: the middle - from the first difference to the last - is marked changed, nothing outside it
    std::vector<std::u32string> base = {U"head", U"a", U"b", U"c", U"d", U"tail"};
    std::vector<std::u32string> current = {U"head", U"w", U"x", U"y", U"z", U"tail"};
    TR_ASSERT(t, Marks(base, current, 2) == ".MMMM.");
    return kTR_Pass;
}

// Any base and edit: one mark per current line, an unchanged line is unmarked only if the edit left it, and the
// number of added + changed lines is what was put in
DLL_EXPORT int test_linediff_sweep(ITesting *t) {
    for (size_t idxAt = 0; idxAt <= kBase.size(); idxAt++) {
        for (size_t numRemoved = 0; idxAt + numRemoved <= kBase.size(); numRemoved++) {
            for (size_t numInserted = 0; numInserted <= 3; numInserted++) {
                auto current = kBase;
                current.erase(current.begin() + idxAt, current.begin() + idxAt + numRemoved);
                for (size_t i = 0; i < numInserted; i++) {
                    current.insert(current.begin() + idxAt + i, U"new" + std::u32string(1, (char32_t)(U'0' + i)));
                }
                auto changes = LineDiff::Compute(Hashes(kBase), Hashes(current));
                TR_ASSERT(t, changes.marks.size() == current.size());
                size_t numMarked = 0;
                bool hasRemoval = false;
                for (size_t i = 0; i < current.size(); i++) {
                    auto mark = changes.marks[i];
                    bool isNew = (i >= idxAt) && (i < idxAt + numInserted);
                    TR_ASSERT(t, isNew == ((mark & (LineDiff::kAdded | LineDiff::kModified)) != 0));
                    numMarked += isNew ? 1 : 0;
                    hasRemoval = hasRemoval || ((mark & (LineDiff::kRemovedAbove | LineDiff::kRemovedBelow)) != 0);
                }
                TR_ASSERT(t, numMarked == numInserted);
                TR_ASSERT(t, hasRemoval == ((numRemoved > numInserted) && !current.empty()));
            }
        }
    }
    return kTR_Pass;
}
