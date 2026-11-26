//
// Force-included first into the unit tests on Windows (CMakeLists.txt, the utests target). trun's testinterface.h - a
// frozen contract - includes <Windows.h>, whose CreateWindow macro breaks ScreenBase::CreateWindow. Included here first,
// the macro is undone once; testinterface.h's own include is then a no-op. The editor itself never includes <Windows.h>
// outside src/Core/windows/.
//

#ifndef GOATEDIT_WINTESTPRELUDE_H
#define GOATEDIT_WINTESTPRELUDE_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#undef CreateWindow

#endif //GOATEDIT_WINTESTPRELUDE_H
