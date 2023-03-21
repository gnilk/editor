//
// Created by gnilk on 18.03.23.
//

#ifndef EDITOR_HEADERVIEW_H
#define EDITOR_HEADERVIEW_H

#include "SingleLineView.h"

namespace gedit {
    class HeaderView : public SingleLineView {
    public:
        HeaderView() = default;
        virtual ~HeaderView() = default;
    protected:
        void DrawViewContents() override {
            auto &dc = window->GetContentDC();
            auto &models = Editor::Instance().GetModels();
            dc.FillLine(0, kTextAttributes::kInverted, ' ');

            int xp = 0;

            // FIXME: Should be space enough to fill the gutter, as we want filename 'tabs' list starting over the editor view
            std::string header = "Files:>";
            dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kNormal | kTextAttributes::kInverted, header.c_str());

            xp += header.length();
            dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kNormal, " ");
            xp ++;
            for(size_t i=0;i<models.size();i++) {
                auto m = models[i];
                auto &name = m->GetTextBuffer()->Name();
                header = name;
                if (m->IsActive()) {
                    dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kUnderline, header.c_str());
                } else {
                    dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kNormal, header.c_str());
                }
                xp += header.length();
                if (i < (models.size()-1)) {
                    header = " | ";
                    dc.DrawStringWithAttributesAt(xp, 0, kTextAttributes::kNormal, header.c_str());
                    xp += header.length();
                } else {
                    header = " ";
                    dc.DrawStringWithAttributesAt(xp, 0, kTextAttributes::kNormal, header.c_str());
                    xp += header.length();
                }
            }
            //dc.DrawStringWithAttributesAt(0,0,kTextAttributes::kUnderline, header.c_str());
        }
    };
}


#endif //EDITOR_HEADERVIEW_H