//
// Created by gnilk on 29.08.23.
//

#ifndef GOATEDIT_DOCUMENTAPI_H
#define GOATEDIT_DOCUMENTAPI_H

#include "Core/Workspace.h"

namespace gedit {
    class DocumentAPI {
    public:
        using Ref = std::shared_ptr<DocumentAPI>;

    public:
        DocumentAPI() = default;
        explicit DocumentAPI(Workspace::Node::Ref workspaceNode) : document(workspaceNode) {

        }

        static DocumentAPI::Ref Create(Workspace::Node::Ref workspaceNode);

        virtual ~DocumentAPI() = default;

        void SetLanguage(const std::string &extension);
        bool Save();
        bool SaveAs(const std::string &newFileName);

        const std::string GetName();
        const std::string GetFileName();

    private:
        Workspace::Node::Ref document = nullptr;
    };
}


#endif //GOATEDIT_DOCUMENTAPI_H
