//
// Created by gnilk on 29.08.23.
//

#include "PathUtil.h"

// This returns the last valid name of a full path
// /src/app/myapp  => myapp
// /src/app/myapp/ => myapp     <- this is the reason we have this function...
std::string pathutil::LastNameOfPath(const std::filesystem::path &pathName) {
    auto it = pathName.end();
    it--;
    if ((it->string() != "") && (it->string() != ".")) {
        return it->string();
    }
    if (it == pathName.begin()) {
        return "";
    }
    it--;
    return it->string();
}