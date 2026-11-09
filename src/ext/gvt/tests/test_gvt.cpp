//
// gvt smoke test — the test library loads and links against gvt
//
#include <testinterface.h>

#include "gvt/Version.h"

using namespace gnilk::vt;

extern "C" {
DLL_EXPORT int test_gvt(ITesting *t);
DLL_EXPORT int test_gvt_version(ITesting *t);
}

DLL_EXPORT int test_gvt(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_gvt_version(ITesting *t) {
    auto v = GetVersion();
    TR_ASSERT(t, v.major == kVersionMajor);
    TR_ASSERT(t, v.minor == kVersionMinor);
    TR_ASSERT(t, v.patch == kVersionPatch);
    return kTR_Pass;
}
