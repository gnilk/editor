//
// Created by gnilk on 29.08.23.
//

#ifndef GOATEDIT_DOCUMENTAPIWRAPPER_H
#define GOATEDIT_DOCUMENTAPIWRAPPER_H

#include <memory>
#include <string>
#include <string_view>

#include "duktape.h"
#include "Core/API/DocumentAPI.h"

namespace gedit {
    class DocumentAPIWrapper {
    public:
        using Ref = std::shared_ptr<DocumentAPIWrapper>;
    public:
        DocumentAPIWrapper() = default;
        explicit DocumentAPIWrapper(DocumentAPI::Ref doc) : document(doc) {
        }
        virtual ~DocumentAPIWrapper() = default;
        static Ref Create(DocumentAPI::Ref doc) {
            return std::make_shared<DocumentAPIWrapper>(doc);
        }

        static void RegisterModule(duk_context *ctx);


        bool Save();
        bool SaveAs(const char *newFileName);

        const std::string GetName();
        const std::string GetFileName();
    private:
        DocumentAPI::Ref document = nullptr;
    };
}


#endif //GOATEDIT_DOCUMENTAPIWRAPPER_H
