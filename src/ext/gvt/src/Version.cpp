//
// gvt version stamp
//
#include "gvt/Version.h"

namespace gnilk::vt {

    Version GetVersion() {
        return {kVersionMajor, kVersionMinor, kVersionPatch};
    }

    const char *GetVersionString() {
        return "0.1.0";
    }

}
