//
// Created by gnilk on 18.07.23.
//
#include <testinterface.h>
#include "Core/Editor.h"
#include "Core/EditorModel.h"
#include "Core/Language/LanguageBase.h"
#include "Core/Language/LangLineTokenizer.h"
#include "Core/TextBuffer.h"


using namespace gedit;

extern "C" {
DLL_EXPORT int test_textbuffer(ITesting *t);
DLL_EXPORT int test_textbuffer_parsefull(ITesting *t);
DLL_EXPORT int test_textbuffer_parseregion(ITesting *t);
DLL_EXPORT int test_textbuffer_thparsefull(ITesting *t);
DLL_EXPORT int test_textbuffer_thparseregion(ITesting *t);
DLL_EXPORT int test_textbuffer_parselarge(ITesting *t);
DLL_EXPORT int test_textbuffer_flatten(ITesting *t);
DLL_EXPORT int test_textbuffer_insertlast(ITesting *t);
}

static void PostCaseCallback(ITesting *t) {
}

DLL_EXPORT int test_textbuffer(ITesting *t) {
    t->SetPostCaseCallback(PostCaseCallback);
    return kTR_Pass;
}
DLL_EXPORT int test_textbuffer_parsefull(ITesting *t) {
    // Disable threading
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    Workspace workspace;
    auto workspaceNode = workspace.NewModelWithFileRef("test_src2.cpp");
    auto buffer = workspaceNode->GetTextBuffer();
    TR_ASSERT(t, workspaceNode->LoadData());
    size_t totalBefore = buffer->GetParseMetrics().total;
    buffer->Reparse();
    TR_ASSERT(t, buffer->GetParseMetrics().total > totalBefore);

    return kTR_Pass;
}
DLL_EXPORT int test_textbuffer_parseregion(ITesting *t) {
    // Disable threading
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    Workspace workspace;
    auto workspaceNode = workspace.NewModelWithFileRef("test_src2.cpp");
    auto buffer = workspaceNode->GetTextBuffer();
    TR_ASSERT(t, workspaceNode->LoadData());
    size_t totalBefore = buffer->GetParseMetrics().total;
    // Just pick a region here...
    buffer->ReparseRegion(10, 20);
    TR_ASSERT(t, buffer->GetParseMetrics().total > totalBefore);

    return kTR_Pass;
}

DLL_EXPORT int test_textbuffer_thparsefull(ITesting *t) {
    // Enable threading
    Config::Instance()["main"].SetBool("threaded_syntaxparser", true);

    Workspace workspace;
    auto workspaceNode = workspace.NewModelWithFileRef("test_src2.cpp");
    auto buffer = workspaceNode->GetTextBuffer();
    TR_ASSERT(t, workspaceNode->LoadData());
    TR_ASSERT(t, !buffer->IsEmpty());   // Did we load?
    buffer->Reparse();
    // We are parsing once while loading - so let's make sure we are idle before doing anything else
    while (buffer->GetParseState() != TextBuffer::ParseState::kState_Idle) {
        std::this_thread::yield();
    }

    size_t totalBefore = buffer->GetParseMetrics().total;
    size_t fullBefore = buffer->GetParseMetrics().full;
    buffer->Reparse();
    // Wait until we have settled down
    while (buffer->GetParseState() != TextBuffer::ParseState::kState_Idle) {
        std::this_thread::yield();
    }
    TR_ASSERT(t, buffer->GetParseMetrics().total > totalBefore);
    TR_ASSERT(t, buffer->GetParseMetrics().full > fullBefore);

    return kTR_Pass;
}

DLL_EXPORT int test_textbuffer_thparseregion(ITesting *t) {
    // Enable threading
    Config::Instance()["main"].SetBool("threaded_syntaxparser", true);

    Workspace workspace;
    auto workspaceNode = workspace.NewModelWithFileRef("test_src2.cpp");
    auto buffer = workspaceNode->GetTextBuffer();
    TR_ASSERT(t, workspaceNode->LoadData());
    TR_ASSERT(t, !buffer->IsEmpty());   // Did we load?

    // We are parsing once while loading - so let's make sure we are idle before doing anything else
    while (buffer->GetParseState() != TextBuffer::ParseState::kState_Idle) {
        std::this_thread::yield();
    }

    size_t totalBefore = buffer->GetParseMetrics().total;
    size_t regionBefore = buffer->GetParseMetrics().region;
    // Just pick a region here...
    buffer->ReparseRegion(10, 20);
    // Wait until we have settled down
    while (buffer->GetParseState() != TextBuffer::ParseState::kState_Idle) {
        std::this_thread::yield();
    }

    TR_ASSERT(t, buffer->GetParseMetrics().total > totalBefore);
    TR_ASSERT(t, buffer->GetParseMetrics().region > regionBefore);

    return kTR_Pass;
}

// This takes around 6 seconds on my MacBook Pro M1
// sqlite3.c is 8.4MB with 238'189 lines of C code
DLL_EXPORT int test_textbuffer_parselarge(ITesting *t) {
    // Disable threading...
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    Workspace workspace;
    auto workspaceNode = workspace.NewModelWithFileRef("sqlite3.c");
    auto buffer = workspaceNode->GetTextBuffer();
    TR_ASSERT(t, workspaceNode->LoadData());
    size_t totalBefore = buffer->GetParseMetrics().total;
    buffer->Reparse();
    TR_ASSERT(t, buffer->GetParseMetrics().total > totalBefore);

    return kTR_Pass;

}
DLL_EXPORT int test_textbuffer_flatten(ITesting *t) {
    auto buffer = TextBuffer::CreateEmptyBuffer();
    for(int i=0;i<10;i++) {
        char tmp[32];
        snprintf(tmp,32,"line %d",i);
        buffer->AddLineUTF8(tmp);
    }


//    char flattenBuffer[512];
//    memset(flattenBuffer, 0, 512);
    std::u32string flattenBuffer;
    // Below cases should stress all exit points of the function...
    TR_ASSERT(t, 0 == buffer->Flatten(flattenBuffer, 100, 10));
    TR_ASSERT(t, 0 == buffer->Flatten(flattenBuffer, buffer->NumLines(), 10));
    TR_ASSERT(t, 5 == buffer->Flatten(flattenBuffer, 5, 10));
    TR_ASSERT(t, 5 == buffer->Flatten(flattenBuffer, 5, 20));
    TR_ASSERT(t, 10 == buffer->Flatten(flattenBuffer, 0, 10));
    TR_ASSERT(t, 10 == buffer->Flatten(flattenBuffer, 0, 0));


    return kTR_Pass;
}

DLL_EXPORT int test_textbuffer_insertlast(ITesting *t) {
    size_t idxActiveLine = 1;
    auto textBuffer = TextBuffer::CreateEmptyBuffer();
    auto &lines = textBuffer->Lines();

    auto it = lines.begin() + idxActiveLine;
    auto newLine = Line::Create(U"dynn");

    if (it == lines.end()) {
        textBuffer->AddLine(newLine);
    } else {
        // This should throw an exception...
        textBuffer->Insert(it + 1, newLine);
    }



    return kTR_Pass;
}
