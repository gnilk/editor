//
// UTF-8 <-> UTF-16 at the Windows API's edge (docs/windows-port.md §4.3): the editor's strings are UTF-8 everywhere, the
// W functions take UTF-16. Only for src/Core/windows/ - nothing else sees a wide string.
//

#ifndef GOATEDIT_WINSTRING_H
#define GOATEDIT_WINSTRING_H

#include <string>
#include <string_view>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace gedit::winstring {
    inline std::wstring Widen(std::string_view text) {
        if (text.empty()) {
            return {};
        }
        int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), nullptr, 0);
        std::wstring wide((size_t)length, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), (int)text.size(), wide.data(), length);
        return wide;
    }

    inline std::string Narrow(std::wstring_view wide) {
        if (wide.empty()) {
            return {};
        }
        int length = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
        std::string text((size_t)length, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), text.data(), length, nullptr, nullptr);
        return text;
    }
}

#endif //GOATEDIT_WINSTRING_H
