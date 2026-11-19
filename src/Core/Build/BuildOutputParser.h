//
// Reads a build's output and picks out its errors and warnings (docs/build-messages.md, hitlist #52).
//
// Streaming: Feed() takes the bytes as they come off the pty - colour escapes, '\r' progress lines, a line split
// across two reads - and a message is listed as soon as its line is complete. The same parser reads a finished
// command's output after the fact (Feed it all, then Finish()). No terminal, no editor: text in, messages out.
//
// The tools it knows are built in (§9.2): gcc / clang (also through make, ninja, cmake --build), CMake's own
// messages, and make's / ninja's 'Entering directory' (what relative paths are relative to).
//

#ifndef GEDIT_BUILDOUTPUTPARSER_H
#define GEDIT_BUILDOUTPUTPARSER_H

#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <set>

#include "BuildMessage.h"

namespace gedit {
    class BuildOutputParser {
    public:
        using Ref = std::shared_ptr<BuildOutputParser>;

    public:
        BuildOutputParser() = default;
        virtual ~BuildOutputParser() = default;

        // Bytes from the output, any length, UTF-8; escapes and '\r' are handled here
        void Feed(std::string_view bytes);
        // The output ended: a last line without its '\n' is read too
        void Finish();
        // Back to nothing read - a new run
        void Reset();

        const std::vector<BuildMessage> &Messages() const { return messages; }
        size_t NumErrors() const;
        size_t NumWarnings() const;
        // Complete lines read so far
        size_t NumLines() const { return numLines; }

    protected:
        // One complete line, escapes stripped, no line end
        void ParseLine(const std::string &line);

    private:
        enum class kEscState {
            kText,
            kEsc,       // after ESC
            kCsi,       // ESC [ ... final byte
            kString,    // ESC ] / P / _ / ^ ... BEL or ESC '\'
            kStringEsc, // ESC inside a string - '\' ends it
        };
        // What the lines after a message are
        enum class kContinuation {
            kNone,
            kCompiler,  // gcc / clang: indented lines (source, caret), 'note:' lines
            kCMake,     // CMake: the indented message lines, blank lines inside
        };

    private:
        void AcceptByte(char ch);
        void EndLine();

        bool ParseDirectory(const std::string &line);
        bool ParseCompiler(const std::string &line);
        bool ParseCMake(const std::string &line);
        bool ParseContinuation(const std::string &line);
        bool ParseIncludeContext(const std::string &line);

        void Add(BuildMessage message, kContinuation nextContinuation, bool isRepeatChecked);
        void AppendWrapped(const std::string &text);
        static std::string RepeatKey(const BuildMessage &message);
        const std::string &CurrentDirectory() const;

    private:
        std::vector<BuildMessage> messages;
        std::set<std::string> seen;     // RepeatKey of every message listed
        size_t numLines = 0;

        // The line being read
        std::string current;
        kEscState escState = kEscState::kText;
        bool isPendingCR = false;

        // make / ninja 'Entering directory', innermost last
        std::vector<std::string> directories;
        // 'In file included from ...' / 'In function ...' - for the message that follows
        std::vector<std::string> pendingContext;
        kContinuation continuation = kContinuation::kNone;
        // The last message was a repeat (a header's warning once per translation unit): its lines are dropped too
        bool isSkippingRepeat = false;
        // CMake: the message text is on the indented line after the header
        bool isCMakeAwaitingText = false;
        // Clang wrapped the message: its indented lines right after it are its text, not detail
        bool isMessageWrapOpen = false;
    };
}

#endif //GEDIT_BUILDOUTPUTPARSER_H
