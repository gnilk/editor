//
// Created by gnilk on 15.02.23.
//

#ifndef EDITOR_EDITCONTROLLER_H
#define EDITOR_EDITCONTROLLER_H

#include "Core/TextBuffer.h"
#include "Core/UndoHistory.h"
#include "Core/EditorModel.h"
#include "Core/KeyMapping.h"
#include "Core/Rect.h"
#include "BaseController.h"
#include "logger.h"
#include <memory>
#include <deque>

namespace gedit {

    class EditController : public BaseController {
    public:
        using Ref = std::shared_ptr<EditController>;
        using TextBufferChangedDelegate = std::function<void()>;

    public:
        EditController() = default;
        EditController(EditorModel::Ref newModel) : model(newModel) {

        }
        virtual ~EditController() = default;
        static Ref Create(EditorModel::Ref newModel);

        void Begin() override;
        void SetTextBuffer(TextBuffer::Ref newTextBuffer);

        const TextBuffer::Ref GetTextBuffer() {
            return model->GetTextBuffer();
        }
        void SetTextBufferChangedHandler(TextBufferChangedDelegate newOnTextBufferChanged) {
            onTextBufferChanged = newOnTextBufferChanged;
        }
        bool HandleKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress) override;
        bool HandleSpecialKeyPress(Cursor &cursor, size_t &idxActiveLine, const KeyPress &keyPress);

        // Returns index to the new active line
        size_t NewLine(size_t idxCurrentLine, Cursor &cursor);
        void MoveLineUp(Cursor &cursor, size_t &idxActiveLine);

        // Proxy for buffer
        const std::vector<Line::Ref> &Lines() {
            return model->Lines();
        }
        // Const accessor...
        const std::vector<Line::Ref> &Lines() const {
            return model->Lines();
        }
        Line::Ref LineAt(size_t idxLine) {
            return model->LineAt(idxLine);
        }

        UndoHistory::UndoItem::Ref BeginUndoItem();
        void EndUndoItem(UndoHistory::UndoItem::Ref undoItem);

        void UpdateSyntaxForBuffer();   // Does a full buffer reparse of the syntax
        Job::Ref UpdateSyntaxForActiveLineRegion(); // Special partial case - activeline +/- 2 lines
        Job::Ref UpdateSyntaxForRegion(size_t idxStartLine, size_t idxEndLine); // Partial reparse (between line index)

        void PasteFromClipboard(LineCursor &lineCursor);

        // Newly moved stuff from EditorView
        void OnViewInit(const Rect &viewRect);
        bool OnKeyPress(const KeyPress &keyPress);
        void HandleKeyPressWithSelection(const KeyPress &keyPress);
        bool OnAction(const KeyPressAction &kpAction);



    protected:
        void DeleteLinesNoSyntaxUpdate(size_t idxLineStart, size_t idxLineEnd);
        bool HandleSpecialKeyPressForEditor(Cursor &cursor, size_t &idxLine, const KeyPress &keyPress);
    private:
        gnilk::ILogger *logger = nullptr;
        EditorModel::Ref model;
        TextBufferChangedDelegate onTextBufferChanged = nullptr;

    };
}


#endif //EDITOR_EDITCONTROLLER_H
