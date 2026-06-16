//
// Created by gnilk on 10.06.26.
//
// H.3 of the HexView spike: the read-only hex view's navigation/translation. HexView::ComputeNavTarget
// is pure over a BinBuffer (the same byte stream HexProjection/ByteStreamReader produce), so it is
// tested here directly with hand-built buffers - horizontal moves step whole chars (never stuck mid
// multibyte), vertical/page/home/end move in 16-byte rows then snap to a char boundary. The write-back
// path (byte-space move -> canonical text cursor) is pinned via Document::SetCursorPosition.
//
#include <testinterface.h>

#include <string>
#include <vector>

#include "Core/Action.h"
#include "Core/BinBuffer.h"
#include "Core/Document.h"
#include "Core/HexProjection.h"
#include "Core/TextBuffer.h"
#include "Core/Editor/Views/HexView.h"

using namespace gedit;

extern "C" {
DLL_EXPORT int test_hexview(ITesting *t);
DLL_EXPORT int test_hexview_navtarget_rows(ITesting *t);
DLL_EXPORT int test_hexview_navtarget_multibyte(ITesting *t);
DLL_EXPORT int test_hexview_setcursor(ITesting *t);
}

static BinBuffer::Ref Bytes(const std::string &raw) {
    return BinBuffer::CreateFrom(reinterpret_cast<const uint8_t *>(raw.data()), raw.size());
}

// Convenience: run ComputeNavTarget with the fixed row width and a given page height.
static Point Nav(const BinBuffer &bin, const Point &from, kAction action, int rowsPerPage) {
    return HexView::ComputeNavTarget(bin, from, action, HexView::kBytesPerRow, rowsPerPage);
}

DLL_EXPORT int test_hexview(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_hexview_navtarget_rows(ITesting *t) {
    // 40 single-byte chars, no separators -> all on text line 0; clean row arithmetic (16/row).
    auto bin = Bytes(std::string(40, 'x'));
    TR_ASSERT(t, bin->Size() == 40);
    const int rpp = 4;

    // Horizontal: one char each way (== one byte here, all ASCII).
    TR_ASSERT(t, Nav(*bin, {0, 0}, kAction::kActionLineRight, rpp).x == 1);
    TR_ASSERT(t, Nav(*bin, {0, 0}, kAction::kActionLineLeft, rpp).x == 0);   // clamp at start

    // Vertical: +/- one 16-byte row.
    TR_ASSERT(t, Nav(*bin, {0, 0}, kAction::kActionLineDown, rpp).x == 16);
    TR_ASSERT(t, Nav(*bin, {16, 0}, kAction::kActionLineUp, rpp).x == 0);
    TR_ASSERT(t, Nav(*bin, {0, 0}, kAction::kActionLineUp, rpp).x == 0);     // clamp at top

    // Home/End snap to the byte row boundaries.
    TR_ASSERT(t, Nav(*bin, {20, 0}, kAction::kActionLineHome, rpp).x == 16);
    TR_ASSERT(t, Nav(*bin, {20, 0}, kAction::kActionLineEnd, rpp).x == 31);  // 16..31
    TR_ASSERT(t, Nav(*bin, {33, 0}, kAction::kActionLineEnd, rpp).x == 40);  // 32..47 clamps to size

    // Page = rowsPerPage rows; clamps to [0, size].
    TR_ASSERT(t, Nav(*bin, {0, 0}, kAction::kActionPageDown, rpp).x == 40);  // 0 + 16*4 clamps to 40
    TR_ASSERT(t, Nav(*bin, {40, 0}, kAction::kActionPageUp, rpp).x == 0);

    // Buffer start/end.
    TR_ASSERT(t, Nav(*bin, {20, 0}, kAction::kActionBufferStart, rpp).x == 0);
    TR_ASSERT(t, Nav(*bin, {0, 0}, kAction::kActionBufferEnd, rpp).x == 40);
    return kTR_Pass;
}

DLL_EXPORT int test_hexview_navtarget_multibyte(ITesting *t) {
    // 'a'(1) + U+20AC euro(3) + 'b'(1), single line. Chars: 0='a'@0, 1='euro'@1, 2='b'@4.
    auto bin = Bytes("a\xE2\x82\xAC" "b");
    TR_ASSERT(t, bin->Size() == 5);
    const int rpp = 4;

    // Right steps WHOLE chars - the one-byte-into-multibyte snap-back never traps the caret.
    TR_ASSERT(t, Nav(*bin, {0, 0}, kAction::kActionLineRight, rpp).x == 1);   // 'a' -> euro
    TR_ASSERT(t, Nav(*bin, {1, 0}, kAction::kActionLineRight, rpp).x == 2);   // euro -> 'b' (skips 2 cont. bytes)
    TR_ASSERT(t, Nav(*bin, {2, 0}, kAction::kActionLineRight, rpp).x == 3);   // 'b' -> EOL

    // Left snaps to the previous char's start (a byte landing mid-euro resolves to the euro).
    TR_ASSERT(t, Nav(*bin, {1, 0}, kAction::kActionLineLeft, rpp).x == 0);    // euro -> 'a'
    TR_ASSERT(t, Nav(*bin, {2, 0}, kAction::kActionLineLeft, rpp).x == 1);    // 'b' -> euro
    return kTR_Pass;
}

DLL_EXPORT int test_hexview_setcursor(ITesting *t) {
    // The hex->text write-back: SetCursorPosition lands the canonical cursor on an absolute (line,char).
    auto textBuffer = TextBuffer::CreateEmptyBuffer();
    textBuffer->DeleteLineAt(0);
    textBuffer->AddLine(U"first");
    textBuffer->AddLine(U"second");
    textBuffer->AddLine(U"third");
    auto document = Document::Create(textBuffer);
    document->OnViewInit(Rect(20, 20));

    document->SetCursorPosition(1, 2);
    TR_ASSERT(t, document->GetLineCursor().idxActiveLine == 1);
    TR_ASSERT(t, document->GetCursor().position.x == 2);
    TR_ASSERT(t, document->GetCursor().position.y == 1);
    return kTR_Pass;
}
