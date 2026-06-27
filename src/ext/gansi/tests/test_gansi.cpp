//
// gansi smoke test — verifies the test harness loads the library and links against it.
// Real per-feature modules (encoder, parser, grid) land in Phase 1+.
//
#include <testinterface.h>

#include "gansi/Version.h"

using namespace gnilk::ansi;

extern "C" {
DLL_EXPORT int test_gansi(ITesting *t);
DLL_EXPORT int test_gansi_version(ITesting *t);
}

DLL_EXPORT int test_gansi(ITesting *t) {
    return kTR_Pass;
}

DLL_EXPORT int test_gansi_version(ITesting *t) {
    auto v = GetVersion();
    TR_ASSERT(t, v.major == kVersionMajor);
    TR_ASSERT(t, v.minor == kVersionMinor);
    TR_ASSERT(t, v.patch == kVersionPatch);
    return kTR_Pass;
}
