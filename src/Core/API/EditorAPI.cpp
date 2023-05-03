//
// Created by gnilk on 08.04.23.
//

#include "EditorAPI.h"


using namespace gedit;
TextBufferAPI *EditorAPI::GetActiveTextBuffer() {
    auto idxActiveModel = Editor::Instance().GetActiveModelIndex();
    auto model = Editor::Instance().GetModelFromIndex(idxActiveModel);
    return new TextBufferAPI(model->GetTextBuffer());

}