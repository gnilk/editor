//
// Created by gnilk on 16.03.23.
//

#ifndef EDITOR_SINGLELINEVIEW_H
#define EDITOR_SINGLELINEVIEW_H

#include "Core/Editor.h"
#include "Core/RuntimeConfig.h"
#include "ViewBase.h"

namespace gedit {
    class SingleLineView : public ViewBase {
    public:
        SingleLineView() = default;
        explicit SingleLineView(const Rect &rect) : ViewBase (rect) {

        }
        void InitView() override {
            auto screen = RuntimeConfig::Instance().Screen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }
            viewRect.SetHeight(1);
            window = screen->CreateWindow(viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
            window->SetCaption("SingleLineView");
        }

        void ReInitView() override {
            auto screen = RuntimeConfig::Instance().Screen();
            if (viewRect.IsEmpty()) {
                viewRect = screen->Dimensions();
            }
            viewRect.SetHeight(1);
            window = screen->UpdateWindow(window, viewRect, WindowBase::kWin_Visible, WindowBase::kWinDeco_None);
        }
    protected:
        void DrawViewContents() override {
            auto &dc = window->GetContentDC();
            auto &models = Editor::Instance().GetModels();
            // FIXME: should have attribute...
            std::string header = "Files:>| ";
            for(auto &m : models) {
                auto &name = m->GetTextBuffer()->Name();

                if (m->IsActive()) {
                    header += "! ";
                }

                header += name;
                header += " | ";
            }
            //dc.DrawStringAt(0,0,"File1.txt | File2.txt | File3.txt                 - wefwef");
            dc.DrawStringAt(0,0,header.c_str());
        }

    };
}

#endif //EDITOR_SINGLELINEVIEW_H
