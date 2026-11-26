//
// Line endings (docs/windows-port.md §4.2): each line keeps the ending it was read with, a line an edit made takes the
// file's, a file without a last newline stays without one - a save writes the file back byte for byte. Before, every
// CRLF file was converted to LF, silently, on its first save, and every file got a last newline.
//
#include <testinterface.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "Core/ClipBoard.h"
#include "Core/Config/Config.h"
#include "Core/LineEnding.h"
#include "Core/TextBuffer.h"
#include "Core/Platform.h"

using namespace gedit;
namespace fs = std::filesystem;

extern "C" {
    DLL_EXPORT int test_lineendings(ITesting *t);
    DLL_EXPORT int test_lineendings_names(ITesting *t);
    DLL_EXPORT int test_lineendings_counts(ITesting *t);
    DLL_EXPORT int test_lineendings_lfroundtrip(ITesting *t);
    DLL_EXPORT int test_lineendings_crlfroundtrip(ITesting *t);
    DLL_EXPORT int test_lineendings_mixedkept(ITesting *t);
    DLL_EXPORT int test_lineendings_nofinalnewline(ITesting *t);
    DLL_EXPORT int test_lineendings_emptyfile(ITesting *t);
    DLL_EXPORT int test_lineendings_newline(ITesting *t);
    DLL_EXPORT int test_lineendings_newfile(ITesting *t);
    DLL_EXPORT int test_lineendings_convert(ITesting *t);
    DLL_EXPORT int test_lineendings_mixedconvert(ITesting *t);
    DLL_EXPORT int test_lineendings_paste(ITesting *t);
    DLL_EXPORT int test_lineendings_readlines(ITesting *t);
}

namespace {
    fs::path MakeFile(const std::string &name, const std::string &bytes) {
        auto root = fs::canonical(fs::temp_directory_path()) / "goatedit_lineendings";
        fs::create_directories(root);
        auto path = root / name;
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file << bytes;
        return path;
    }

    std::string ReadBytes(const fs::path &path) {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    }

    void Defaults() {
        Config::Instance()["main"].SetStr("line_endings", "unix");
        Config::Instance()["main"].SetStr("mixed_line_endings", "ask");
        Config::Instance()["main"].SetStr("convert_encoding", "keep");
    }

    // Load, save without a change, compare
    bool RoundTrips(const std::string &name, const std::string &bytes) {
        auto path = MakeFile(name, bytes);
        auto buffer = TextBuffer::CreateBufferFromFile(path);
        if ((buffer == nullptr) || !buffer->SaveForce(path)) {
            return false;
        }
        return ReadBytes(path) == bytes;
    }
}

DLL_EXPORT int test_lineendings(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_lineendings_names(ITesting *t) {
    TR_ASSERT(t, LineEndings::FromName("unix", "macos") == kLineEnding::kLF);
    TR_ASSERT(t, LineEndings::FromName("windows", "linux") == kLineEnding::kCRLF);
    TR_ASSERT(t, LineEndings::FromName("lf", "windows") == kLineEnding::kLF);
    TR_ASSERT(t, LineEndings::FromName("crlf", "linux") == kLineEnding::kCRLF);
    TR_ASSERT(t, LineEndings::FromName("platform", "windows") == kLineEnding::kCRLF);
    TR_ASSERT(t, LineEndings::FromName("platform", "linux") == kLineEnding::kLF);
    TR_ASSERT(t, LineEndings::FromName("platform", "macos") == kLineEnding::kLF);
    TR_ASSERT(t, !LineEndings::FromName("dos", "linux").has_value());
    TR_ASSERT(t, !LineEndings::FromName("", "linux").has_value());
    // What the user sees: never LF / CRLF
    TR_ASSERT(t, std::string(LineEndings::Name(kLineEnding::kLF)) == "unix");
    TR_ASSERT(t, std::string(LineEndings::Name(kLineEnding::kCRLF)) == "windows");

    TR_ASSERT(t, LineEndings::MixedPolicyFromName("ask") == LineEndings::kMixedPolicy::kAsk);
    TR_ASSERT(t, LineEndings::MixedPolicyFromName("keep") == LineEndings::kMixedPolicy::kKeep);
    TR_ASSERT(t, LineEndings::MixedPolicyFromName("convert") == LineEndings::kMixedPolicy::kConvert);
    TR_ASSERT(t, !LineEndings::MixedPolicyFromName("lf").has_value());
    // An unknown value is 'ask' - never a silent convert
    Config::Instance()["main"].SetStr("mixed_line_endings", "bogus");
    TR_ASSERT(t, LineEndings::MixedPolicy() == LineEndings::kMixedPolicy::kAsk);
    Config::Instance()["main"].SetStr("line_endings", "bogus");
    TR_ASSERT(t, LineEndings::Preferred() == LineEndings::FromName("platform", Platform::Name()));
    Defaults();
    return kTR_Pass;
}

DLL_EXPORT int test_lineendings_counts(ITesting *t) {
    LineEndingCounts counts;
    TR_ASSERT(t, !counts.IsMixed());
    // Nothing says: the preferred
    TR_ASSERT(t, counts.Dominant(kLineEnding::kCRLF) == kLineEnding::kCRLF);
    counts.Add(kLineEnding::kCRLF);
    counts.Add(kLineEnding::kDefault);      // a line an edit made counts for nothing
    TR_ASSERT(t, counts.numCRLF == 1);
    TR_ASSERT(t, counts.numLF == 0);
    TR_ASSERT(t, counts.Dominant(kLineEnding::kLF) == kLineEnding::kCRLF);
    counts.Add(kLineEnding::kLF);
    TR_ASSERT(t, counts.IsMixed());
    // A tie: the preferred
    TR_ASSERT(t, counts.Dominant(kLineEnding::kLF) == kLineEnding::kLF);
    TR_ASSERT(t, counts.Dominant(kLineEnding::kCRLF) == kLineEnding::kCRLF);
    counts.Add(kLineEnding::kLF);
    TR_ASSERT(t, counts.Dominant(kLineEnding::kCRLF) == kLineEnding::kLF);
    return kTR_Pass;
}

DLL_EXPORT int test_lineendings_lfroundtrip(ITesting *t) {
    Defaults();
    auto path = MakeFile("lf.c", "int a;\nint b;\n");
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->NumLines() == 2);
    TR_ASSERT(t, buffer->LineAt(0)->GetLineEnding() == kLineEnding::kLF);
    TR_ASSERT(t, buffer->GetLineEndingCounts().numLF == 2);
    TR_ASSERT(t, buffer->HasFinalNewline());
    TR_ASSERT(t, RoundTrips("lf.c", "int a;\nint b;\n"));
    return kTR_Pass;
}

// The bug: the '\r' was dropped on load and '\n' written on save - every line of a CRLF file changed
DLL_EXPORT int test_lineendings_crlfroundtrip(ITesting *t) {
    Defaults();
    std::string crlfFile = "int a;\r\n\r\nint b;   \r\n";
    auto path = MakeFile("crlf.c", crlfFile);
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->NumLines() == 3);
    // The '\r' is the line break's, not text
    TR_ASSERT(t, buffer->LineAt(0)->Buffer() == U"int a;");
    TR_ASSERT(t, buffer->LineAt(1)->Buffer().empty());
    TR_ASSERT(t, buffer->LineAt(1)->GetLineEnding() == kLineEnding::kCRLF);
    TR_ASSERT(t, buffer->GetLineEndingCounts().numCRLF == 3);
    TR_ASSERT(t, !buffer->GetLineEndingCounts().IsMixed());
    TR_ASSERT(t, buffer->SaveForce(path));
    // Trailing white space still goes on load (not part of this) - the endings stay
    TR_ASSERT(t, ReadBytes(path) == "int a;\r\n\r\nint b;\r\n");
    TR_ASSERT(t, RoundTrips("crlf2.c", "a\r\nb\r\n"));
    // ... in UTF-16 too
    std::string utf16 = std::string("\xff\xfe", 2) + std::string("a\0\r\0\n\0b\0\r\0\n\0", 12);
    TR_ASSERT(t, RoundTrips("crlf16.txt", utf16));
    return kTR_Pass;
}

// 'ask' at a save that isn't the save command's (autosave, close's 'Save') keeps each line as it is
DLL_EXPORT int test_lineendings_mixedkept(ITesting *t) {
    Defaults();
    std::string mixedFile = "unix\nwindows\r\nunix again\n";
    auto path = MakeFile("mixed.txt", mixedFile);
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->GetLineEndingCounts().IsMixed());
    TR_ASSERT(t, buffer->LineAt(1)->GetLineEnding() == kLineEnding::kCRLF);
    TR_ASSERT(t, buffer->SaveForce(path));
    TR_ASSERT(t, ReadBytes(path) == mixedFile);
    Config::Instance()["main"].SetStr("mixed_line_endings", "keep");
    TR_ASSERT(t, RoundTrips("mixed2.txt", mixedFile));
    Defaults();
    return kTR_Pass;
}

// The second silent change: every save ended the file with a newline
DLL_EXPORT int test_lineendings_nofinalnewline(ITesting *t) {
    Defaults();
    auto path = MakeFile("nofinal.txt", "first\nlast");
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, !buffer->HasFinalNewline());
    TR_ASSERT(t, buffer->NumLines() == 2);
    TR_ASSERT(t, buffer->LineAt(1)->GetLineEnding() == kLineEnding::kDefault);
    TR_ASSERT(t, RoundTrips("nofinal.txt", "first\nlast"));
    TR_ASSERT(t, RoundTrips("nofinalcrlf.txt", "first\r\nlast"));
    // A line added at the end: still no newline after the last one, the one before it gets the file's
    buffer = TextBuffer::CreateBufferFromFile(MakeFile("nofinalcrlf.txt", "first\r\nlast"));
    buffer->Insert(2, Line::Create(U"added"));
    auto out = MakeFile("nofinalcrlf.out", "");
    TR_ASSERT(t, buffer->SaveForce(out));
    TR_ASSERT(t, ReadBytes(out) == "first\r\nlast\r\nadded");
    return kTR_Pass;
}

DLL_EXPORT int test_lineendings_emptyfile(ITesting *t) {
    Defaults();
    auto path = MakeFile("empty.txt", "");
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->NumLines() == 1);
    TR_ASSERT(t, !buffer->HasFinalNewline());
    TR_ASSERT(t, RoundTrips("empty.txt", ""));
    // A single newline is one empty line, ended
    TR_ASSERT(t, RoundTrips("newline.txt", "\n"));
    TR_ASSERT(t, RoundTrips("crlfonly.txt", "\r\n"));
    return kTR_Pass;
}

// A line an edit made takes the file's style - a CRLF file stays CRLF without anyone choosing
DLL_EXPORT int test_lineendings_newline(ITesting *t) {
    Defaults();
    auto path = MakeFile("edited.c", "a\r\nb\r\n");
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer != nullptr);
    buffer->Insert(1, Line::Create(U"new"));
    TR_ASSERT(t, buffer->LineAt(1)->GetLineEnding() == kLineEnding::kDefault);
    TR_ASSERT(t, buffer->GetDominantLineEnding() == kLineEnding::kCRLF);
    TR_ASSERT(t, buffer->SaveForce(path));
    TR_ASSERT(t, ReadBytes(path) == "a\r\nnew\r\nb\r\n");
    // After the save the line says what the file has
    TR_ASSERT(t, buffer->LineAt(1)->GetLineEnding() == kLineEnding::kCRLF);
    TR_ASSERT(t, buffer->GetLineEndingCounts().numCRLF == 3);
    // In a mixed file the most lines' ending
    path = MakeFile("edited2.c", "a\nb\nc\r\n");
    buffer = TextBuffer::CreateBufferFromFile(path);
    buffer->Insert(0, Line::Create(U"new"));
    Config::Instance()["main"].SetStr("mixed_line_endings", "keep");
    TR_ASSERT(t, buffer->SaveForce(path));
    TR_ASSERT(t, ReadBytes(path) == "new\na\nb\nc\r\n");
    Defaults();
    return kTR_Pass;
}

// A buffer that never was a file: main.line_endings, and a last newline
DLL_EXPORT int test_lineendings_newfile(ITesting *t) {
    Defaults();
    Config::Instance()["main"].SetStr("line_endings", "windows");
    auto buffer = TextBuffer::CreateEmptyBuffer();
    buffer->AddLine(U"one");
    buffer->AddLine(U"two");
    TR_ASSERT(t, buffer->HasFinalNewline());
    TR_ASSERT(t, buffer->GetDominantLineEnding() == kLineEnding::kCRLF);
    auto path = MakeFile("newfile.txt", "");
    TR_ASSERT(t, buffer->SaveForce(path));
    TR_ASSERT(t, ReadBytes(path).ends_with("one\r\ntwo\r\n"));
    Defaults();
    return kTR_Pass;
}

// '.line_endings windows': every line, a change to save
DLL_EXPORT int test_lineendings_convert(ITesting *t) {
    Defaults();
    auto path = MakeFile("convert.txt", "a\nb\r\nc\n");
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->GetBufferState() == TextBuffer::kBuffer_Loaded);
    buffer->ConvertLineEndings(kLineEnding::kCRLF);
    TR_ASSERT(t, buffer->GetBufferState() == TextBuffer::kBuffer_Changed);
    TR_ASSERT(t, !buffer->GetLineEndingCounts().IsMixed());
    TR_ASSERT(t, buffer->GetLineEndingCounts().numCRLF == 3);
    // Save (not forced): it is a change
    TR_ASSERT(t, buffer->Save(path));
    TR_ASSERT(t, ReadBytes(path) == "a\r\nb\r\nc\r\n");
    TR_ASSERT(t, buffer->GetBufferState() == TextBuffer::kBuffer_Loaded);
    buffer->ConvertLineEndings(kLineEnding::kLF);
    TR_ASSERT(t, buffer->Save(path));
    TR_ASSERT(t, ReadBytes(path) == "a\nb\nc\n");
    return kTR_Pass;
}

DLL_EXPORT int test_lineendings_mixedconvert(ITesting *t) {
    Defaults();
    Config::Instance()["main"].SetStr("mixed_line_endings", "convert");
    Config::Instance()["main"].SetStr("line_endings", "windows");
    auto path = MakeFile("mixedconvert.txt", "a\nb\r\nc\n");
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer->SaveForce(path));
    TR_ASSERT(t, ReadBytes(path) == "a\r\nb\r\nc\r\n");
    // The answer 'keep' wins over the policy for this buffer
    path = MakeFile("mixedconvert2.txt", "a\nb\r\nc\n");
    buffer = TextBuffer::CreateBufferFromFile(path);
    buffer->KeepMixedLineEndings();
    TR_ASSERT(t, buffer->SaveForce(path));
    TR_ASSERT(t, ReadBytes(path) == "a\nb\r\nc\n");
    // A file that is all one style is never converted, whatever line_endings says
    TR_ASSERT(t, RoundTrips("unixonly.txt", "a\nb\n"));
    Defaults();
    return kTR_Pass;
}

// Text from a Windows program arrives as CRLF over the OS clipboard (CopyFromExternal - the E.18 round trip): the '\r'
// was pasted as text into every line
DLL_EXPORT int test_lineendings_paste(ITesting *t) {
    Defaults();
    // An empty buffer has one empty line to edit in
    auto buffer = TextBuffer::CreateEmptyBuffer();
    if (buffer->NumLines() == 0) {
        buffer->AddLine(U"");
    }
    buffer->LineAt(0)->Append(std::u32string(U"start"));
    TR_ASSERT(t, buffer->NumLines() == 1);
    auto item = ClipBoard::ClipBoardItem::CreateExternal("one\r\ntwo\r\n");
    item->PasteToBuffer(buffer, Point(0, 0));
    TR_ASSERT(t, buffer->NumLines() == 3);
    TR_ASSERT(t, buffer->LineAt(0)->Buffer() == U"one");
    TR_ASSERT(t, buffer->LineAt(1)->Buffer() == U"two");
    TR_ASSERT(t, buffer->LineAt(2)->Buffer() == U"start");
    // A '\r' that doesn't end a line stays text
    auto inline_ = ClipBoard::ClipBoardItem::CreateExternal("a\rb");
    auto inlineBuffer = TextBuffer::CreateEmptyBuffer();
    if (inlineBuffer->NumLines() == 0) {
        inlineBuffer->AddLine(U"");
    }
    inline_->PasteToBuffer(inlineBuffer, Point(0, 0));
    TR_ASSERT(t, inlineBuffer->LineAt(0)->Buffer() == U"a\rb");
    return kTR_Pass;
}

// What a reload compares against carries the endings
DLL_EXPORT int test_lineendings_readlines(ITesting *t) {
    Defaults();
    auto path = MakeFile("readlines.txt", "a\r\nb\nc");
    std::vector<std::u32string> lines;
    TextBuffer::FileLineEndings endings;
    TR_ASSERT(t, TextBuffer::ReadLines(path, lines, nullptr, &endings));
    TR_ASSERT(t, lines.size() == 3);
    TR_ASSERT(t, endings.endings.size() == 3);
    TR_ASSERT(t, endings.endings[0] == kLineEnding::kCRLF);
    TR_ASSERT(t, endings.endings[1] == kLineEnding::kLF);
    TR_ASSERT(t, endings.endings[2] == kLineEnding::kDefault);
    TR_ASSERT(t, !endings.hasFinalNewline);
    return kTR_Pass;
}
