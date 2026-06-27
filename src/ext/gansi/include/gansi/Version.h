//
// gansi — modern-terminal (ANSI/VT) library.
//
// Standalone: nothing here may depend on the editor. Public namespace is gnilk::ansi.
//
#ifndef GANSI_VERSION_H
#define GANSI_VERSION_H

namespace gnilk::ansi {

    // Semantic version of the gansi library.
    struct Version {
        int major;
        int minor;
        int patch;
    };

    // Compile-time version constants.
    inline constexpr int kVersionMajor = 0;
    inline constexpr int kVersionMinor = 1;
    inline constexpr int kVersionPatch = 0;

    // Library version (defined in Version.cpp so there is one real TU to link).
    Version GetVersion();

    // Human-readable "major.minor.patch".
    const char *GetVersionString();

}

#endif // GANSI_VERSION_H
