//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_STRUTIL_H
#define EDITOR_STRUTIL_H

#include <string>

namespace strutil {
    std::string& ltrim(std::string& str, const std::string& chars = "\t\n\v\f\r ");
    std::string& rtrim(std::string& str, const std::string& chars = "\t\n\v\f\r ");
    std::string& trim(std::string& str, const std::string& chars = "\t\n\v\f\r ");
    void split(std::vector<std::string> &strings, const char *strInput, int splitChar);
    bool isnumber(const std::string &str);
    uint32_t hex2dec(const char *s);
    uint32_t hex2dec(const std::string &str);

}

#endif //EDITOR_STRUTIL_H
