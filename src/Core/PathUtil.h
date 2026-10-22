//
// Created by gnilk on 29.08.23.
//

#ifndef GOATEDIT_PATHUTIL_H
#define GOATEDIT_PATHUTIL_H

#include <string>
#include <filesystem>

namespace pathutil {
    std::string LastNameOfPath(const std::filesystem::path &pathName);
    // Absolute, normalized, symlinks resolved as far as the path exists (no trailing '.' or '/'). The
    // workspace tree is kept in this form - it is also what the OS folder monitors report.
    std::filesystem::path CanonicalPath(const std::filesystem::path &pathName);
}


#endif //GOATEDIT_PATHUTIL_H
