//
// Created by gnilk on 29.08.23.
//

#include "dukglue/dukglue.h"

#include "DocumentAPIWrapper.h"

using namespace gedit;

void DocumentAPIWrapper::RegisterModule(duk_context *ctx) {
    dukglue_register_constructor_managed<DocumentAPIWrapper>(ctx, "Document");
    dukglue_register_delete<DocumentAPIWrapper>(ctx);

    dukglue_register_method(ctx, &DocumentAPIWrapper::GetName, "GetName");
    dukglue_register_method(ctx, &DocumentAPIWrapper::GetFileName, "GetFileName");

    dukglue_register_method(ctx, &DocumentAPIWrapper::Save, "Save");
    dukglue_register_method(ctx, &DocumentAPIWrapper::SaveAs, "SaveAs");


}

const std::string DocumentAPIWrapper::GetName() {
    if (document == nullptr) {
        return "";
    }
    return document->GetName();
}
const std::string DocumentAPIWrapper::GetFileName() {
    if (document == nullptr) {
        return "";
    }
    return document->GetFileName();
}


bool DocumentAPIWrapper::Save() {
    if (document == nullptr) {
        return false;
    }
    return document->Save();
}
bool DocumentAPIWrapper::SaveAs(const char *newFileName) {
    if (document == nullptr) {
        return false;
    }
    return document->SaveAs(newFileName);
}

