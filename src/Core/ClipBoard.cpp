//
// Created by gnilk on 19.07.23.
//

#include "ClipBoard.h"
#include <string>
#include <sstream>
#include "UnicodeHelper.h"
using namespace gedit;

bool ClipBoard::CopyFromBuffer(TextBuffer::Ref srcBuffer, const Point &ptStart, const Point &ptEnd) {
    auto item = ClipBoardItem::Create(srcBuffer, ptStart, ptEnd);
    history.push_front(item);
    NotifyChangeHandler(item);
    return true;
}

bool ClipBoard::CopyFromExternal(const char *srcBuffer) {
    auto item = ClipBoardItem::CreateExternal(srcBuffer);
    history.push_front(item);
    // Note: WE DO NOT call 'NotifyChangeHandler' for data coming from the OS as the change handler is for the OS..
    return true;
}

void ClipBoard::NotifyChangeHandler(ClipBoardItem::Ref item) {
    if (cbOnUpdate == nullptr) {
        return;
    }
    cbOnUpdate(item);
}


void ClipBoard::PasteToBuffer(TextBuffer::Ref dstBuffer, const Point &ptWhere) {
    // Nothing to paste???
    if (history.empty()) {
        return;
    }

    Top()->PasteToBuffer(dstBuffer, ptWhere);
}

ClipBoard::ClipBoardItem::Ref ClipBoard::Top() {
    if (history.empty()) {
        return nullptr;
    }
    return history.front();
}

void ClipBoard::Dump() {
    for(auto &item : history) {
        item->Dump();
    }
}

ClipBoard::ClipBoardItem::Ref ClipBoard::ClipBoardItem::Create(TextBuffer::Ref srcBuffer, const Point &ptStart, const Point &ptEnd) {
    auto item = std::make_shared<ClipBoardItem>();
    item->start = ptStart;
    item->end = ptEnd;
    item->CopyFromBuffer(srcBuffer);
    return item;
}

ClipBoard::ClipBoardItem::Ref ClipBoard::ClipBoardItem::CreateExternal(const char *srcData) {
    auto item = std::make_shared<ClipBoardItem>();
    item->isExternal = true;
    item->CopyFromExternal(srcData);
    return item;
}


void ClipBoard::ClipBoardItem::CopyFromBuffer(TextBuffer::Ref srcBuffer) {

    // If we have end at the start of a line we copy 'until' that line otherwise we include it...
    size_t yEnd = end.y;
    if (end.x != 0) {
        yEnd++;
    }

    // We simply copy the full lines and when we restore it we read with x-offset handling...
    for(int i=start.y;i<yEnd;i++) {
        auto line = srcBuffer->LineAt(i);
        data.push_back(std::u32string(line->Buffer()));
    }
}

void ClipBoard::ClipBoardItem::CopyFromExternal(const char *srcData) {
    std::stringstream strStream(srcData);
    std::string line;
    while(std::getline(strStream, line)) {
        std::u32string tmp;
        if (UnicodeHelper::ConvertUTF8ToUTF32String(tmp, line)) {
            data.push_back(tmp);
        }
    }
}

void ClipBoard::ClipBoardItem::PasteToBuffer(TextBuffer::Ref dstBuffer, const Point &ptWhere) {
    if (data.empty()) {
        return;
    }

    // Resolve the stored whole-lines + selection columns into the exact text segments to splice in,
    // one per line. Pasting is then a plain text-splice: the target line is split once at the paste
    // column into head|tail, the first segment joins the head, the last segment carries the tail, and
    // any middle segments become their own lines. A trailing empty segment represents a selection that
    // ended on a line break (so the paste leaves a fresh empty line behind).
    std::vector<std::u32string> segs;
    if (isExternal) {
        segs = data;    // external data is already literal, per-line text
    } else if ((data.size() == 1) && (end.x != 0)) {
        // Selection contained within a single line: [start.x, end.x)
        segs.push_back(data[0].substr(start.x, end.x - start.x));
    } else {
        // First line: from start.x to its end
        segs.push_back(data[0].substr(start.x));
        // Whole middle lines
        for (size_t i = 1; (i + 1) < data.size(); i++) {
            segs.push_back(data[i]);
        }
        if (data.size() >= 2) {
            if (end.x != 0) {
                segs.push_back(data.back().substr(0, end.x));   // last line clipped to end.x
            } else {
                segs.push_back(data.back());                    // last line whole...
                segs.push_back(std::u32string());               // ...plus the trailing line break
            }
        } else {
            // Single whole line that ended on a line break (end.x == 0)
            segs.push_back(std::u32string());
        }
    }

    // A destination buffer always keeps at least one line to splice into.
    if (dstBuffer->NumLines() == 0) {
        dstBuffer->AddLine(std::u32string());
    }

    // Clamp the paste line into range and the paste column into the target line.
    size_t idxLine = ptWhere.y;
    if (idxLine >= dstBuffer->NumLines()) {
        idxLine = dstBuffer->NumLines() - 1;
    }
    auto targetLine = dstBuffer->LineAt(idxLine);
    size_t px = ptWhere.x;
    if (px > targetLine->Length()) {
        px = targetLine->Length();
    }

    // Split the target line into head|tail around the paste column.
    const std::u32string original = targetLine->Buffer();
    const std::u32string head = original.substr(0, px);
    const std::u32string tail = original.substr(px);

    if (segs.size() == 1) {
        // No line break in the pasted text - splice it inline: head + seg + tail.
        targetLine->Clear();
        targetLine->Append(head + segs[0] + tail);
        return;
    }

    // Multi-line paste: first segment joins the head, the rest become their own lines, and the last
    // segment carries the original tail.
    targetLine->Clear();
    targetLine->Append(head + segs.front());

    size_t insertAt = idxLine + 1;
    for (size_t i = 1; (i + 1) < segs.size(); i++) {
        dstBuffer->Insert(insertAt, segs[i]);
        insertAt++;
    }
    dstBuffer->Insert(insertAt, segs.back() + tail);
}
void ClipBoard::ClipBoardItem::Dump() {
    printf("PtStart (%d,%d) - PtEnd (%d, %d)\n", start.x, start.y, end.x, end.y);
    int lc = 0;
    for(auto &s : data) {
        auto asciistr = UnicodeHelper::utf32toascii(s);
        printf("  %d: %s\n", lc, asciistr.c_str());
        lc++;
    }
}

// Return the size of data +1 for terminating null char (if needed)
size_t ClipBoard::ClipBoardItem::GetByteSize() {
    size_t nBytes = 0;
    for(auto &l : data) {
        nBytes += l.size();
    }
    // +1 for terminating null...
    nBytes += sizeof('\n') * data.size() + 1;

    return nBytes;
}
