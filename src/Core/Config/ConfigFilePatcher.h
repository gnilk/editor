//
// In-place, comment-preserving edit of a YAML config FILE (text), for '.cfg set'. yaml-cpp drops every
// comment when a document is loaded and written back, and the config files are hand-maintained (comments
// are used to switch logging etc. on/off), so a set must never round-trip a file through yaml-cpp.
//
// SetScalar edits text only: it walks the block-style indentation to the key path and replaces just the
// value on that one line (keeping a trailing '# comment'), or inserts the missing key(s) at the end of the
// deepest existing section (a missing top-level section is appended at the end of the file). Every other
// line is left byte-for-byte as it was.
//
// There is NO fallback: the patched text is re-parsed and must equal "original + this one value" exactly;
// anything it can't place safely (flow style, a list/map at the path, tab indentation, CRLF, a file that
// doesn't parse) is refused with an error - the caller then points the user at a manual edit.
//

#ifndef EDITOR_CONFIGFILEPATCHER_H
#define EDITOR_CONFIGFILEPATCHER_H

#include <string>
#include <vector>

namespace gedit {
    class ConfigFilePatcher {
    public:
        struct Result {
            bool isOk = false;
            std::string text;       // the patched file contents (when isOk)
            std::string error;      // why it was refused (when !isOk)
        };
    public:
        // Set the scalar at 'keyPath' (e.g. {"editorview", "wrap"}) to 'value' in the YAML 'text'.
        static Result SetScalar(const std::string &text, const std::vector<std::string> &keyPath, const std::string &value);
        // 'value' as YAML: plain when that reads back unchanged, otherwise double-quoted.
        static std::string FormatScalar(const std::string &value);
    };
}

#endif //EDITOR_CONFIGFILEPATCHER_H
