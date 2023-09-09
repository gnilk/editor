//
// Created by gnilk on 19.07.23.
//

#include "ClipBoard.h"
#include <string>
#include <sstream>

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
        data.push_back(std::string(line->Buffer()));
    }
}

void ClipBoard::ClipBoardItem::CopyFromExternal(const char *srcData) {
    std::stringstream strStream(srcData);
    std::string line;
    while(std::getline(strStream, line)) {
        data.push_back(line);
    }
}

void ClipBoard::ClipBoardItem::PasteToBuffer(TextBuffer::Ref dstBuffer, const Point &ptWhere) {

    size_t idxLine = ptWhere.y;

    // Can't paste outside
    if (idxLine > dstBuffer->NumLines()) {
        idxLine = dstBuffer->NumLines();
    }
    size_t idxData = 0;

    if ((start.x != 0) || (ptWhere.x != 0)) {
        int dx = data[0].length() - start.x;
        if (start.y == end.y) {
            dx = end.x - start.x;
        }
        auto substr = data[0].substr(start.x, dx);
        if (dstBuffer->NumLines() == 0) {
            dstBuffer->AddLineUTF8(substr.data());
        } else if (ptWhere.x > dstBuffer->LineAt(idxLine)->Length()) {
            dstBuffer->LineAt(idxLine)->Append(substr);
        } else {
            dstBuffer->LineAt(idxLine)->Insert(ptWhere.x, substr);
        }
        // First line dealt with...
        idxLine++;
        idxData = 1;
    }

    while (idxData < (data.size() - 1)) {
        // Last line??
        dstBuffer->Insert(idxLine, data[idxData]);
        idxData++;
        idxLine++;
    }
    // Last line - in case we have a 'broken' region...
    if (end.x != 0) {
        auto strToAdd = data[idxData].substr(0, end.x);
        // Are we pasting in the middle of the buffer - then we must insert the data to an existing line
        if (dstBuffer->NumLines() > idxLine) {
            auto lastLine = dstBuffer->LineAt(idxLine);
            lastLine->Insert(0, strToAdd);
        } else {
            // We are extending the buffer - just put the data there...
            dstBuffer->Insert(idxLine, strToAdd);
        }
    } else {
        // last line was a 'clean line' - just insert it...
        dstBuffer->Insert(idxLine, data[idxData]);
    }

}
void ClipBoard::ClipBoardItem::Dump() {
    printf("PtStart (%d,%d) - PtEnd (%d, %d)\n", start.x, start.y, end.x, end.y);
    int lc = 0;
    for(auto &s : data) {
        printf("  %d: %s\n", lc, s.c_str());
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
