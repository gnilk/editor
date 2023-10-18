//
// Created by gnilk on 14.01.23.
//

#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include "Core/StrUtil.h"
#include "Core/UnicodeHelper.h"

namespace strutil {
    std::string &ltrim(std::string &s, const std::string &chars /* = "\t\n\v\f\r " */) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [chars](unsigned char ch) {
            return !std::strchr(chars.c_str(), ch);
            // Note: Can't use 'isspace' as some routines depends on us being able to provide a custom string...
            //return !std::isspace(ch);
        }));
        return s;
    }


    std::string &rtrim(std::string &s, const std::string &chars /* = "\t\n\v\f\r " */) {
        s.erase(std::find_if(s.rbegin(), s.rend(), [chars](unsigned char ch) {
            return !std::strchr(chars.c_str(), ch);
        }).base(), s.end());
        return s;
    }

    std::string &trim(std::string &str, const std::string &chars /* = "\t\n\v\f\r " */) {
        return ltrim(rtrim(str, chars), chars);
    }

    std::u32string& ltrim(std::u32string  &str, const std::u32string &chars) {
        str.erase(str.begin(), std::find_if(str.begin(), str.end(), [chars](char32_t ch) {
            return (chars.find(ch) == std::u32string::npos);
        }));
        return str;
    }
    std::u32string& rtrim(std::u32string  &str, const std::u32string &chars) {
        str.erase(std::find_if(str.rbegin(), str.rend(), [chars](char32_t ch) {
            return (chars.find(ch) == std::u32string::npos);
        }).base(), str.end());
        return str;
    }
    std::u32string& trim(std::u32string  &str, const std::u32string &chars) {
        return ltrim(rtrim(str, chars), chars);
    }


    void split(std::vector<std::string> &strings, const char *strInput, int splitChar) {
        std::string input(strInput);
        size_t iPos = 0;
        while (iPos != std::string::npos) {
            size_t iStart = iPos;
            iPos = input.find(splitChar, iPos);
            if (iPos != std::string::npos) {
                std::string str = input.substr(iStart, iPos - iStart);
                trim(str);
                // 2018-11-29, Gnilk: Push back even if length is 0
                strings.emplace_back(str);
                iPos++;
            } else {
                std::string str = input.substr(iStart, input.length() - iStart);
                trim(str);
                if (str.length() > 0) {
                    strings.push_back(str);
                }
            }
        }
    }

    void split(std::vector<std::string> &strings, const std::string &strInput, int splitChar) {
        return split(strings, strInput.c_str(), splitChar);
    }

    void split(std::vector<std::u32string> &strings, const std::u32string &strInput, char32_t splitChar) {
        size_t iPos = 0;
        while (iPos != std::string::npos) {
            size_t iStart = iPos;
            iPos = strInput.find(splitChar, iPos);
            if (iPos != std::u32string::npos) {
                auto str = strInput.substr(iStart, iPos - iStart);
                trim(str);
                // 2018-11-29, Gnilk: Push back even if length is 0
                strings.emplace_back(str);
                iPos++;
            } else {
                auto str = strInput.substr(iStart, strInput.length() - iStart);
                trim(str);
                if (str.length() > 0) {
                    strings.push_back(str);
                }
            }
        }
    }



    bool isinteger(const std::string& s) {
        // not empty, and we want the iterator to any non-digit (or end() if all are digits)
        return (!s.empty() && std::find_if(s.begin(),
                                           s.end(),
                                           [](unsigned char c) { return !std::isdigit(c); }) == s.end());
    }

    bool isdouble(const std::string& s)
    {
        char* end = nullptr;
        double val = strtod(s.c_str(), &end);
        return end != s.c_str() && *end == '\0' && val != HUGE_VAL;
    }


//
// hex2dec converts an hexadecimal string to it's base 10 representative
// prefix either with 0x or $ or skip if you know....
//
// 16 = hex2dec("0x10")
// 16 = hex2dec("$10")
// 16 = hex2dec("10")
//

    uint32_t hex2dec(const char *s) {
        uint32_t n = 0;
        size_t length = strlen(s);
        size_t idxStart = 0;

        if (s[0] == '$') {
            idxStart = 1;
        } else if ((length > 2) && (s[0] == '0') && (s[1] == 'x')) {
            idxStart = 2;
        }

        for (size_t i = idxStart; i < length && s[i] != '\0'; i++) {
            int v = 0;
            if ('a' <= s[i] && s[i] <= 'f') { v = s[i] - 97 + 10; }
            else if ('A' <= s[i] && s[i] <= 'F') { v = s[i] - 65 + 10; }
            else if ('0' <= s[i] && s[i] <= '9') { v = s[i] - 48; }
            else break;
            n *= 16;
            n += v;
        }
        return n;
    }

    uint32_t hex2dec(const std::string &str) {
        return hex2dec(str.c_str());
    }

    //
    // The following comes originally from the tokenizer code
    //
    // Skips white space and moves input forward..
    bool skipWhiteSpace(char **input) {
        if (**input == '\0') {
            return false;
        }
        while ((isspace(**input)) && (**input != '\0')) {
            (*input)++;
        }
        if (**input == '\0') {
            return false;    // only trailing space
        }
        return true;
    }
    bool skipWhiteSpace(std::u32string::const_iterator &it) {
        if (*it == U'\0') {
            return false;
        }
        while(std::isspace(*it) && *it != U'\0') {
            it++;
        }
        if (*it == U'\0') {
            return false;
        }
        return true;
    }


    char *getNextTokenNoOperator(char *dst, int nMax, char **input) {
        if (!skipWhiteSpace(input)) {
            return nullptr;
        }

        int i = 0;
        while (!isspace(**input) && (**input != '\0')) {
            dst[i++] = **input;

            // This is a developer problem, ergo - safe to exit..
            if (i >= nMax) {
                fprintf(stderr, "ERR: GetNextToken, token size larger than buffer (>nMax)\n");
                exit(1);
            }

            (*input)++;
        }
        dst[i] = '\0';
        return dst;
    }

    bool getNextTokenNoOperator(std::u32string &dst, std::u32string::const_iterator &it) {
        if (!skipWhiteSpace(it)) {
            return false;
        }
        while(!std::isspace(*it) && (*it != U'\0')) {
            dst.push_back(*it);
            it++;
        }
        return true;
    }

    void splitToStringList(std::vector<std::string> &outList, const char *input) {
        char tmp[256];
        char *parsepoint = (char *)input;

        if (input == nullptr) {
            return;
        }

        while (getNextTokenNoOperator(tmp, 256, &parsepoint)) {
            outList.emplace_back(std::string(tmp));
        }
    }

    bool inStringList(std::vector<std::string> &strList, const char *input, int &outSz) {
        for (auto s: strList) {
            if (!strncmp(s.c_str(), input, s.size())) {
                outSz = s.size();
                return true;
            }
        }
        return false;
    }

    void splitToStringList(std::vector<std::u32string> &outList, const std::u32string &input) {
        if (input.empty()) {
            return;
        }
        std::u32string str;
        auto it = input.begin();
        while(getNextTokenNoOperator(str, it)) {
            outList.emplace_back(str);
            str = U"";
        }
    }

    bool inStringList(std::vector<std::u32string> &strList, const std::u32string &input) {
        return false;
    }


    std::u32string itou32(int num) {
        char tmp[32];
        snprintf(tmp, 32, "%d", num);
        std::u32string str;
        gedit::UnicodeHelper::ConvertUTF8ToUTF32String(str, tmp);
        return str;
    }

    std::u32string itou32(size_t num) {
        char tmp[32];
        snprintf(tmp, 32, "%zu", num);
        std::u32string str;
        gedit::UnicodeHelper::ConvertUTF8ToUTF32String(str, tmp);
        return str;
    }


    bool isspace(const char32_t ch) {
        static std::u32string defaultLocaleSpace(U" \f\n\r\t\v");
        return (defaultLocaleSpace.find(ch) != std::u32string::npos);
    }



    bool startsWith(const std::string &str, const std::string &prefix) {
        if (prefix.length() > str.length()) return false;
        for (size_t i = 0; i < prefix.length(); i++) {
            if (prefix[i] != str[i]) return false;
        }
        return true;
    }
    bool startsWith(const std::u32string &str, const std::u32string &prefix) {
        if (prefix.length() > str.length()) return false;
        for (size_t i = 0; i < prefix.length(); i++) {
            if (prefix[i] != str[i]) return false;
        }
        return true;
    }



}
