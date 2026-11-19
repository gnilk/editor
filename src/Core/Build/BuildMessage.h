//
// One error / warning a build printed, with the place it points at (docs/build-messages.md §6, hitlist #52)
//

#ifndef GEDIT_BUILDMESSAGE_H
#define GEDIT_BUILDMESSAGE_H

#include <cstdint>
#include <string>
#include <vector>

namespace gedit {
    struct BuildMessage {
        enum class kSeverity {
            kError,
            kWarning,
            kNote,
        };

        kSeverity severity = kSeverity::kError;
        // As the tool printed it - absolute or relative; empty when the message names no file ('CMake Error: ...')
        std::string path;
        // What a relative path is relative to when the output said so (make's / ninja's 'Entering directory'); empty
        // otherwise - the owner then tries the shell's directory and the project root (§5)
        std::string directory;
        uint32_t line = 0;          // from 1, as printed; 0 = none
        uint32_t column = 0;        // from 1, as printed; 0 = none
        std::string message;        // the first line, UTF-8, escapes stripped
        std::vector<std::string> detail;    // the lines that belong to it: include chain, notes, the source + caret
        size_t outputLine = 0;      // the output line (from 0) the message starts on

        bool IsError() const { return severity == kSeverity::kError; }
        bool IsWarning() const { return severity == kSeverity::kWarning; }
    };
}

#endif //GEDIT_BUILDMESSAGE_H
