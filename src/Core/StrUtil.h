//
// Created by gnilk on 14.01.23.
//

#ifndef EDITOR_STRUTIL_H
#define EDITOR_STRUTIL_H

#include <string>
#include <vector>


namespace strutil {
    std::string& ltrim(std::string &str, const std::string &chars = "\t\n\v\f\r ");
    std::string& rtrim(std::string &str, const std::string &chars = "\t\n\v\f\r ");
    std::string& trim(std::string &str, const std::string &chars = "\t\n\v\f\r ");

    std::u32string& ltrim(std::u32string  &str, const std::u32string &chars = U"\t\n\v\f\r ");
    std::u32string& rtrim(std::u32string  &str, const std::u32string &chars = U"\t\n\v\f\r ");
    std::u32string& trim(std::u32string  &str, const std::u32string &chars = U"\t\n\v\f\r ");

    void split(std::vector<std::string> &strings, const char *strInput, int splitChar);
    void split(std::vector<std::string> &strings, const std::string &strInput, int splitChar);
    bool isinteger(const std::string &str);
    bool isdouble(const std::string& s);
    uint32_t hex2dec(const char *s);
    uint32_t hex2dec(const std::string &str);

    // The following has been added from the Tokenizer code
    bool skipWhiteSpace(char **input);
    char *getNextTokenNoOperator(char *dst, int nMax, char **input);
    void splitToStringList(std::vector<std::string> &outList, const char *input);
    bool inStringList(std::vector<std::string> &strList, const char *input, int &outSz);

    bool startsWith(const std::string &str, const std::string &prefix);

}



#endif //EDITOR_STRUTIL_H
