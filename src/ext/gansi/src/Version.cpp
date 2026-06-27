//
// gansi version stamp — the one guaranteed TU so the static library always links.
//
#include "gansi/Version.h"

namespace gnilk::ansi {

    Version GetVersion() {
        return {kVersionMajor, kVersionMinor, kVersionPatch};
    }

    const char *GetVersionString() {
        return "0.1.0";
    }

}
