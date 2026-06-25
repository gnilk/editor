//
// Created by gnilk on 18.07.23.
//
#include <testinterface.h>
#include <filesystem>
#include <fstream>
#include <cstdint>
#include "Core/Editor.h"
#include "Core/Document.h"
#include "Core/Language/LanguageBase.h"
#include "Core/Language/LangLineTokenizer.h"
#include "Core/TextBuffer.h"
#include "Core/Line.h"
#include "Core/ColorRGBA.h"


using namespace gedit;

extern "C" {
DLL_EXPORT int test_textbuffer(ITesting *t);
DLL_EXPORT int test_textbuffer_parsefull(ITesting *t);
DLL_EXPORT int test_textbuffer_parseregion(ITesting *t);
DLL_EXPORT int test_textbuffer_thparsefull(ITesting *t);
DLL_EXPORT int test_textbuffer_thparseregion(ITesting *t);
#ifdef NDEBUG
DLL_EXPORT int test_textbuffer_parselarge(ITesting *t);
#endif
DLL_EXPORT int test_textbuffer_flatten(ITesting *t);
DLL_EXPORT int test_textbuffer_insertlast(ITesting *t);
DLL_EXPORT int test_textbuffer_saveattribs_roundtrip(ITesting *t);
DLL_EXPORT int test_textbuffer_saveattribs_badmagic(ITesting *t);
DLL_EXPORT int test_textbuffer_saveattribs_newerversion(ITesting *t);
DLL_EXPORT int test_textbuffer_saveattribs_missing(ITesting *t);
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
    auto workspaceNode = workspace.NewDocumentWithFileRef("testfiles/ConvertUTF.cpp");
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
    auto workspaceNode = workspace.NewDocumentWithFileRef("testfiles/ConvertUTF.cpp");
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
    auto workspaceNode = workspace.NewDocumentWithFileRef("testfiles/ConvertUTF.cpp");
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
    auto workspaceNode = workspace.NewDocumentWithFileRef("testfiles/ConvertUTF.cpp");
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
    // ReparseRegion is asynchronous - it returns the job. Polling GetParseState() is racy (the
    // worker may still report kState_Idle before it dequeues), so wait on the job's completion.
    auto job = buffer->ReparseRegion(10, 20);
    TR_ASSERT(t, job != nullptr);
    job->WaitComplete();

    TR_ASSERT(t, buffer->GetParseMetrics().total > totalBefore);
    TR_ASSERT(t, buffer->GetParseMetrics().region > regionBefore);

    return kTR_Pass;
}

// Release-only: parses the full sqlite3 amalgamation (~6s on an M1, 8.4MB / 238'189 lines).
// Far too slow for the debug dev cycle, and depends on an untracked sqlite3.c in the cwd, so it
// is compiled only in release builds (NDEBUG). Run with a release build of utests to exercise it.
#ifdef NDEBUG
DLL_EXPORT int test_textbuffer_parselarge(ITesting *t) {
    // Disable threading...
    Config::Instance()["main"].SetBool("threaded_syntaxparser", false);

    Workspace workspace;
    auto workspaceNode = workspace.NewDocumentWithFileRef("sqlite3.c");
    auto buffer = workspaceNode->GetTextBuffer();
    TR_ASSERT(t, workspaceNode->LoadData());
    size_t totalBefore = buffer->GetParseMetrics().total;
    buffer->Reparse();
    TR_ASSERT(t, buffer->GetParseMetrics().total > totalBefore);

    return kTR_Pass;
}
#endif
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
    // CreateEmptyBuffer() seeds an empty line, so NumLines() here is 11 (1 + 10 added). Express the
    // expectations against NumLines() rather than baking in that off-by-one.
    const size_t n = buffer->NumLines();
    // Below cases should stress all exit points of the function...
    TR_ASSERT(t, 0 == buffer->Flatten(flattenBuffer, n + 1, 10));    // start past end -> nothing
    TR_ASSERT(t, 0 == buffer->Flatten(flattenBuffer, n, 10));        // start == NumLines -> nothing
    TR_ASSERT(t, (n - 5) == buffer->Flatten(flattenBuffer, 5, 10));  // nLines exceeds remaining -> clamp
    TR_ASSERT(t, (n - 5) == buffer->Flatten(flattenBuffer, 5, 20));  // nLines exceeds remaining -> clamp
    TR_ASSERT(t, 10 == buffer->Flatten(flattenBuffer, 0, 10));       // nLines < remaining -> exactly nLines
    TR_ASSERT(t, n == buffer->Flatten(flattenBuffer, 0, 0));         // nLines == 0 -> all


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

// --- TS-5a: SaveWithAttributes/LoadWithAttributes round-trip (terminal-scrollback §8.1) ---------------

// Build a line with one explicit colored attrib span (mirrors what TerminalScreen::RowToLine produces
// when a colored row scrolls into scrollback).
static Line::Ref MakeColoredLine(const std::u32string &text, int idxStart,
                                 const ColorRGBA &fg, const ColorRGBA &bg, kTextAttributes attrs) {
    auto line = Line::Create(text);
    line->Attributes().clear();
    Line::LineAttrib a;
    a.idxOrigString   = idxStart;
    a.foregroundColor = fg;
    a.backgroundColor = bg;
    a.textAttributes  = attrs;
    line->Attributes().push_back(a);
    return line;
}

static std::filesystem::path ScratchBinPath(const char *name) {
    return std::filesystem::temp_directory_path() / name;
}

// Save a 3-line colored buffer, load it into a fresh one, assert text AND every attribute span survive.
DLL_EXPORT int test_textbuffer_saveattribs_roundtrip(ITesting *t) {
    auto path = ScratchBinPath("goatedit_ts5a_roundtrip.bin");
    std::filesystem::remove(path);

    auto fgRed   = ColorRGBA::FromRGB(220, 40, 30);
    auto bgDark  = ColorRGBA::FromRGB(10, 12, 16);
    auto fgGreen = ColorRGBA::FromRGB(40, 200, 60);

    auto src = std::make_shared<TextBuffer>();
    src->AddLine(MakeColoredLine(U"hello world", 0, fgRed, bgDark, kTextAttributes::kBold));
    // A line carrying TWO spans, to exercise multi-span lines.
    {
        auto line = Line::Create(U"two tone");
        line->Attributes().clear();
        Line::LineAttrib a0; a0.idxOrigString = 0; a0.foregroundColor = fgRed;   a0.backgroundColor = bgDark; a0.textAttributes = kTextAttributes::kNormal;
        Line::LineAttrib a1; a1.idxOrigString = 4; a1.foregroundColor = fgGreen; a1.backgroundColor = bgDark; a1.textAttributes = kTextAttributes::kUnderline;
        line->Attributes().push_back(a0);
        line->Attributes().push_back(a1);
        src->AddLine(line);
    }
    src->AddLine(MakeColoredLine(U"third", 0, fgGreen, bgDark, kTextAttributes::kNormal));

    TR_ASSERT(t, src->SaveWithAttributes(path));

    auto dst = std::make_shared<TextBuffer>();
    TR_ASSERT(t, dst->LoadWithAttributes(path));
    TR_ASSERT(t, dst->NumLines() == src->NumLines());

    for (size_t i = 0; i < src->NumLines(); i++) {
        auto a = src->LineAt(i);
        auto b = dst->LineAt(i);
        TR_ASSERT(t, b != nullptr);
        // Text is lossless.
        TR_ASSERT(t, b->Buffer() == a->Buffer());
        // Attribute spans are lossless (count + every field, colors compared as 8-bit ints via ==).
        auto &as = a->Attributes();
        auto &bs = b->Attributes();
        TR_ASSERT(t, bs.size() == as.size());
        for (size_t s = 0; s < as.size(); s++) {
            TR_ASSERT(t, bs[s].idxOrigString == as[s].idxOrigString);
            TR_ASSERT(t, bs[s].textAttributes == as[s].textAttributes);
            TR_ASSERT(t, bs[s].foregroundColor == as[s].foregroundColor);
            TR_ASSERT(t, bs[s].backgroundColor == as[s].backgroundColor);
            TR_ASSERT(t, bs[s].tokenClass == as[s].tokenClass);
        }
    }

    std::filesystem::remove(path);
    return kTR_Pass;
}

// A file that is not ours (wrong magic) is rejected without crashing; the target buffer is untouched.
DLL_EXPORT int test_textbuffer_saveattribs_badmagic(ITesting *t) {
    auto path = ScratchBinPath("goatedit_ts5a_badmagic.bin");
    {
        std::ofstream os(path, std::ios::binary | std::ios::trunc);
        const char junk[] = {'X', 'X', 'X', 'X', 0, 1, 2, 3};
        os.write(junk, sizeof(junk));
    }
    auto dst = std::make_shared<TextBuffer>();
    TR_ASSERT(t, dst->LoadWithAttributes(path) == false);
    TR_ASSERT(t, dst->NumLines() == 0);     // nothing loaded, no crash
    std::filesystem::remove(path);
    return kTR_Pass;
}

// A valid magic but an unknown/newer version is rejected (start clean, never mis-parse).
DLL_EXPORT int test_textbuffer_saveattribs_newerversion(ITesting *t) {
    auto path = ScratchBinPath("goatedit_ts5a_newerversion.bin");
    {
        std::ofstream os(path, std::ios::binary | std::ios::trunc);
        const char magic[4] = {'G', 'T', 'S', 'B'};
        uint32_t version   = 0xFFFF;    // far newer than anything we write
        uint32_t flags     = 0;
        uint32_t lineCount = 0;
        os.write(magic, sizeof(magic));
        os.write(reinterpret_cast<const char *>(&version), sizeof(version));
        os.write(reinterpret_cast<const char *>(&flags), sizeof(flags));
        os.write(reinterpret_cast<const char *>(&lineCount), sizeof(lineCount));
    }
    auto dst = std::make_shared<TextBuffer>();
    TR_ASSERT(t, dst->LoadWithAttributes(path) == false);
    std::filesystem::remove(path);
    return kTR_Pass;
}

// A missing file is a clean false (the restore path starts with empty scrollback).
DLL_EXPORT int test_textbuffer_saveattribs_missing(ITesting *t) {
    auto path = ScratchBinPath("goatedit_ts5a_does_not_exist.bin");
    std::filesystem::remove(path);
    auto dst = std::make_shared<TextBuffer>();
    TR_ASSERT(t, dst->LoadWithAttributes(path) == false);
    return kTR_Pass;
}
