//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_EDITCONTROLLER_H
#define EDITOR_EDITCONTROLLER_H

#include "Core/TextBuffer.h"
#include "Core/UndoHistory.h"
#include "Core/Document.h"
#include "Core/KeyMapping.h"
#include "Core/Rect.h"
#include "Core/Language/AutoPairEngine.h"
#include "BaseController.h"
#include "logger.h"
#include <memory>
#include <deque>

namespace gedit {

    // EditController holds the raw-KeyPress editing logic (char insert + undo bracketing, line-join
    // on backspace/delete at a boundary, selection-aware delete-on-type). Resolved Actions go
    // straight to the Document (Document::OnAction) - the controller no longer proxies them. It is a
    // per-item member of EditorView and borrows (does NOT own) the document it is attached to; the
    // Workspace owns document lifetime. Attach/Detach re-point it as the view switches documents.
    class EditController : public BaseController {
    public:
        using Ref = std::shared_ptr<EditController>;

    public:
        EditController() = default;
        virtual ~EditController() = default;
        static Ref Create(const Document::Ref &newDocument);

        // Borrow / release the document this controller edits. Non-owning: the handle must outlive
        // the attachment (the Workspace owns it; the view detaches before it goes away).
        void Attach(const Document::Ref &newDocument) {
            document = newDocument.get();
            Begin();
        }
        void Detach() {
            document = nullptr;
        }

        void Begin() override;

        bool HandleKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress) override;
        bool HandleSpecialKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress);

        void MoveLineUp(Cursor &cursor, size_t &idxActiveLine);

        bool OnKeyPress(const KeyPress &keyPress);

    protected:
        bool HandleSpecialKeyPressForEditor(Cursor &cursor, size_t &idxLine, const KeyPress &keyPress);

        // Auto-pairing glue: the ONE place that knows how to wire AutoPairEngine (table by language,
        // token-class at the cursor). Callers just consult the returned Action. (Selection-wrap is not
        // wired yet - see docs/autopair-plan.md "3b".)
        AutoPairEngine::Context BuildAutoPairContext(const Line::Ref &line, const Cursor &cursor, bool selectionActive);
    private:
        gnilk::ILogger *logger = nullptr;
        Document *document = nullptr;       // borrowed - lifetime owned by the Workspace

    };
}


#endif //EDITOR_EDITCONTROLLER_H
