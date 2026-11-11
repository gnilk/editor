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
        bool IsModified();
        // The file on disk differs from the one this document last read or wrote - another program rewrote it
        // (docs/usability-round.md §0b)
        bool IsChangedOnDisk();
        // Take in what the file holds now, dropping unsaved edits - one undo step, so they can be stepped back to
        bool Reload();
        // Reformat (docs/usability-round.md §8) - the indent engine does the work, in C++; one undo item each.
        // False when there is nothing to do: no document, or a language whose reformat is off (YAML).
        bool ReformatFile();
        bool ReformatBlock();       // the selection, else the enclosing block, else the line - as Cmd+I
        bool IsReformatEnabled();
        // The language's identifier ("cpp", "yaml") - for the scripts' messages
        const std::string GetLanguage();

        const std::string GetName();
        const std::string GetFileName();

    private:
        Workspace::Node::Ref document = nullptr;
    };
}


#endif //GOATEDIT_DOCUMENTAPI_H
