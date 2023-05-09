//
// Created by gnilk on 09.05.23.
//

#ifndef EDITOR_WORKSPACE_H
#define EDITOR_WORKSPACE_H

#include <vector>
#include <string>

#include "logger.h"

#include "EditorModel.h"

namespace gedit {
    class Workspace {
    public:
        Workspace() = default;
        virtual ~Workspace() = default;

        std::vector<EditorModel::Ref> &GetModels();
        size_t GetActiveModelIndex();

        void SetActiveModel(TextBuffer::Ref textBuffer);

        size_t NextModelIndex(size_t idxCurrent);
        EditorModel::Ref GetModelFromIndex(size_t idxModel);

        EditorModel::Ref NewModel(const char *name);
        EditorModel::Ref LoadModel(const std::string &filename);


    protected:
        EditorModel::Ref LoadEditorModelFromFile(const char *filename);

    private:
        gnilk::ILogger *logger = nullptr;
        std::vector<EditorModel::Ref> models;   // rename..

    };
}


#endif //EDITOR_WORKSPACE_H
