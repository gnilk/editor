//
// Created by gnilk on 29.08.23.
//

#ifndef GOATEDIT_PATHUTIL_H
#define GOATEDIT_PATHUTIL_H

#include <string>
#include <filesystem>

namespace pathutil {
    std::string LastNameOfPath(const std::filesystem::path &pathName);
}


#endif //GOATEDIT_PATHUTIL_H
