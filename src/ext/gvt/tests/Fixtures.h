//
// gvt tests — byte-stream fixtures. tests/fixtures/*.bin is copied to <build>/gvt-fixtures/ (trun runs from the
// build directory). See tests/fixtures/README.md for what each one is and how it was recorded.
//
#ifndef GVT_TESTS_FIXTURES_H
#define GVT_TESTS_FIXTURES_H

#include <string>
#include <vector>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <algorithm>

namespace gvttest {

    inline const std::filesystem::path kFixtureDir = "gvt-fixtures";

    inline std::vector<std::filesystem::path> ListFixtures() {
        std::vector<std::filesystem::path> result;
        std::error_code ec;
        for (auto &entry : std::filesystem::directory_iterator(kFixtureDir, ec)) {
            if (entry.path().extension() == ".bin") {
                result.push_back(entry.path());
            }
        }
        std::sort(result.begin(), result.end());
        return result;
    }

    inline std::string LoadFixture(const std::filesystem::path &path) {
        std::ifstream in(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    }

}

#endif // GVT_TESTS_FIXTURES_H
