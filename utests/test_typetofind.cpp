//
// TypeToFind (docs/usability-round.md §3 a) - letters typed in a tree jump to the next matching name, on strings
//
#include <testinterface.h>

#include <chrono>
#include <string>
#include <vector>

#include "Core/UI/TypeToFind.h"

using namespace gedit;
using namespace std::chrono_literals;

extern "C" {
    DLL_EXPORT int test_typetofind(ITesting *t);
    DLL_EXPORT int test_typetofind_prefix(ITesting *t);
    DLL_EXPORT int test_typetofind_grow(ITesting *t);
    DLL_EXPORT int test_typetofind_step(ITesting *t);
    DLL_EXPORT int test_typetofind_repeat(ITesting *t);
    DLL_EXPORT int test_typetofind_wrap(ITesting *t);
    DLL_EXPORT int test_typetofind_case(ITesting *t);
    DLL_EXPORT int test_typetofind_none(ITesting *t);
}

namespace {
    // The workspace tree as it is drawn - folders end in '/'
    const std::vector<std::u32string> kNames = {U"editor/", U"Assets/", U"build/", U"docs/", U"dd.txt", U"debug.log",
                                               U"main.cpp", U"src/", U"Document.cpp"};
}

DLL_EXPORT int test_typetofind(ITesting *t) {
    return kTR_Pass;
}

// The letters within a pause are one prefix; after the pause, or a reset, the next letter starts again
DLL_EXPORT int test_typetofind_prefix(ITesting *t) {
    TypeToFind find(1000ms);
    auto now = TypeToFind::Clock::now();
    TR_ASSERT(t, find.Type(U'd', now) == U"d");
    TR_ASSERT(t, find.Type(U'o', now + 300ms) == U"do");
    TR_ASSERT(t, find.Type(U'c', now + 1200ms) == U"doc");     // the pause counts from the last letter
    TR_ASSERT(t, find.Type(U'm', now + 2300ms) == U"m");
    find.Reset();
    TR_ASSERT(t, find.Prefix().empty());
    TR_ASSERT(t, find.Type(U's', now + 2400ms) == U"s");
    return kTR_Pass;
}

// While the prefix grows, the row it is on still counts
DLL_EXPORT int test_typetofind_grow(ITesting *t) {
    // On 'docs/': "do" stays, "doc" stays, "docu" goes on to Document.cpp
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 3, U"do") == 3u);
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 3, U"doc") == 3u);
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 3, U"docu") == 8u);
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 0, U"ma") == 6u);
    return kTR_Pass;
}

// A single letter goes to the next row with it, not the one it is on
DLL_EXPORT int test_typetofind_step(ITesting *t) {
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 0, U"d") == 3u);
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 3, U"d") == 4u);
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 4, U"d") == 5u);
    return kTR_Pass;
}

// One letter again and again steps through the rows with it - unless a name starts with the repeat ("dd.txt")
DLL_EXPORT int test_typetofind_repeat(ITesting *t) {
    // On 'docs/', "dd": a name starts with it - that one
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 3, U"dd") == 4u);
    // On 'dd.txt', "ddd": none does - the next 'd'
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 4, U"ddd") == 5u);
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 5, U"dddd") == 8u);
    // Not the same letter: no stepping
    TR_ASSERT(t, !TypeToFind::FindNext(kNames, 3, U"dx").has_value());
    return kTR_Pass;
}

// Past the last row the search goes on from the top
DLL_EXPORT int test_typetofind_wrap(ITesting *t) {
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 8, U"e") == 0u);
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 8, U"d") == 3u);
    // The only match is the row we are on: a single letter stays there
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 6, U"m") == 6u);
    return kTR_Pass;
}

// Case doesn't count, either way
DLL_EXPORT int test_typetofind_case(ITesting *t) {
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 0, U"a") == 1u);
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 0, U"DOCU") == 8u);
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 0, U"Dd") == 4u);
    return kTR_Pass;
}

// Nothing matches, nothing to search, or no prefix: no row - the selection stays
DLL_EXPORT int test_typetofind_none(ITesting *t) {
    TR_ASSERT(t, !TypeToFind::FindNext(kNames, 0, U"zz").has_value());
    TR_ASSERT(t, !TypeToFind::FindNext({}, 0, U"d").has_value());
    TR_ASSERT(t, !TypeToFind::FindNext(kNames, 0, U"").has_value());
    // A current row past the end (the tree just shrank) searches from the top
    TR_ASSERT(t, TypeToFind::FindNext(kNames, 99, U"ma") == 6u);
    return kTR_Pass;
}
