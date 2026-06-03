//
// Created by gnilk on 19.07.23.
//
#include <testinterface.h>
#include <vector>
#include <string>
#include "Core/ClipBoard.h"
#include "Core/TextBuffer.h"


// Fine-grained characterization of clipboard copy/paste.
//
// The matrix below crosses every COPY SHAPE with every PASTE DESTINATION and asserts the result the
// editor SHOULD produce (a plain text-splice: pasting the copied text at the paste point, splitting
// the target line where needed). Cases the current code gets wrong are expected to FAIL until
// ClipBoard::PasteToBuffer is fixed - the failing list is the to-do map.
//
// Copy shapes (region [start,end), end exclusive):
//   fullline   - one whole line incl. its newline      {0,1}..{0,2}
//   partial    - a substring within a single line      {2,1}..{4,1}
//   multifull  - several whole lines                    {0,1}..{0,4}
//   pstart     - starts mid-line, ends at a line break  {2,1}..{0,3}
//   pend       - starts at col 0, ends mid-line         {0,1}..{3,3}
//   pboth      - starts AND ends mid-line               {2,1}..{3,3}
//
// Paste destinations:
//   empty - into a fresh buffer (one seeded empty line) at {0,0}
//   col0  - into existing text at column 0 of a line   (non-overlapping, line boundary)
//   mid   - into existing text mid-line (ptWhere.x!=0)  (overlapping - splits the target line)
//   end   - at/after the end of a line (ptWhere.x!=0)   (non-overlapping - appends)
//
// NOTE on coordinates: helper MakeNumberedBuffer() drops the seeded empty line[0], so buffer index i
// holds "<prefix> i" with no off-by-one. The "empty" destination keeps the seeded empty line, since a
// real document buffer always has at least one line.

using namespace gedit;

extern "C" {
DLL_EXPORT int test_clipboard(ITesting *t);
// copy-shape line counting
DLL_EXPORT int test_clipboard_copylines(ITesting *t);
DLL_EXPORT int test_clipboard_copylineregion(ITesting *t);
DLL_EXPORT int test_clipboard_copyclippedstart(ITesting *t);
// paste into an empty buffer
DLL_EXPORT int test_clipboard_paste_empty_fullline(ITesting *t);
DLL_EXPORT int test_clipboard_paste_empty_partial(ITesting *t);
DLL_EXPORT int test_clipboard_paste_empty_multifull(ITesting *t);
DLL_EXPORT int test_clipboard_paste_empty_pstart(ITesting *t);
DLL_EXPORT int test_clipboard_paste_empty_pend(ITesting *t);
DLL_EXPORT int test_clipboard_paste_empty_pboth(ITesting *t);
// paste at column 0 of an existing line (non-overlapping, line boundary)
DLL_EXPORT int test_clipboard_paste_col0_fullline(ITesting *t);
DLL_EXPORT int test_clipboard_paste_col0_multifull(ITesting *t);
DLL_EXPORT int test_clipboard_paste_col0_pstart(ITesting *t);
DLL_EXPORT int test_clipboard_paste_col0_pboth(ITesting *t);
// paste mid-line into existing text (overlapping - target line must split)
DLL_EXPORT int test_clipboard_paste_mid_partial(ITesting *t);
DLL_EXPORT int test_clipboard_paste_mid_fullline(ITesting *t);
DLL_EXPORT int test_clipboard_paste_mid_pboth(ITesting *t);
// paste at end of a line (non-overlapping append, ptWhere.x!=0)
DLL_EXPORT int test_clipboard_paste_end_partial(ITesting *t);
DLL_EXPORT int test_clipboard_paste_end_pboth(ITesting *t);
// external (OS) clipboard
DLL_EXPORT int test_clipboard_copypasteexternal(ITesting *t);
}

DLL_EXPORT int test_clipboard(ITesting *t) {
    return kTR_Pass;
}

// ---------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------

// Build a buffer with `n` numbered lines "<prefix> 0".."<prefix> (n-1)" and NO leading empty line, so
// buffer index i holds "<prefix> i" (1:1 coordinates - removes the seeded-line off-by-one).
static TextBuffer::Ref MakeNumberedBuffer(const char *prefix, int n) {
    auto buffer = TextBuffer::CreateEmptyBuffer();
    buffer->DeleteLineAt(0);    // drop the seeded empty line
    for (int i = 0; i < n; i++) {
        char tmp[64];
        snprintf(tmp, sizeof(tmp), "%s %d", prefix, i);
        buffer->AddLineUTF8(tmp);
    }
    return buffer;
}

// One row of the matrix.
struct PasteCase {
    Point copyStart;
    Point copyEnd;
    const char *dstPrefix;      // nullptr => empty buffer (single seeded empty line)
    int dstCount;
    Point where;
    std::vector<std::u32string> expected;
    Point expectedEnd = {-1,-1};    // caret end Point returned by PasteToBuffer; {-1,-1} => don't check
};

static int RunPasteCase(ITesting *t, const PasteCase &c) {
    auto src = MakeNumberedBuffer("line", 10);  // "line 0".."line 9"
    ClipBoard clipBoard;
    TR_ASSERT(t, clipBoard.CopyFromBuffer(src, c.copyStart, c.copyEnd));
    TR_ASSERT(t, clipBoard.Top() != nullptr);

    // The resolved paste line count must match the actual number of lines the splice produces.
    auto pasteLineCount = clipBoard.Top()->GetPasteLineCount();

    TextBuffer::Ref dst;
    if (c.dstPrefix == nullptr) {
        dst = TextBuffer::CreateEmptyBuffer();  // one seeded empty line
    } else {
        dst = MakeNumberedBuffer(c.dstPrefix, c.dstCount);
    }
    auto linesBefore = dst->NumLines();

    auto ptEnd = clipBoard.PasteToBuffer(dst, c.where);

    // Dump actual result (stderr so it shows on failure) to make iterating on asserts easy.
    for (size_t i = 0; i < dst->NumLines(); i++) {
        fprintf(stderr, "  actual[%zu] = '%s'\n", i, dst->LineAt(i)->BufferAsUTF8().c_str());
    }
    fprintf(stderr, "  ptEnd = (%d,%d)\n", ptEnd.x, ptEnd.y);

    TR_ASSERT(t, dst->NumLines() == c.expected.size());
    for (size_t i = 0; i < c.expected.size(); i++) {
        TR_ASSERT(t, dst->LineAt(i)->Buffer() == c.expected[i]);
    }

    // The number of NEW lines added equals the resolved line count minus one.
    TR_ASSERT(t, (dst->NumLines() - linesBefore) == (pasteLineCount - 1));

    if ((c.expectedEnd.x >= 0) && (c.expectedEnd.y >= 0)) {
        TR_ASSERT(t, ptEnd.x == c.expectedEnd.x);
        TR_ASSERT(t, ptEnd.y == c.expectedEnd.y);
    }
    return kTR_Pass;
}

// ---------------------------------------------------------------------------------------------------
// Copy-shape line counting (the clipboard stores whole lines; the x-offsets are applied at paste)
// ---------------------------------------------------------------------------------------------------

DLL_EXPORT int test_clipboard_copylines(ITesting *t) {
    auto src = MakeNumberedBuffer("line", 10);
    ClipBoard clipBoard;
    TR_ASSERT(t, clipBoard.Top() == nullptr);
    // {0,2}..{0,5}: whole lines 2,3,4 (end at col 0 of line 5 => line 5 excluded)
    TR_ASSERT(t, clipBoard.CopyFromBuffer(src, {0,2}, {0,5}));
    TR_ASSERT(t, clipBoard.Top() != nullptr);
    TR_ASSERT(t, clipBoard.Top()->GetLineCount() == 3);
    return kTR_Pass;
}

DLL_EXPORT int test_clipboard_copylineregion(ITesting *t) {
    auto src = MakeNumberedBuffer("line", 10);
    ClipBoard clipBoard;
    TR_ASSERT(t, clipBoard.Top() == nullptr);
    // {2,2}..{4,2}: a substring within a single line => 1 line spanned
    TR_ASSERT(t, clipBoard.CopyFromBuffer(src, {2,2}, {4,2}));
    TR_ASSERT(t, clipBoard.Top() != nullptr);
    TR_ASSERT(t, clipBoard.Top()->GetLineCount() == 1);
    return kTR_Pass;
}

DLL_EXPORT int test_clipboard_copyclippedstart(ITesting *t) {
    auto src = MakeNumberedBuffer("line", 10);
    ClipBoard clipBoard;
    TR_ASSERT(t, clipBoard.Top() == nullptr);
    // {3,2}..{0,5}: starts mid-line 2, ends at col 0 of line 5 => lines 2,3,4 spanned
    TR_ASSERT(t, clipBoard.CopyFromBuffer(src, {3,2}, {0,5}));
    TR_ASSERT(t, clipBoard.Top() != nullptr);
    TR_ASSERT(t, clipBoard.Top()->GetLineCount() == 3);
    return kTR_Pass;
}

// ---------------------------------------------------------------------------------------------------
// Paste into an EMPTY buffer at {0,0}
// ---------------------------------------------------------------------------------------------------

DLL_EXPORT int test_clipboard_paste_empty_fullline(ITesting *t) {
    return RunPasteCase(t, { {0,1}, {0,2}, nullptr, 0, {0,0},
                             { U"line 1", U"" }, {0,1} });
}
DLL_EXPORT int test_clipboard_paste_empty_partial(ITesting *t) {
    return RunPasteCase(t, { {2,1}, {4,1}, nullptr, 0, {0,0},
                             { U"ne" }, {2,0} });
}
DLL_EXPORT int test_clipboard_paste_empty_multifull(ITesting *t) {
    return RunPasteCase(t, { {0,1}, {0,4}, nullptr, 0, {0,0},
                             { U"line 1", U"line 2", U"line 3", U"" }, {0,3} });
}
DLL_EXPORT int test_clipboard_paste_empty_pstart(ITesting *t) {
    return RunPasteCase(t, { {2,1}, {0,3}, nullptr, 0, {0,0},
                             { U"ne 1", U"line 2", U"" }, {0,2} });
}
DLL_EXPORT int test_clipboard_paste_empty_pend(ITesting *t) {
    return RunPasteCase(t, { {0,1}, {3,3}, nullptr, 0, {0,0},
                             { U"line 1", U"line 2", U"lin" }, {3,2} });
}
DLL_EXPORT int test_clipboard_paste_empty_pboth(ITesting *t) {
    return RunPasteCase(t, { {2,1}, {3,3}, nullptr, 0, {0,0},
                             { U"ne 1", U"line 2", U"lin" }, {3,2} });
}

// ---------------------------------------------------------------------------------------------------
// Paste at column 0 of an existing line  (dst = "DST 0".."DST 4", paste at {0,2})
// ---------------------------------------------------------------------------------------------------

DLL_EXPORT int test_clipboard_paste_col0_fullline(ITesting *t) {
    return RunPasteCase(t, { {0,1}, {0,2}, "DST", 5, {0,2},
                             { U"DST 0", U"DST 1", U"line 1", U"DST 2", U"DST 3", U"DST 4" }, {0,3} });
}
DLL_EXPORT int test_clipboard_paste_col0_multifull(ITesting *t) {
    return RunPasteCase(t, { {0,1}, {0,4}, "DST", 5, {0,2},
                             { U"DST 0", U"DST 1", U"line 1", U"line 2", U"line 3",
                               U"DST 2", U"DST 3", U"DST 4" } });
}
DLL_EXPORT int test_clipboard_paste_col0_pstart(ITesting *t) {
    return RunPasteCase(t, { {2,1}, {0,3}, "DST", 5, {0,2},
                             { U"DST 0", U"DST 1", U"ne 1", U"line 2", U"DST 2", U"DST 3", U"DST 4" } });
}
DLL_EXPORT int test_clipboard_paste_col0_pboth(ITesting *t) {
    return RunPasteCase(t, { {2,1}, {3,3}, "DST", 5, {0,2},
                             { U"DST 0", U"DST 1", U"ne 1", U"line 2", U"linDST 2", U"DST 3", U"DST 4" } });
}

// ---------------------------------------------------------------------------------------------------
// Paste MID-LINE into existing text (overlapping - the target line must split)
// dst = "DST 0".."DST 4"; "DST 2" = D,S,T,' ',2; paste at {2,2} => head "DS" | tail "T 2"
// ---------------------------------------------------------------------------------------------------

DLL_EXPORT int test_clipboard_paste_mid_partial(ITesting *t) {
    return RunPasteCase(t, { {2,1}, {4,1}, "DST", 5, {2,2},
                             { U"DST 0", U"DST 1", U"DSneT 2", U"DST 3", U"DST 4" }, {4,2} });
}
DLL_EXPORT int test_clipboard_paste_mid_fullline(ITesting *t) {
    return RunPasteCase(t, { {0,1}, {0,2}, "DST", 5, {2,2},
                             { U"DST 0", U"DST 1", U"DSline 1", U"T 2", U"DST 3", U"DST 4" }, {0,3} });
}
DLL_EXPORT int test_clipboard_paste_mid_pboth(ITesting *t) {
    return RunPasteCase(t, { {2,1}, {3,3}, "DST", 5, {2,2},
                             { U"DST 0", U"DST 1", U"DSne 1", U"line 2", U"linT 2", U"DST 3", U"DST 4" }, {3,4} });
}

// ---------------------------------------------------------------------------------------------------
// Paste at the END of a line (non-overlapping append, ptWhere.x != 0)
// "DST 2" has length 5; paste at {5,2} => head "DST 2" | tail ""
// ---------------------------------------------------------------------------------------------------

DLL_EXPORT int test_clipboard_paste_end_partial(ITesting *t) {
    return RunPasteCase(t, { {2,1}, {4,1}, "DST", 5, {5,2},
                             { U"DST 0", U"DST 1", U"DST 2ne", U"DST 3", U"DST 4" }, {7,2} });
}
DLL_EXPORT int test_clipboard_paste_end_pboth(ITesting *t) {
    return RunPasteCase(t, { {2,1}, {3,3}, "DST", 5, {5,2},
                             { U"DST 0", U"DST 1", U"DST 2ne 1", U"line 2", U"lin", U"DST 3", U"DST 4" }, {3,4} });
}

// ---------------------------------------------------------------------------------------------------
// External (OS) clipboard
// ---------------------------------------------------------------------------------------------------

DLL_EXPORT int test_clipboard_copypasteexternal(ITesting *t) {
    ClipBoard clipBoard;
    auto dstBuffer = TextBuffer::CreateEmptyBuffer();
    auto strData = "hello\nthis is\na multiline\ncopy";   // 4 lines, no trailing newline
    clipBoard.CopyFromExternal(strData);
    clipBoard.PasteToBuffer(dstBuffer, {0,0});
    TR_ASSERT(t, dstBuffer->NumLines() != 0);
    // Splicing 4 lines into an empty doc yields 4 lines (no phantom trailing empty line).
    TR_ASSERT(t, dstBuffer->NumLines() == 4);
    TR_ASSERT(t, dstBuffer->LineAt(0)->Buffer() == U"hello");
    TR_ASSERT(t, dstBuffer->LineAt(3)->Buffer() == U"copy");
    return kTR_Pass;
}
