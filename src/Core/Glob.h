//
// Created by gnilk on 29.08.23.
//

#ifndef GOATEDIT_GLOB_H
#define GOATEDIT_GLOB_H

#include <string>

namespace gedit {

    class Glob {
    public:
        enum class kMatch {
            NoMatch,
            Match,
            Error,
        };
    public:
        static kMatch Match(const std::string &pattern, const std::string &string);
        static kMatch Match(const char *pattern, const char *text);

    };
}


#endif //GOATEDIT_GLOB_H
