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

    struct StackableView {
        kLayout layout = kFill;
        ViewBase *view = nullptr;
    } ;

}

#endif //EDITOR_STACKABLEVIEW_H
