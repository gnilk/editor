//
// Theme importer (docs/code-index.md §10.1, step 1c, D13): converts a VS Code theme or a Sublime colour
// scheme ONCE into GoatEdit's own colors.json, which the user then tunes. The editor never reads foreign
// formats at runtime.
//
// Both sources name code colours by TextMate scope. Each GoatEdit colour key has a short list of the scopes
// its tokens would carry in a C/C++ grammar; the best (most specific, then latest) rule matching one of them
// gives the colour. Context selectors ('meta.class entity.name.type'), exclusions and font styles don't map -
// they are approximated or dropped, and the report says so.
//
// Plain data in, plain data out: the caller reads the files (an included parent theme comes through
// 'IncludeLoader') and writes the result.
//

#ifndef GOATEDIT_THEMEIMPORTER_H
#define GOATEDIT_THEMEIMPORTER_H

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace gedit {
    class ThemeImporter {
    public:
        // Text of a file a theme includes, by the path the theme names - nullopt when it can't be read
        using IncludeLoader = std::function<std::optional<std::string>(const std::string &path)>;

        struct Result {
            bool isOk = false;
            std::string name;                   // the theme's own name
            std::string colorsJson;             // GoatEdit colors.json
            std::vector<std::string> report;    // what was approximated or dropped (or why it failed)
        };
    public:
        static Result Import(const std::string &text, const IncludeLoader &includeLoader = {});

        // VS Code theme files are JSON with comments and trailing commas - drop the commas (comments are
        // left for the JSON reader to skip)
        static std::string RemoveTrailingCommas(const std::string &text);
    };
}

#endif //GOATEDIT_THEMEIMPORTER_H
