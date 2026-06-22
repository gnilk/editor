//
// Created by gnilk on 18.03.23.
//

#ifndef EDITOR_EDITORHEADERVIEW_H
#define EDITOR_EDITORHEADERVIEW_H

#include "Core/UI/Views/SingleLineView.h"

#include <vector>

namespace gedit {
    class EditorHeaderView : public SingleLineView {
    public:
        EditorHeaderView() = default;
        virtual ~EditorHeaderView() = default;

        // Clicking a file name in the header switches the active document. The header layout is fully
        // dynamic (markers, variable name widths, separators), so the draw pass is the only honest
        // source of where each name sits - it records the clickable column spans into headerHits and
        // this resolves a click against them. Draw and mouse dispatch are both main-thread, so the
        // draw-populated cache is safe to read here without locking.
        bool OnMouseEvent(const MouseEvent &mouseEvent) override {
            if (mouseEvent.kind != MouseEvent::kMouseEventKind_Press) {
                return false;
            }
            int x = mouseEvent.x - GetContentRect().TopLeft().x;
            for (auto &hit : headerHits) {
                if ((x >= hit.xStart) && (x < hit.xEnd)) {
                    Editor::Instance().SetActiveDocument(hit.document);
                    return true;
                }
            }
            return false;
        }

    protected:
        void DrawViewContents() override {
            auto &dc = window->GetContentDC();

            dc.ResetDrawColors();

            headerHits.clear();

            auto workspace = Editor::Instance().GetWorkspace();
            auto &documents = Editor::Instance().GetDocuments();
            auto activeDocument = Editor::Instance().GetActiveDocument();
            auto theme = Editor::Instance().GetTheme();
            auto uiColors = theme->GetUIColors();
            if (parentView->IsActive()) {
                dc.SetColor(uiColors["header_active_foreground"], uiColors["header_active_background"]);
            } else {
                dc.SetColor(uiColors["header_foreground"], uiColors["header_background"]);
            }

            dc.FillLine(0, kTextAttributes::kNormal, ' ');

            int xp = 0;

            std::string header = "[Files]";
            dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kNormal, header.c_str());

            xp += header.length();
            dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kNormal, " ");
            xp++;
            for(size_t i=0;i<documents.size();i++) {
                auto document = documents[i];
                auto node = workspace->GetNodeFromDocument(documents[i]);
                if (node == nullptr) {
                    continue;
                }
                auto &name = node->GetDisplayName();
                header = name;

                // Start of this entry's clickable span (covers the markers + the name).
                int entryStart = xp;

                // Add marker for changed
                if (document->GetTextBuffer()->GetBufferState() == TextBuffer::kBuffer_Changed) {
                    dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kNormal, "* ");
                    xp += 2;
                }
                if (document->GetTextBuffer()->IsReadOnly()) {
                    dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kNormal, "R ");
                    xp += 2;
                }

                if (document == activeDocument) {
                    dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kUnderline, header.c_str());
                } else {
                    dc.DrawStringWithAttributesAt(xp,0,kTextAttributes::kNormal, header.c_str());
                }
                xp += header.length();

                // Record the clickable span (markers + name, excluding the separator that follows).
                headerHits.push_back({entryStart, xp, document});

                if (i < (documents.size()-1)) {
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

    private:
        // Clickable column spans built during DrawViewContents, resolved in OnMouseEvent. Content-local
        // columns; xStart inclusive, xEnd exclusive. Rebuilt every draw.
        struct HeaderHit {
            int xStart;
            int xEnd;
            Document::Ref document;
        };
        std::vector<HeaderHit> headerHits;
    };
}


#endif //EDITOR_EDITORHEADERVIEW_H
