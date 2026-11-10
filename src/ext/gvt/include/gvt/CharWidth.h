//
// gvt — how many terminal cells a code point takes: 0 (joins the previous cell), 1 or 2.
//
// wcwidth-style rules from a table generated out of the Unicode data (tools/gen_width.py): East Asian Wide/Fullwidth
// and emoji presentation take two cells, combining marks and format characters none. Independent of the C locale,
// so the editor can use it for documents too.
//
#ifndef GVT_CHARWIDTH_H
#define GVT_CHARWIDTH_H

namespace gnilk::vt {

    int CharWidth(char32_t codepoint);

}

#endif // GVT_CHARWIDTH_H
