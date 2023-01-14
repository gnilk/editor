//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_STRUTIL_H
#define EDITOR_STRUTIL_H

#include <string>

namespace strutil {
    std::string& rtrim(std::string& str, const std::string& chars = "\t\n\v\f\r ");
}

#endif //EDITOR_STRUTIL_H
