//
// Created by gnilk on 10.06.26.
//
#include <cstdio>

#include "HexView.h"
#include "Core/Action.h"
#include "Core/Config/Config.h"
#include "Core/Editor.h"
#include "Core/UI/Graphics/DrawContext.h"
#include "Core/RuntimeConfig.h"
#include "Core/TextAttributes.h"
#include "Core/UnicodeHelper.h"

using namespace gedit;

// The config.yml section for this view (keymap lookup); falls back to "hexview_keymap" if absent.
static const std::string cfgSectionName = "hexview";

void HexView::InitView() {
    logger = gnilk::Logger::GetLogger("HexView");

    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
    window->SetCaption("HexView");

    SetDocument(Editor::Instance().GetActiveDocument());
}

void HexView::ReInitView() {
    auto screen = RuntimeConfig::Instance().GetScreen();
    if (viewRect.IsEmpty()) {
        viewRect = screen->Dimensions();
    }
    window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);

    SetDocument(Editor::Instance().GetActiveDocument());
}

// Re-point at a document and (re)build the byte stream over it. The reader owns the UTF-32 -> UTF-8
// re-encode; HexView only ever reads bytes back out of it.
void HexView::SetDocument(Document::Ref newDocument) {
    document = newDocument;
    reader = (document != nullptr) ? ByteStreamReader::Create(document->GetTextBuffer()) : nullptr;
    viewTopRow = 0;
}

void HexView::OnActivate(bool isActive) {
    if (isActive) {
        Editor::Instance().SetActiveKeyMapping(Config::Instance()[cfgSectionName].GetStr("keymap", "hexview_keymap"));
    }
}

bool HexView::OnAction(const EditorAction &kpAction) {
    if ((document != nullptr) && (reader != nullptr)) {
        if (DispatchNavAction(kpAction)) {
            return true;
        }
    }
    return ViewBase::OnAction(kpAction);
}

// Navigation only (read-only view): translate the caret to byte space, move, translate back, and write
// the result into the canonical text cursor. Anything that isn't a nav action falls through.
bool HexView::DispatchNavAction(const EditorAction &kpAction) {
    switch (kpAction.action) {
        case kAction::kActionLineLeft:
        case kAction::kActionLineRight:
        case kAction::kActionLineUp:
        case kAction::kActionLineDown:
        case kAction::kActionLineHome:
        case kAction::kActionLineEnd:
        case kAction::kActionPageUp:
        case kAction::kActionPageDown:
        case kAction::kActionBufferStart:
        case kAction::kActionBufferEnd:
        case kAction::kActionGotoFirstLine:
        case kAction::kActionGotoLastLine:
            break;
        default:
            return false;
    }

    gedit::Point cursorPos(document->GetCursor().position.x, (int)document->GetLineCursor().idxActiveLine);
    gedit::Point target = ComputeNavTarget(Bytes(), cursorPos, kpAction.action, kBytesPerRow, RowsPerPage());
    document->SetCursorPosition(target.y, target.x);
    RefocusHexView(HexProjection::TextToByteOffset(target, Bytes()));
    InvalidateView();
    return true;
}

gedit::Point HexView::ComputeNavTarget(const BinBuffer &utf8, const gedit::Point &cursorPos,
                                kAction action, int bytesPerRow, int rowsPerPage) {
    size_t size = utf8.Size();
    size_t cur = HexProjection::TextToByteOffset(cursorPos, utf8);
    long long target = (long long)cur;
    long long row = (long long)(cur % (size_t)bytesPerRow);

    switch (action) {
        case kAction::kActionLineRight:
            // Step a whole char: a one-byte step into a multibyte char would snap straight back.
            target = (long long)HexProjection::NextCharStart(utf8, cur);
            break;
        case kAction::kActionLineLeft:
            // One byte back snaps to the previous char's start on the way through ByteOffsetToText.
            target = (cur > 0) ? (long long)cur - 1 : 0;
            break;
        case kAction::kActionLineDown:
            target = (long long)cur + bytesPerRow;
            break;
        case kAction::kActionLineUp:
            target = (long long)cur - bytesPerRow;
            break;
        case kAction::kActionLineHome:
            target = (long long)cur - row;
            break;
        case kAction::kActionLineEnd:
            target = (long long)cur - row + bytesPerRow - 1;
            break;
        case kAction::kActionPageDown:
            target = (long long)cur + (long long)bytesPerRow * rowsPerPage;
            break;
        case kAction::kActionPageUp:
            target = (long long)cur - (long long)bytesPerRow * rowsPerPage;
            break;
        case kAction::kActionBufferStart:
        case kAction::kActionGotoFirstLine:
            target = 0;
            break;
        case kAction::kActionBufferEnd:
        case kAction::kActionGotoLastLine:
            target = (long long)size;
            break;
        default:
            break;
    }

    if (target < 0) {
        target = 0;
    }
    if (target > (long long)size) {
        target = (long long)size;
    }
    return HexProjection::ByteOffsetToText((size_t)target, utf8);
}

void HexView::DrawViewContents() {
    if ((document == nullptr) || (reader == nullptr)) {
        return;
    }
    // Read-only view: re-encode so the dump reflects the current buffer (cheap at spike sizes).
    reader->Rebuild();

    auto &theme = Editor::Instance().GetTheme();
    auto contentColors = theme->GetContentColors();


    auto &dc = window->GetContentDC();
    dc.ResetDrawColors();
    // Fetch the correct color from the theme
    dc.SetFGColor(contentColors["hexview"]);
    dc.Clear();

    const BinBuffer &bytes = Bytes();
    size_t total = bytes.Size();

    size_t caret = CaretByteOffset();
    RefocusHexView(caret);

    int rows = dc.GetRect().Height();
    char buf[256];
    for (int r = 0; r < rows; r++) {
        size_t rowStart = (size_t)(viewTopRow + r) * (size_t)kBytesPerRow;
        if (total == 0) {
            if (r > 0) {
                break;
            }
        } else if (rowStart >= total) {
            break;
        }

        int pos = 0;
        pos += snprintf(buf + pos, sizeof(buf) - pos, "%08zX  ", rowStart);
        for (int i = 0; i < kBytesPerRow; i++) {
            if (i == 8) {
                buf[pos++] = ' ';
            }
            size_t at = rowStart + (size_t)i;
            if (at < total) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%02X ", bytes.At(at));
            } else {
                pos += snprintf(buf + pos, sizeof(buf) - pos, "   ");
            }
        }
        buf[pos++] = ' ';
        buf[pos++] = '|';
        for (int i = 0; i < kBytesPerRow; i++) {
            size_t at = rowStart + (size_t)i;
            if (at < total) {
                uint8_t b = bytes.At(at);
                buf[pos++] = ((b >= 0x20) && (b <= 0x7E)) ? (char)b : '.';
            } else {
                buf[pos++] = ' ';
            }
        }
        buf[pos++] = '|';
        buf[pos] = '\0';
        dc.DrawStringAt(0, r, buf);
    }

    // The caret: an inverted hex pair in the hex column plus the matching inverted glyph in the ASCII
    // column, and the hardware cursor parked on the hex cell.
    int caretRow = (int)(caret / (size_t)kBytesPerRow) - viewTopRow;
    int idxInRow = (int)(caret % (size_t)kBytesPerRow);
    int caretCol = 10 + idxInRow * 3 + ((idxInRow >= 8) ? 1 : 0);
    // ASCII column start = offset field (10) + hex field (kBytesPerRow*3, +1 for the mid-row gap) + " |".
    int asciiCol = 10 + kBytesPerRow * 3 + 1 + 2 + idxInRow;
    cursor.position = {caretCol, caretRow};
    if (caret < total) {
        uint8_t b = bytes.At(caret);
        char hx[3];
        snprintf(hx, sizeof(hx), "%02X", b);
        dc.DrawStringWithAttributesAt(caretCol, caretRow, kTextAttributes::kInverted, hx);

        char ascii[2] = { ((b >= 0x20) && (b <= 0x7E)) ? (char)b : '.', '\0' };
        dc.DrawStringWithAttributesAt(asciiCol, caretRow, kTextAttributes::kInverted, ascii);
    }
}

std::pair<std::u32string, std::u32string> HexView::GetStatusBarInfo() {
    std::u32string statusCenter = U"";
    std::u32string statusRight = U"";

    // If we have a document - draw details...
    auto node = Editor::Instance().GetWorkspaceNodeForActiveDocument();
    if (node == nullptr) {
        return {statusCenter, statusRight};
    }

    auto document = node->GetDocument();
    if (document == nullptr) {
        return {statusCenter, statusRight};
    }

    // Resolve center information
    // NOTE: Removed this - as we are using the bin-buffer in the HEX viewer (not an editor yet)
    // if (document->GetTextBuffer()->GetBufferState() == TextBuffer::kBuffer_Changed) {
    //     statusCenter += U"* ";
    // }
    // if (document->GetTextBuffer()->IsReadOnly()) {
    //     statusCenter += U"R/O ";
    // }

    statusCenter += node->GetDisplayNameU32();
    statusCenter += U" | ";
    if (!document->GetTextBuffer()->CanEdit()) {
        statusCenter += U"[locked] | ";
    }
    statusCenter += document->GetTextBuffer()->HaveLanguage() ? document->GetTextBuffer()->GetLanguage().Identifier() : U"none";


    if ((document != nullptr) && (reader != nullptr)) {
        char buf[64];
        snprintf(buf, sizeof(buf), "0x%08zX / %zu bytes", CaretByteOffset(), reader->Size());
        statusRight = UnicodeHelper::utf8to32(buf);
    }
    return {statusCenter, statusRight};
}

void HexView::RefocusHexView(size_t caretOffset) {
    int caretRow = (int)(caretOffset / (size_t)kBytesPerRow);
    int rows = RowsPerPage();
    if (caretRow < viewTopRow) {
        viewTopRow = caretRow;
    } else if (caretRow >= viewTopRow + rows) {
        viewTopRow = caretRow - rows + 1;
    }
    if (viewTopRow < 0) {
        viewTopRow = 0;
    }
}

int HexView::RowsPerPage() {
    if (window == nullptr) {
        return 1;
    }
    int height = window->GetContentDC().GetRect().Height();
    return (height > 0) ? height : 1;
}

size_t HexView::CaretByteOffset() {
    gedit::Point cursorPos(document->GetCursor().position.x, (int)document->GetLineCursor().idxActiveLine);
    return HexProjection::TextToByteOffset(cursorPos, Bytes());
}

const BinBuffer &HexView::Bytes() {
    return *reader->GetBuffer();
}
