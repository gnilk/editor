//
// Created by gnilk on 08.04.23.
//

#include <memory>
#include "EditorAPI.h"


using namespace gedit;
TextBufferAPI::Ref EditorAPI::GetActiveTextBuffer() {
    auto idxActiveModel = Editor::Instance().GetActiveModelIndex();
    auto model = Editor::Instance().GetModelFromIndex(idxActiveModel);
    return std::make_shared<TextBufferAPI>(model->GetTextBuffer());

}