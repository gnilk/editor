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
// Where a theme comes from and its licence (docs/theme-collection.md §3) are carried into the output - the editor
// never reads them, they are there so a theme can be shared or shipped with its terms. The theme file's own keys win
// over what the caller found around it (the extension's package.json, a LICENSE).
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

        // Empty = not found
        struct Provenance {
            std::string author;
            std::string license;        // as the source names it ('MIT')
            std::string copyright;      // 'Copyright (c) ...'
            std::string source;         // the upstream (a repository URL) or the file imported

            // 'other' fills what this one lacks
            void FillFrom(const Provenance &other);
        };

        struct Result {
            bool isOk = false;
            std::string name;                   // the theme's own name
            std::string colorsJson;             // GoatEdit colors.json
            Provenance provenance;              // the theme's own, filled from 'around' 
            std::vector<std::string> report;    // what was approximated or dropped (or why it failed)
        };
    public:
        // 'around' is what the caller found next to the theme file (FromPackageJson, CopyrightLine)
        static Result Import(const std::string &text, const IncludeLoader &includeLoader = {},
                             const Provenance &around = {});

        // A VS Code extension's package.json: license, author (string or {name}) else publisher, repository
        // (string or {url})
        static Provenance FromPackageJson(const std::string &text);
        // The first 'Copyright ...' line of a LICENSE file, trimmed - empty if there is none
        static std::string CopyrightLine(const std::string &licenseText);

        // VS Code theme files are JSON with comments and trailing commas - drop the commas (comments are
        // left for the JSON reader to skip)
        static std::string RemoveTrailingCommas(const std::string &text);
    };
}

#endif //GOATEDIT_THEMEIMPORTER_H
