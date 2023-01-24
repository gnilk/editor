//
// Created by gnilk on 14.01.23.
//

#include <vector>
#include <string>
#include "Core/StrUtil.h"

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
                strings.push_back(str);
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

    bool isnumber(const std::string& s) {
        // not empty, and we want the iterator to any non-digit (or end() if all are digits)
        return (!s.empty() && std::find_if(s.begin(),
                                           s.end(),
                                           [](unsigned char c) { return !std::isdigit(c); }) == s.end());
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


}
