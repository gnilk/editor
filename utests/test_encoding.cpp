//
// Text encodings on load and save (open-bugs #15): a file that isn't UTF-8 (Latin-1 / cp1252 comments in old C code)
// was cut at its first non-UTF-8 byte on load, and the save wrote the cut lines back. Now: a BOM is exact, valid UTF-8
// is UTF-8, anything else is decoded with the single-byte fallback (main.encoding_fallback, cp1252) - and the save
// writes the encoding it was loaded in (main.convert_encoding: keep), byte for byte.
//
#include <testinterface.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "Core/Config/Config.h"
#include "Core/TextBuffer.h"
#include "Core/TextEncoding.h"

using namespace gedit;
namespace fs = std::filesystem;

extern "C" {
    DLL_EXPORT int test_encoding(ITesting *t);
    DLL_EXPORT int test_encoding_detect(ITesting *t);
    DLL_EXPORT int test_encoding_cp1252(ITesting *t);
    DLL_EXPORT int test_encoding_names(ITesting *t);
    DLL_EXPORT int test_encoding_latin1roundtrip(ITesting *t);
    DLL_EXPORT int test_encoding_utf8roundtrip(ITesting *t);
    DLL_EXPORT int test_encoding_bomroundtrip(ITesting *t);
    DLL_EXPORT int test_encoding_utf16roundtrip(ITesting *t);
    DLL_EXPORT int test_encoding_unencodable(ITesting *t);
    DLL_EXPORT int test_encoding_autoconvert(ITesting *t);
    DLL_EXPORT int test_encoding_converted(ITesting *t);
    DLL_EXPORT int test_encoding_splitlines(ITesting *t);
}

namespace {
    using kEncoding = TextEncoding::kEncoding;

    fs::path MakeFile(const std::string &name, const std::string &bytes) {
        auto root = fs::canonical(fs::temp_directory_path()) / "goatedit_encoding";
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

    // From noc_goa src/extras/pcx.c - the line the bug cut at 'oj'
    const std::string latin1File = "int a;\n\t\t\t\tif (pcx->width&1) /* Kolla om oj\xe4mn bredd */\n/* g\xf6r om s\xe5 */\nint b;\n";

    void Defaults() {
        Config::Instance()["main"].SetStr("encoding_fallback", "cp1252");
        Config::Instance()["main"].SetStr("convert_encoding", "keep");
    }
}

DLL_EXPORT int test_encoding(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_encoding_detect(ITesting *t) {
    TR_ASSERT(t, TextEncoding::Detect("plain ascii\n", kEncoding::kCP1252) == kEncoding::kUTF8);
    TR_ASSERT(t, TextEncoding::Detect("r\xc3\xa4ksm\xc3\xb6rg\xc3\xa5s", kEncoding::kCP1252) == kEncoding::kUTF8);
    TR_ASSERT(t, TextEncoding::Detect("\xef\xbb\xbfhello", kEncoding::kCP1252) == kEncoding::kUTF8BOM);
    TR_ASSERT(t, TextEncoding::Detect(std::string("\xff\xfeh\0", 4), kEncoding::kCP1252) == kEncoding::kUTF16LE);
    TR_ASSERT(t, TextEncoding::Detect(std::string("\xfe\xff\0h", 4), kEncoding::kCP1252) == kEncoding::kUTF16BE);
    // Not UTF-8: the fallback, whichever it is
    TR_ASSERT(t, TextEncoding::Detect("oj\xe4mn", kEncoding::kCP1252) == kEncoding::kCP1252);
    TR_ASSERT(t, TextEncoding::Detect("oj\xe4mn", kEncoding::kLatin1) == kEncoding::kLatin1);
    // A UTF-8 sequence cut at the end of the text is not UTF-8
    TR_ASSERT(t, TextEncoding::Detect("abc\xc3", kEncoding::kCP1252) == kEncoding::kCP1252);
    // Overlong '/' and a surrogate are not UTF-8 either
    TR_ASSERT(t, TextEncoding::Detect("a\xc0\xaf", kEncoding::kCP1252) == kEncoding::kCP1252);
    TR_ASSERT(t, TextEncoding::Detect("a\xed\xa0\x80", kEncoding::kCP1252) == kEncoding::kCP1252);
    return kTR_Pass;
}

DLL_EXPORT int test_encoding_cp1252(ITesting *t) {
    // 0x80-0x9f are cp1252's own (euro, quotes, ...); the five it leaves undefined stay their C1 code point, so every
    // byte decodes and encodes back to itself
    std::string allBytes;
    for (int ch = 0; ch < 256; ch++) {
        allBytes.push_back((char)ch);
    }
    auto text = TextEncoding::Decode(allBytes, kEncoding::kCP1252);
    TR_ASSERT(t, text.size() == 256);
    TR_ASSERT(t, text[0x80] == U'€');
    TR_ASSERT(t, text[0x93] == U'“');
    TR_ASSERT(t, text[0x81] == U'\u0081');
    TR_ASSERT(t, text[0xe4] == U'ä');
    std::string back;
    size_t idxFailed = 0;
    TR_ASSERT(t, TextEncoding::Encode(text, kEncoding::kCP1252, back, idxFailed));
    TR_ASSERT(t, back == allBytes);
    // Latin-1 is every byte as its own code point
    auto latin1 = TextEncoding::Decode(allBytes, kEncoding::kLatin1);
    TR_ASSERT(t, latin1[0x80] == U'\u0080');
    back.clear();
    TR_ASSERT(t, TextEncoding::Encode(latin1, kEncoding::kLatin1, back, idxFailed));
    TR_ASSERT(t, back == allBytes);
    // What a single byte can't hold
    TR_ASSERT(t, !TextEncoding::Encode(U"ok \U0001F600", kEncoding::kCP1252, back, idxFailed));
    TR_ASSERT(t, idxFailed == 3);
    TR_ASSERT(t, !TextEncoding::Encode(U"€", kEncoding::kLatin1, back, idxFailed));
    // ...and Unicode encodings hold everything
    back.clear();
    TR_ASSERT(t, TextEncoding::Encode(U"ok \U0001F600", kEncoding::kUTF16LE, back, idxFailed));
    TR_ASSERT(t, TextEncoding::Decode(back, kEncoding::kUTF16LE) == U"ok \U0001F600");
    return kTR_Pass;
}

DLL_EXPORT int test_encoding_names(ITesting *t) {
    TR_ASSERT(t, TextEncoding::FromName("cp1252") == kEncoding::kCP1252);
    TR_ASSERT(t, TextEncoding::FromName("Windows-1252") == kEncoding::kCP1252);
    TR_ASSERT(t, TextEncoding::FromName("latin1") == kEncoding::kLatin1);
    TR_ASSERT(t, TextEncoding::FromName("ISO-8859-1") == kEncoding::kLatin1);
    TR_ASSERT(t, TextEncoding::FromName("utf8") == kEncoding::kUTF8);
    TR_ASSERT(t, TextEncoding::FromName("UTF-8") == kEncoding::kUTF8);
    TR_ASSERT(t, TextEncoding::FromName("utf-16le") == kEncoding::kUTF16LE);
    TR_ASSERT(t, !TextEncoding::FromName("ebcdic").has_value());
    for (auto encoding : {kEncoding::kUTF8, kEncoding::kUTF8BOM, kEncoding::kUTF16LE, kEncoding::kUTF16BE, kEncoding::kCP1252, kEncoding::kLatin1}) {
        TR_ASSERT(t, TextEncoding::FromName(TextEncoding::Name(encoding)) == encoding);
    }
    return kTR_Pass;
}

// The bug: load cut the line at the first non-UTF-8 byte and the save wrote it back cut
DLL_EXPORT int test_encoding_latin1roundtrip(ITesting *t) {
    Defaults();
    auto path = MakeFile("pcx.c", latin1File);
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->NumLines() == 4);
    TR_ASSERT(t, buffer->LineAt(1)->Buffer() == U"\t\t\t\tif (pcx->width&1) /* Kolla om ojämn bredd */");
    TR_ASSERT(t, buffer->LineAt(2)->Buffer() == U"/* gör om så */");
    TR_ASSERT(t, buffer->GetEncoding() == kEncoding::kCP1252);
    TR_ASSERT(t, buffer->SaveForce(path));
    TR_ASSERT(t, ReadBytes(path) == latin1File);
    return kTR_Pass;
}

DLL_EXPORT int test_encoding_utf8roundtrip(ITesting *t) {
    Defaults();
    std::string utf8File = "/* r\xc3\xa4ksm\xc3\xb6rg\xc3\xa5s \xe2\x82\xac */\nint a;\n";
    auto path = MakeFile("utf8.c", utf8File);
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->GetEncoding() == kEncoding::kUTF8);
    TR_ASSERT(t, buffer->LineAt(0)->Buffer() == U"/* räksmörgås € */");
    TR_ASSERT(t, buffer->SaveForce(path));
    TR_ASSERT(t, ReadBytes(path) == utf8File);
    return kTR_Pass;
}

DLL_EXPORT int test_encoding_bomroundtrip(ITesting *t) {
    Defaults();
    std::string bomFile = "\xef\xbb\xbf" "first\nsecond\n";
    auto path = MakeFile("bom.txt", bomFile);
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->GetEncoding() == kEncoding::kUTF8BOM);
    // The BOM is not text
    TR_ASSERT(t, buffer->LineAt(0)->Buffer() == U"first");
    TR_ASSERT(t, buffer->SaveForce(path));
    TR_ASSERT(t, ReadBytes(path) == bomFile);
    return kTR_Pass;
}

DLL_EXPORT int test_encoding_utf16roundtrip(ITesting *t) {
    Defaults();
    // "hä\n\U0001F600\n" in UTF-16 LE, with its BOM
    std::string utf16File("\xff\xfe" "h\0" "\xe4\0" "\n\0" "\x3d\xd8\x00\xde" "\n\0", 14);
    auto path = MakeFile("utf16.txt", utf16File);
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->GetEncoding() == kEncoding::kUTF16LE);
    TR_ASSERT(t, buffer->NumLines() == 2);
    TR_ASSERT(t, buffer->LineAt(0)->Buffer() == U"hä");
    TR_ASSERT(t, buffer->LineAt(1)->Buffer() == U"\U0001F600");
    TR_ASSERT(t, buffer->SaveForce(path));
    TR_ASSERT(t, ReadBytes(path) == utf16File);
    return kTR_Pass;
}

// 'keep' and a character the encoding can't hold: the save is refused and the file left as it was - never cut
DLL_EXPORT int test_encoding_unencodable(ITesting *t) {
    Defaults();
    auto path = MakeFile("unencodable.c", latin1File);
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer != nullptr);
    buffer->LineAt(0)->Append(U'\U0001F600');
    TR_ASSERT(t, !buffer->Save(path));
    TR_ASSERT(t, ReadBytes(path) == latin1File);
    // The euro sign is cp1252's
    buffer->LineAt(0)->Delete((int)buffer->LineAt(0)->Length() - 1);
    buffer->LineAt(0)->Append(U'€');
    TR_ASSERT(t, buffer->Save(path));
    TR_ASSERT(t, ReadBytes(path).substr(0, 7) == "int a;\x80");
    return kTR_Pass;
}

// 'auto': whatever was loaded, the save writes UTF-8
DLL_EXPORT int test_encoding_autoconvert(ITesting *t) {
    Defaults();
    Config::Instance()["main"].SetStr("convert_encoding", "auto");
    auto path = MakeFile("auto.c", latin1File);
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->SaveForce(path));
    Defaults();
    TR_ASSERT(t, ReadBytes(path) == "int a;\n\t\t\t\tif (pcx->width&1) /* Kolla om oj\xc3\xa4mn bredd */\n/* g\xc3\xb6r om s\xc3\xa5 */\nint b;\n");
    TR_ASSERT(t, buffer->GetEncoding() == kEncoding::kUTF8);
    return kTR_Pass;
}

// Converting on purpose ('.encoding utf-8'): a change to save, even with no line touched
DLL_EXPORT int test_encoding_converted(ITesting *t) {
    Defaults();
    auto path = MakeFile("converted.c", latin1File);
    auto buffer = TextBuffer::CreateBufferFromFile(path);
    TR_ASSERT(t, buffer != nullptr);
    TR_ASSERT(t, buffer->GetBufferState() == TextBuffer::kBuffer_Loaded);
    buffer->SetEncoding(kEncoding::kUTF8);
    TR_ASSERT(t, buffer->GetBufferState() == TextBuffer::kBuffer_Changed);
    TR_ASSERT(t, buffer->Save(path));
    TR_ASSERT(t, ReadBytes(path).find("oj\xc3\xa4mn") != std::string::npos);
    return kTR_Pass;
}

// The git version of a file (the gutter's change marks) splits and decodes as a load does, so an untouched Latin-1
// line compares equal
DLL_EXPORT int test_encoding_splitlines(ITesting *t) {
    Defaults();
    std::vector<std::u32string> fromGit;
    TextBuffer::SplitLines(latin1File, fromGit);
    std::vector<std::u32string> fromDisk;
    auto path = MakeFile("split.c", latin1File);
    TR_ASSERT(t, TextBuffer::ReadLines(path, fromDisk));
    TR_ASSERT(t, fromGit == fromDisk);
    TR_ASSERT(t, fromGit.size() == 4);
    TR_ASSERT(t, fromGit[2] == U"/* gör om så */");
    return kTR_Pass;
}
