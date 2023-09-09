//
// Created by gnilk on 19.07.23.
//
#include <testinterface.h>
#include "Core/ClipBoard.h"
#include "Core/TextBuffer.h"


using namespace gedit;

extern "C" {
DLL_EXPORT int test_clipboard(ITesting *t);
DLL_EXPORT int test_clipboard_copylines(ITesting *t);
DLL_EXPORT int test_clipboard_copylineregion(ITesting *t);
DLL_EXPORT int test_clipboard_copyclippedstart(ITesting *t);
DLL_EXPORT int test_clipboard_paste(ITesting *t);
DLL_EXPORT int test_clipboard_pastelineregion(ITesting *t);
DLL_EXPORT int test_clipboard_pasteregionover(ITesting *t);
DLL_EXPORT int test_clipboard_copypasteexternal(ITesting *t);
}

DLL_EXPORT int test_clipboard(ITesting *t) {
    return kTR_Pass;
}
DLL_EXPORT int test_clipboard_copylines(ITesting *t) {
    ClipBoard clipBoard;
    // Setup a text buffer
    auto textBuffer = TextBuffer::CreateEmptyBuffer();
    for(int i=0;i<10;i++) {
        char tmp[32];
        snprintf(tmp,32,"line %d", i);
        textBuffer->AddLineUTF8(tmp);
    }
    TR_ASSERT(t, clipBoard.Top() == nullptr);
    TR_ASSERT(t, clipBoard.CopyFromBuffer(textBuffer, {0,2}, {0,5}));
    TR_ASSERT(t, clipBoard.Top() != nullptr);
    TR_ASSERT(t, clipBoard.Top()->GetLineCount() == 3);
    clipBoard.Dump();
    return kTR_Pass;
}
DLL_EXPORT int test_clipboard_copylineregion(ITesting *t) {
    ClipBoard clipBoard;
    // Setup a text buffer
    auto textBuffer = TextBuffer::CreateEmptyBuffer();
    for(int i=0;i<10;i++) {
        char tmp[32];
        snprintf(tmp,32,"line %d", i);
        textBuffer->AddLineUTF8(tmp);
    }
    TR_ASSERT(t, clipBoard.Top() == nullptr);
    TR_ASSERT(t, clipBoard.CopyFromBuffer(textBuffer, {2,2}, {4,2}));
    TR_ASSERT(t, clipBoard.Top() != nullptr);
    auto item = clipBoard.Top();
    TR_ASSERT(t, item->GetLineCount() == 1);
    clipBoard.Dump();
    return kTR_Pass;
}
DLL_EXPORT int test_clipboard_copyclippedstart(ITesting *t) {
    ClipBoard clipBoard;
    // Setup a text buffer
    auto textBuffer = TextBuffer::CreateEmptyBuffer();
    for(int i=0;i<10;i++) {
        char tmp[32];
        snprintf(tmp,32,"line %d", i);
        textBuffer->AddLineUTF8(tmp);
    }
    TR_ASSERT(t, clipBoard.Top() == nullptr);
    TR_ASSERT(t, clipBoard.CopyFromBuffer(textBuffer, {3,2}, {0,5}));
    TR_ASSERT(t, clipBoard.Top() != nullptr);
    TR_ASSERT(t, clipBoard.Top()->GetLineCount() == 3);

    clipBoard.Dump();
    return kTR_Pass;
}

DLL_EXPORT int test_clipboard_paste(ITesting *t) {
    ClipBoard clipBoard;

    auto srcBuffer = TextBuffer::CreateEmptyBuffer();
    auto dstBuffer = TextBuffer::CreateEmptyBuffer();
    for(int i=0;i<10;i++) {
        char tmp[32];
        snprintf(tmp,32,"line %d", i);
        srcBuffer->AddLineUTF8(tmp);
    }
    TR_ASSERT(t, clipBoard.Top() == nullptr);
    TR_ASSERT(t, clipBoard.CopyFromBuffer(srcBuffer, {0,2}, {0,5}));
    TR_ASSERT(t, clipBoard.Top() != nullptr);
    TR_ASSERT(t, clipBoard.Top()->GetLineCount() == 3);

    TR_ASSERT(t, dstBuffer->NumLines() == 0);

    clipBoard.PasteToBuffer(dstBuffer, {0,0});
    auto lines = dstBuffer->Lines();
    int lc = 0;
    for(auto &l : lines) {
        printf("%d : %s\n",lc, l->Buffer().data());
        lc++;
    }

    TR_ASSERT(t, dstBuffer->NumLines() == 3);
    TR_ASSERT(t, lines[0]->Buffer() == "line 2");
    return kTR_Pass;
}

DLL_EXPORT int test_clipboard_pastelineregion(ITesting *t) {
    ClipBoard clipBoard;

    auto srcBuffer = TextBuffer::CreateEmptyBuffer();
    auto dstBuffer = TextBuffer::CreateEmptyBuffer();
    for(int i=0;i<10;i++) {
        char tmp[32];
        snprintf(tmp,32,"line %d", i);
        srcBuffer->AddLineUTF8(tmp);
    }
    TR_ASSERT(t, clipBoard.Top() == nullptr);
    TR_ASSERT(t, clipBoard.CopyFromBuffer(srcBuffer, {2,2}, {4,4}));
    TR_ASSERT(t, clipBoard.Top() != nullptr);
//    TR_ASSERT(t, clipBoard.Top()->GetLineCount() == 1);

    TR_ASSERT(t, dstBuffer->NumLines() == 0);

    clipBoard.PasteToBuffer(dstBuffer, {0,0});
    auto lines = dstBuffer->Lines();
    int lc = 0;
    for(auto &l : lines) {
        printf("%d : '%s'\n",lc, l->Buffer().data());
        lc++;
    }

    TR_ASSERT(t, dstBuffer->NumLines() == 3);
    TR_ASSERT(t, lines[0]->Buffer() == "ne 2");
    return kTR_Pass;
}

DLL_EXPORT int test_clipboard_pasteregionover(ITesting *t) {
    ClipBoard clipBoard;

    auto srcBuffer = TextBuffer::CreateEmptyBuffer();
    auto dstBuffer = TextBuffer::CreateEmptyBuffer();
    for(int i=0;i<10;i++) {
        char tmp[32];
        snprintf(tmp,32,"line %d", i);
        srcBuffer->AddLineUTF8(tmp);
        snprintf(tmp,32,"MAMAMAMAMAMAMAMAMAM %d", i);
        dstBuffer->AddLineUTF8(tmp);
    }

    TR_ASSERT(t, clipBoard.Top() == nullptr);
    // Note: If the region is changed you MUST change the assert below!!!
    TR_ASSERT(t, clipBoard.CopyFromBuffer(srcBuffer, {2,2}, {4,4}));
    TR_ASSERT(t, clipBoard.Top() != nullptr);

    clipBoard.PasteToBuffer(dstBuffer, {2,4});
    auto lines = dstBuffer->Lines();
    int lc = 0;
    for(auto &l : lines) {
        printf("%d : '%s'\n",lc, l->Buffer().data());
        lc++;
    }

    TR_ASSERT(t, dstBuffer->NumLines() == 11);
    TR_ASSERT(t, lines[4]->Buffer() == "MAne 2MAMAMAMAMAMAMAMAM 4");
    return kTR_Pass;
}
DLL_EXPORT int test_clipboard_copypasteexternal(ITesting *t) {
    ClipBoard clipBoard;

    auto dstBuffer = TextBuffer::CreateEmptyBuffer();
    auto strData = "hello\nthis is\na multiline\ncopy";
    clipBoard.CopyFromExternal(strData);
    clipBoard.PasteToBuffer(dstBuffer, {0,0});
    TR_ASSERT(t, dstBuffer->NumLines() != 0);
    TR_ASSERT(t, dstBuffer->NumLines() == 4);
    return kTR_Pass;
}