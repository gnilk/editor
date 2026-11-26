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
    dukglue_register_method(ctx, &DocumentAPIWrapper::IsModified, "IsModified");
    dukglue_register_method(ctx, &DocumentAPIWrapper::IsChangedOnDisk, "IsChangedOnDisk");
    dukglue_register_method(ctx, &DocumentAPIWrapper::Reload, "Reload");
    dukglue_register_method(ctx, &DocumentAPIWrapper::GetEncoding, "GetEncoding");
    dukglue_register_method(ctx, &DocumentAPIWrapper::SetEncoding, "SetEncoding");
    dukglue_register_method(ctx, &DocumentAPIWrapper::GetLineEndings, "GetLineEndings");
    dukglue_register_method(ctx, &DocumentAPIWrapper::SetLineEndings, "SetLineEndings");
    dukglue_register_method(ctx, &DocumentAPIWrapper::GetPreferredLineEndings, "GetPreferredLineEndings");
    dukglue_register_method(ctx, &DocumentAPIWrapper::IsLineEndingsQuestion, "IsLineEndingsQuestion");
    dukglue_register_method(ctx, &DocumentAPIWrapper::KeepLineEndings, "KeepLineEndings");
    dukglue_register_method(ctx, &DocumentAPIWrapper::GotoLine, "GotoLine");
    dukglue_register_method(ctx, &DocumentAPIWrapper::ReformatFile, "ReformatFile");
    dukglue_register_method(ctx, &DocumentAPIWrapper::ReformatBlock, "ReformatBlock");
    dukglue_register_method(ctx, &DocumentAPIWrapper::IsReformatEnabled, "IsReformatEnabled");
    dukglue_register_method(ctx, &DocumentAPIWrapper::GetLanguage, "GetLanguage");


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

bool DocumentAPIWrapper::IsModified() {
    if (document == nullptr) {
        return false;
    }
    return document->IsModified();
}

bool DocumentAPIWrapper::IsChangedOnDisk() {
    if (document == nullptr) {
        return false;
    }
    return document->IsChangedOnDisk();
}

const std::string DocumentAPIWrapper::GetEncoding() {
    if (document == nullptr) {
        return {};
    }
    return document->GetEncoding();
}

bool DocumentAPIWrapper::SetEncoding(const char *name) {
    if ((document == nullptr) || (name == nullptr)) {
        return false;
    }
    return document->SetEncoding(name);
}

const std::string DocumentAPIWrapper::GetLineEndings() {
    if (document == nullptr) {
        return {};
    }
    return document->GetLineEndings();
}

bool DocumentAPIWrapper::SetLineEndings(const char *name) {
    if ((document == nullptr) || (name == nullptr)) {
        return false;
    }
    return document->SetLineEndings(name);
}

const std::string DocumentAPIWrapper::GetPreferredLineEndings() {
    if (document == nullptr) {
        return {};
    }
    return document->GetPreferredLineEndings();
}

bool DocumentAPIWrapper::IsLineEndingsQuestion() {
    if (document == nullptr) {
        return false;
    }
    return document->IsLineEndingsQuestion();
}

void DocumentAPIWrapper::KeepLineEndings() {
    if (document == nullptr) {
        return;
    }
    document->KeepLineEndings();
}

bool DocumentAPIWrapper::GotoLine(int lineNumber) {
    if (document == nullptr) {
        return false;
    }
    return document->GotoLine(lineNumber);
}

bool DocumentAPIWrapper::Reload() {
    if (document == nullptr) {
        return false;
    }
    return document->Reload();
}

bool DocumentAPIWrapper::ReformatFile() {
    if (document == nullptr) {
        return false;
    }
    return document->ReformatFile();
}

bool DocumentAPIWrapper::ReformatBlock() {
    if (document == nullptr) {
        return false;
    }
    return document->ReformatBlock();
}

bool DocumentAPIWrapper::IsReformatEnabled() {
    if (document == nullptr) {
        return false;
    }
    return document->IsReformatEnabled();
}

const std::string DocumentAPIWrapper::GetLanguage() {
    if (document == nullptr) {
        return "";
    }
    return document->GetLanguage();
}
