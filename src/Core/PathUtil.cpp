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
std::filesystem::path pathutil::CanonicalPath(const std::filesystem::path &pathName) {
    std::error_code ec;
    auto result = std::filesystem::weakly_canonical(std::filesystem::absolute(pathName), ec);
    if (ec) {
        return std::filesystem::absolute(pathName).lexically_normal();
    }
    // weakly_canonical keeps a trailing separator when the input ended in '.' or '/'
    if (!result.has_filename() && result.has_parent_path() && (result != result.root_path())) {
        result = result.parent_path();
    }
    return result;
}
