//
// gvt — terminal emulation library. Standalone: nothing here may depend on the editor.
// Public namespace is gnilk::vt.
//
#ifndef GVT_VERSION_H
#define GVT_VERSION_H

namespace gnilk::vt {

    struct Version {
        int major;
        int minor;
        int patch;
    };

    inline constexpr int kVersionMajor = 0;
    inline constexpr int kVersionMinor = 1;
    inline constexpr int kVersionPatch = 0;

    Version GetVersion();
    const char *GetVersionString();

}

#endif // GVT_VERSION_H
