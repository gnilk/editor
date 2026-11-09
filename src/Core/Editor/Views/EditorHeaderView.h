//
// Created by gnilk on 18.03.23.
//

#ifndef EDITOR_EDITORHEADERVIEW_H
#define EDITOR_EDITORHEADERVIEW_H

#include "Core/UI/Views/SingleLineView.h"
#include "Core/ActionHelper.h"
#include "Core/Editor.h"

#include <algorithm>
#include <string>
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
                    // The '...' of files that didn't fit: the list of all of them
                    if (hit.document == nullptr) {
                        ActionHelper::ShowOpenFiles();
                    } else {
                        Editor::Instance().SetActiveDocument(hit.document);
                    }
                    return true;
                }
            }
            return false;
        }

        // Which entries the header shows when 'width' columns hold them - joined by ' | ', with '...' and a separator
        // in place of what is cut at either end. The active one is always in: the row starts later rather than
        // leaving it out, and when it is too wide for a '...' before it, it starts the row. [idxFirst, idxEnd).
        struct Span {
            size_t idxFirst = 0;
            size_t idxEnd = 0;
            bool isCutBefore = false;
            bool isCutAfter = false;
        };
        static Span LayoutEntries(const std::vector<int> &widths, size_t idxActive, int width) {
            const int widthSeparator = (int)SEPARATOR.length();
            const int widthCut = (int)ELLIPSIS.length() + widthSeparator;
            size_t numEntries = widths.size();
            if (numEntries == 0) {
                return {};
            }
            idxActive = std::min(idxActive, numEntries - 1);
            auto fits = [&](size_t idxFirst, size_t idxEnd) {
                int total = 0;
                for (size_t i = idxFirst; i < idxEnd; i++) {
                    total += widths[i] + ((i > idxFirst) ? widthSeparator : 0);
                }
                if (idxFirst > 0) {
                    total += widthCut;
                }
                if (idxEnd < numEntries) {
                    total += widthCut;
                }
                return total <= width;
            };
            size_t idxFirst = 0;
            while ((idxFirst < idxActive) && !fits(idxFirst, idxActive + 1)) {
                idxFirst++;
            }
            size_t idxEnd = idxFirst;
            while ((idxEnd < numEntries) && fits(idxFirst, idxEnd + 1)) {
                idxEnd++;
            }
            // Too wide even alone: shown anyway, the screen cuts it
            if (idxEnd <= idxActive) {
                idxEnd = idxActive + 1;
            }
            Span span = {idxFirst, idxEnd, idxFirst > 0, idxEnd < numEntries};
            // The active one alone, and no room for a '... | ' before it: the '...' goes, so the row starts with the
            // name and the screen cuts its end, not its start. The '...' after it still opens the list.
            if (span.isCutBefore && (idxEnd - idxFirst == 1) && ((widths[idxFirst] + widthCut) > width)) {
                span.isCutBefore = false;
            }
            return span;
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
            std::string header = "[Files] ";
            dc.DrawStringWithAttributesAt(xp, 0, kTextAttributes::kNormal, header.c_str());
            xp += (int)header.length();

            // Markers and name per open file - the markers are part of what is clicked
            struct Entry {
                Document::Ref document;
                std::string markers;
                std::string name;
            };
            std::vector<Entry> entries;
            std::vector<int> widths;
            size_t idxActive = 0;
            for (auto &document : documents) {
                auto node = workspace->GetNodeFromDocument(document);
                if (node == nullptr) {
                    continue;
                }
                if (document == activeDocument) {
                    idxActive = entries.size();
                }
                entries.push_back({document, workspace->StatusMarkers(document), node->GetDisplayName()});
                widths.push_back((int)(entries.back().markers.length() + entries.back().name.length()));
            }

            auto span = LayoutEntries(widths, idxActive, GetContentRect().Width() - xp - 1);
            auto drawCut = [&]() {
                dc.DrawStringWithAttributesAt(xp, 0, kTextAttributes::kNormal, ELLIPSIS.c_str());
                headerHits.push_back({xp, xp + (int)ELLIPSIS.length(), nullptr});
                xp += (int)ELLIPSIS.length();
            };
            if (span.isCutBefore) {
                drawCut();
                dc.DrawStringWithAttributesAt(xp, 0, kTextAttributes::kNormal, SEPARATOR.c_str());
                xp += (int)SEPARATOR.length();
            }
            for (size_t i = span.idxFirst; i < span.idxEnd; i++) {
                auto &entry = entries[i];
                // Start of this entry's clickable span (covers the markers + the name).
                int entryStart = xp;
                dc.DrawStringWithAttributesAt(xp, 0, kTextAttributes::kNormal, entry.markers.c_str());
                xp += (int)entry.markers.length();
                auto attributes = (entry.document == activeDocument) ? kTextAttributes::kUnderline : kTextAttributes::kNormal;
                dc.DrawStringWithAttributesAt(xp, 0, attributes, entry.name.c_str());
                xp += (int)entry.name.length();
                // Record the clickable span (markers + name, excluding the separator that follows).
                headerHits.push_back({entryStart, xp, entry.document});

                if ((i + 1 < span.idxEnd) || span.isCutAfter) {
                    dc.DrawStringWithAttributesAt(xp, 0, kTextAttributes::kNormal, SEPARATOR.c_str());
                    xp += (int)SEPARATOR.length();
                }
            }
            if (span.isCutAfter) {
                drawCut();
            }
        }

    private:
        inline static const std::string SEPARATOR = " | ";
        inline static const std::string ELLIPSIS = "...";

        // Clickable column spans built during DrawViewContents, resolved in OnMouseEvent. Content-local
        // columns; xStart inclusive, xEnd exclusive. Rebuilt every draw. No document: a '...'.
        struct HeaderHit {
            int xStart;
            int xEnd;
            Document::Ref document;
        };
        std::vector<HeaderHit> headerHits;
    };
}


#endif //EDITOR_EDITORHEADERVIEW_H
