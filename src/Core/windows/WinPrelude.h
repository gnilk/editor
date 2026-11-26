//
// Force-included first into every C++ file on Windows (CMakeLists.txt, /FI). Two reasons:
//
// - gnklog's LogCore.h includes <Windows.h> and <process.h> INSIDE 'namespace gnilk' - their declarations land in the
//   wrong namespace and the include guards keep <thread> from seeing '_beginthreadex' again. Included here first, at
//   global scope, gnklog's includes are no-ops. (To fix in gnklog: include them before the namespace, or not in a
//   public header.)
// - <windows.h> defines macros named like our methods (CreateWindow, DrawText, ...). Since gnklog brings it into every
//   file that logs, the function names that clash are undone here, once.
//

#ifndef GOATEDIT_WINPRELUDE_H
#define GOATEDIT_WINPRELUDE_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <process.h>

#undef CreateWindow
#undef DrawText
#undef GetMessage
#undef PostMessage
#undef SendMessage
#undef LoadImage
#undef CreateFont
#undef GetObject
#undef DeleteFile
#undef CopyFile
#undef MoveFile
#undef CreateDirectory
#undef RemoveDirectory
#undef GetCurrentDirectory
#undef SetCurrentDirectory
#undef GetCommandLine
#undef GetUserName
#undef GetClassName
#undef FindWindow
#undef LoadString
#undef Yield
#undef GetCurrentTime
// Not 'near', 'far', 'IN', 'OUT', 'interface', 'small', 'ERROR': Windows' own headers (dxgi, mmreg - SDL includes them
// later) still use those

#endif //GOATEDIT_WINPRELUDE_H
