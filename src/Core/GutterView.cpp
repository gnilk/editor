//
// Created by gnilk on 14.02.23.
//

#include "GutterView.h"

using namespace gedit;

void GutterView::DrawViewContents() {
    auto &ctx = ViewBase::ContentAreaDrawContext();
    char str[64];
    for(int i=0;i<ctx.ContextRect().Height();i++) {
        snprintf(str, 64, " %4d", i);
        if (i == 9) {
            snprintf(str, 64, "*%4d", i);
        }
        ctx.DrawStringAt(0,i,str);
    }
}
