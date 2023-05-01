
#ifndef GEDIT_TEXTBUFFERAPIWRAPPER_H
#define GEDIT_TEXTBUFFERAPIWRAPPER_H


#include "duktape.h"

class TextBufferAPIWrapper{
public:
    void SetLanguage(int param);
    static void RegisterModule(duk_context *ctx);
};

#endif