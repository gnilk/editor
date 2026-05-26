// Single compilation unit for stb single-header library implementations.
// Including the implementation defines here ensures each symbol is defined
// exactly once, regardless of how many backends include these headers.

#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
