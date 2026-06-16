//
// Created by gnilk on 16.03.23.
//

#ifndef EDITOR_STACKABLEVIEW_H
#define EDITOR_STACKABLEVIEW_H

#include <stdint.h>
#include "ViewBase.h"


namespace gedit {
    typedef enum : int32_t {
        kFill = 1,
        kFixed = 2,
    } kLayout;

    static const std::string &LayoutToString(kLayout layout) {
        static std::string sFill = "Fill";
        static std::string sFixed = "Fixed";
        static std::string sUnknown = "Unknown";
        switch (layout) {
            case kFill:
                return sFill;
            case kFixed:
                return sFixed;
            default:
                return sUnknown;
        }
    }

    struct StackableView {
        kLayout layout = kFill;
        ViewBase *view = nullptr;
    } ;

}

#endif //EDITOR_STACKABLEVIEW_H
