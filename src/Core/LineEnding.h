//
// Line endings (docs/windows-port.md §4.2): each line keeps the ending it was read with; a line an edit made has none
// of its own (kDefault) and is written in the document's style - the one most of its lines have. The names the user
// sees and types are 'unix' and 'windows', never LF / CRLF (nobody remembers which is which).
//

#ifndef GOATEDIT_LINEENDING_H
#define GOATEDIT_LINEENDING_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace gedit {
    enum class kLineEnding : uint8_t {
        kDefault,       // made by an edit: the document's style
        kLF,            // unix
        kCRLF,          // windows
    };

    // What a document's lines say, counted where it's cheap (load, convert, save) - never on a keystroke
    struct LineEndingCounts {
        size_t numLF = 0;
        size_t numCRLF = 0;

        bool IsMixed() const {
            return (numLF > 0) && (numCRLF > 0);
        }
        void Add(kLineEnding ending) {
            if (ending == kLineEnding::kLF) {
                numLF++;
            } else if (ending == kLineEnding::kCRLF) {
                numCRLF++;
            }
        }
        // The style a kDefault line is written in: the most lines', 'preferred' when none (or a tie) says
        kLineEnding Dominant(kLineEnding preferred) const {
            if (numCRLF > numLF) {
                return kLineEnding::kCRLF;
            }
            if (numLF > numCRLF) {
                return kLineEnding::kLF;
            }
            return preferred;
        }
    };

    namespace LineEndings {
        // 'unix' / 'windows' / 'platform' ('windows' on Windows, 'unix' elsewhere), 'lf' / 'crlf' read as aliases;
        // nullopt for anything else
        std::optional<kLineEnding> FromName(std::string_view name, std::string_view platformName);
        // 'unix' / 'windows' (kDefault: 'unix')
        const char *Name(kLineEnding ending);
        // What ends a line in the file
        const char32_t *Text(kLineEnding ending);

        // main.line_endings, on this platform - a new file's style, and what 'convert' makes
        kLineEnding Preferred();

        enum class kMixedPolicy : uint8_t {
            kAsk,       // the save asks: keep or convert
            kKeep,      // each line as it is
            kConvert,   // to Preferred()
        };
        // main.mixed_line_endings - 'ask' when unset or unknown
        kMixedPolicy MixedPolicy();
        std::optional<kMixedPolicy> MixedPolicyFromName(std::string_view name);
    }
}

#endif //GOATEDIT_LINEENDING_H
