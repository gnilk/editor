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
    void split(std::vector<std::u32string> &strings, const std::u32string &strInput, char32_t splitChar);
    bool isinteger(const std::string &str);
    bool isdouble(const std::string& s);
    uint32_t hex2dec(const char *s);
    uint32_t hex2dec(const std::string &str);

    std::u32string itou32(int num);
    std::u32string itou32(size_t num);

    bool isspace(const char32_t ch);

    // The following has been added from the Tokenizer code
    bool skipWhiteSpace(char **input);
    bool skipWhiteSpace(std::u32string::const_iterator &it);
    char *getNextTokenNoOperator(char *dst, int nMax, char **input);
    bool getNextTokenNoOperator(std::u32string &dst, std::u32string::const_iterator &it);
    void splitToStringList(std::vector<std::string> &outList, const char *input);
    bool inStringList(std::vector<std::string> &strList, const char *input, int &outSz);

    void splitToStringList(std::vector<std::u32string> &outList, const std::u32string &input);
    bool inStringList(std::vector<std::u32string> &strList, const std::u32string &input);

    bool startsWith(const std::string &str, const std::string &prefix);
    bool startsWith(const std::u32string &str, const std::u32string &prefix);

}



#endif //EDITOR_STRUTIL_H
