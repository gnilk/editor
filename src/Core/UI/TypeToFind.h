//
// TypeToFind - letters typed in a list or a tree jump to the next row whose name starts with them (docs/usability-round.md
// §3 a). Plain: characters and a clock in, a prefix out; names and a row in, a row out. Knows nothing of views.
//
// The letters typed within a pause make the prefix; after the pause the next letter starts a new one. While the prefix
// grows the row it is on still counts ("do" stays on "docs/"); a single letter goes on to the next match, and so does
// one letter typed again and again when no name starts with the repeat ("d", "d", "d" steps through the d's) - as
// Finder does. Case doesn't count, the search wraps around.
//
#ifndef GEDIT_TYPETOFIND_H
#define GEDIT_TYPETOFIND_H

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace gedit {

    class TypeToFind {
    public:
        using Clock = std::chrono::steady_clock;

        static constexpr std::chrono::milliseconds kDefaultPause{1000};

    public:
        TypeToFind() = default;
        explicit TypeToFind(Clock::duration pause) : pause(pause) {
        }
        virtual ~TypeToFind() = default;

        // A typed character: the prefix it is now part of - a new one after a pause
        const std::u32string &Type(char32_t ch, Clock::time_point now);
        // Another key (an arrow, Enter): the next letter starts a new prefix
        void Reset() {
            prefix.clear();
        }
        const std::u32string &Prefix() const {
            return prefix;
        }
        void SetPause(Clock::duration newPause) {
            pause = newPause;
        }

        // The row whose name starts with 'prefix', from 'idxCurrent' on (see above); none when no name does
        static std::optional<size_t> FindNext(const std::vector<std::u32string> &names, size_t idxCurrent,
                                              const std::u32string &prefix);

    private:
        static std::optional<size_t> FindFrom(const std::vector<std::u32string> &names, size_t idxStart,
                                              const std::u32string &prefix);
        static bool StartsWith(const std::u32string &name, const std::u32string &prefix);
        static bool IsOneLetterRepeated(const std::u32string &prefix);

    private:
        Clock::duration pause = kDefaultPause;
        std::u32string prefix;
        Clock::time_point timeLastTyped = {};
    };

}

#endif //GEDIT_TYPETOFIND_H
