//
// The mouse pointer's shape (docs/usability-round.md §2): what the views ask for, what a screen shows. Plain - the views
// decide, the backend only draws it.
//

#ifndef GOATEDIT_POINTERSHAPE_H
#define GOATEDIT_POINTERSHAPE_H

#include <cstdint>

namespace gedit {
    enum class kPointerShape : uint8_t {
        kDefault,           // the arrow
        kText,              // the I-beam, over text
        kResizeColumns,     // a vertical divider, dragged left and right
        kResizeRows,        // a horizontal divider, dragged up and down
    };
}

#endif //GOATEDIT_POINTERSHAPE_H
