//
// Created by gnilk on 16.05.23.
//

#ifndef EDITOR_ACTIONHELPER_H
#define EDITOR_ACTIONHELPER_H

#include "Core/Document.h"
#include "Core/Line.h"
#include "Core/Index/WorkspaceNames.h"
#include <cstdint>
#include <string>

#include "Core/Index/GoToResolver.h"

namespace gedit {
    class ActionHelper {
    public:
        static void SwitchToNextBuffer();
        static void SwitchToPreviousBuffer();
        // Tab 'tabNumber' (1..9, 9 = last) becomes the active buffer and the editor gets focus
        static void SwitchToBufferNumber(int tabNumber);
        static void SwitchToNamedView(const std::string &viewName);
        static void ShowOpenFiles();
        static void ShowFind();
        static void ShowGotoSymbol();
        // Open a file by part of its name (docs/usability-round.md §3 b) - walks the roots again meanwhile
        static void ShowQuickOpen();
        // Opens the file as it was left (an open one keeps its cursor) and puts the jump on the navigation stack
        static bool OpenFile(const std::string &path);
        // The active document's file, canonical; empty when there is none
        static std::string ActivePath();
        static void ShowRelationships();
        // Smart go-to on the name under the cursor: one place jumps, several open a list (docs/code-index.md §5.6)
        static void GoToNameAtCursor(GoToResolver::kMode mode);
        static void GoToRelatedAtCursor(GoToResolver::kRelated related);
        static void GoToTypeAtCursor();
        // The parameters and locals in sight of the cursor, from the lines of the function it is in (docs/code-index.md §11.11 C)
        static GoToResolver::Locals LocalsAtCursor(const Document::Ref &document, const std::string &path);
        static void GoToHits(std::vector<CodeIndex::Hit> hits, Document::Ref document, const std::string &path);
        // Opens the place and puts the jump on the navigation stack
        static bool JumpTo(const std::string &path, uint32_t line, uint32_t column);
        // A line as the editor has it, in its colours: a copy from the open document, with its unsaved edits, else from the
        // file. nullptr if there is none.
        static Line::Ref SourceLine(const std::string &path, uint32_t line);
        // One line coloured on its own: by the language of 'extension', then by what the workspace names mean. It has no
        // lines around it, so the middle of a block comment reads as code.
        static Line::Ref ColouredLine(const std::u32string &text, const std::string &extension, const WorkspaceNames::Ref &workspaceNames);
        static void JumpBack();
        static void JumpForward();
    };
}


#endif //EDITOR_ACTIONHELPER_H
