//
// Created by gnilk on 16.05.23.
//

#include "ActionHelper.h"
#include "Core/Editor.h"
#include "Core/UI/Views/RootView.h"
#include "Core/Editor/Modals/FindModal.h"
#include "Core/Editor/Modals/SymbolSearchModal.h"
#include "Core/PathUtil.h"
#include "Core/Session/SessionManager.h"

using namespace gedit;

// The active document's path as the index knows it. Empty if it has no place on disk.
static std::string ActivePath() {
    auto &editor = Editor::Instance();
    auto document = editor.GetActiveDocument();
    if (document == nullptr) {
        return {};
    }
    auto node = editor.GetWorkspace()->GetNodeFromDocument(document);
    return (node == nullptr) ? std::string{} : pathutil::CanonicalPath(node->GetNodePath()).string();
}

static NavigationStack::Entry CurrentPlace() {
    auto &editor = Editor::Instance();
    auto document = editor.GetActiveDocument();
    if (document == nullptr) {
        return {};
    }
    auto &lineCursor = document->GetLineCursor();
    return NavigationStack::MakeEntry(editor.GetCodeIndex().get(), ActivePath(), (uint32_t)lineCursor.idxActiveLine, (uint32_t)lineCursor.cursor.position.x);
}

// A file can have shrunk since the place was stored
static bool OpenAt(const std::string &path, uint32_t line, uint32_t column) {
    auto document = Editor::Instance().LoadDocument(path);
    if (document == nullptr) {
        return false;
    }
    auto buffer = document->GetTextBuffer();
    if (buffer->NumLines() == 0) {
        return false;
    }
    auto idxLine = std::min((size_t)line, buffer->NumLines() - 1);
    auto idxChar = std::min((size_t)column, buffer->LineAt(idxLine)->Length());
    Editor::Instance().SetActiveDocument(document);
    document->CancelSelection();
    document->SetCursorPosition(idxLine, idxChar);
    document->ScrollActiveLineIntoLowerPart();
    if (RuntimeConfig::Instance().HasRootView()) {
        ActionHelper::SwitchToNamedView(glbEditorView);
    }
    return true;
}

// A place that can't be opened any more is stepped over
template<typename Step>
static void TakeStep(Step step) {
    auto &editor = Editor::Instance();
    auto index = editor.GetCodeIndex().get();
    auto &stack = editor.GetNavigationStack();
    auto current = CurrentPlace();
    while (true) {
        auto entry = step(stack, current);
        if (!entry.has_value()) {
            return;
        }
        if (OpenAt(entry->path, NavigationStack::CurrentLine(index, *entry), entry->column)) {
            SessionManager::Instance().NotifyChanged();
            return;
        }
        current = *entry;
    }
}

// What the name was coloured as says more than the index's silence
static bool IsLibraryName(Line &line, size_t idxName) {
    line.Lock();
    auto tokenClass = kLanguageTokenClass::kRegular;
    for (auto &attrib : line.Attributes()) {
        if ((size_t)attrib.idxOrigString > idxName) {
            break;
        }
        tokenClass = attrib.tokenClass;
    }
    line.Release();
    return (tokenClass == kLanguageTokenClass::kLibraryType) || (tokenClass == kLanguageTokenClass::kLibraryFunction);
}

void ActionHelper::SwitchToNextBuffer() {
    auto idxCurrent = Editor::Instance().GetActiveDocumentIndex();
    auto idxNext = Editor::Instance().NextDocumentIndex(idxCurrent);
    if (idxCurrent == idxNext) {
        return;
    }
    Editor::Instance().SetActiveDocumentFromIndex(idxNext);
}

void ActionHelper::SwitchToPreviousBuffer() {
    auto idxCurrent = Editor::Instance().GetActiveDocumentIndex();
    auto idxNext = Editor::Instance().PreviousDocumentIndex(idxCurrent);
    if (idxCurrent == idxNext) {
        return;
    }
    Editor::Instance().SetActiveDocumentFromIndex(idxNext);
}

void ActionHelper::SwitchToBufferNumber(int tabNumber) {
    auto idxDocument = Editor::Instance().GetWorkspace()->DocumentIndexForTabNumber(tabNumber);
    if (!idxDocument.has_value()) {
        return;
    }
    Editor::Instance().SetActiveDocumentFromIndex(*idxDocument);
    // Jumping to a buffer means working in it - from the workspace or the terminal too
    SwitchToNamedView(glbEditorView);
}

// Find in the active document - starts with the selection when it is on one line
void ActionHelper::ShowFind() {
    auto document = Editor::Instance().GetActiveDocument();
    if ((document == nullptr) || !RuntimeConfig::Instance().HasRootView()) {
        return;
    }
    auto modal = FindModal::Create(document);
    auto findModal = modal.get();
    auto rootView = RuntimeConfig::Instance().GetRootViewAs<RootView>();
    rootView->ShowModal(std::move(modal));
    // After ShowModal - the list height comes from the laid out modal
    findModal->SetQuery(FindModal::SelectedText(document));
}

// Search the code index - starts with the selection when it is on one line
void ActionHelper::ShowGotoSymbol() {
    auto &editor = Editor::Instance();
    if ((editor.GetCodeIndex() == nullptr) || !RuntimeConfig::Instance().HasRootView()) {
        return;
    }
    auto document = editor.GetActiveDocument();
    auto modal = SymbolSearchModal::Create(editor.GetCodeIndex(), document, ActivePath(), editor.GetIndexedRoots());
    auto symbolModal = modal.get();
    auto rootView = RuntimeConfig::Instance().GetRootViewAs<RootView>();
    rootView->ShowModal(std::move(modal));
    // After ShowModal - the list height comes from the laid out modal
    symbolModal->SetQuery(FindModal::SelectedText(document));
}

void ActionHelper::GoToNameAtCursor(GoToResolver::kMode mode) {
    auto &editor = Editor::Instance();
    auto document = editor.GetActiveDocument();
    if ((document == nullptr) || (editor.GetCodeIndex() == nullptr)) {
        return;
    }
    auto &lineCursor = document->GetLineCursor();
    auto line = document->GetTextBuffer()->LineAt(lineCursor.idxActiveLine);
    if (line == nullptr) {
        return;
    }
    auto path = ActivePath();
    size_t idxChar = (size_t)lineCursor.cursor.position.x;
    line->Lock();
    auto text = line->Buffer();
    line->Release();

    auto result = GoToResolver::Resolve(*editor.GetCodeIndex(), text, idxChar, (uint32_t)lineCursor.idxActiveLine, path, mode);
    if (result.name.empty()) {
        return;
    }
    if (result.isAlreadyThere) {
        editor.ShowStatusMessage(U"'" + result.name + U"' has no other place");
    } else if (result.hits.empty()) {
        bool isLibrary = IsLibraryName(*line, result.idxName);
        editor.ShowStatusMessage(U"'" + result.name + (isLibrary ? U"' is from a library" : U"' isn't in the index"));
    } else if (result.hits.size() == 1) {
        JumpTo(result.hits[0].path, result.hits[0].line, result.hits[0].column);
    } else if (RuntimeConfig::Instance().HasRootView()) {
        auto modal = SymbolSearchModal::CreateForHits(std::move(result.hits), document, path, editor.GetIndexedRoots());
        auto symbolModal = modal.get();
        RuntimeConfig::Instance().GetRootViewAs<RootView>()->ShowModal(std::move(modal));
        // After ShowModal - the list height comes from the laid out modal
        symbolModal->SetQuery(U"");
    }
}

bool ActionHelper::JumpTo(const std::string &path, uint32_t line, uint32_t column) {
    auto &editor = Editor::Instance();
    auto index = editor.GetCodeIndex().get();
    auto from = CurrentPlace();
    if (!OpenAt(path, line, column)) {
        return false;
    }
    editor.GetNavigationStack().Jump(from, NavigationStack::MakeEntry(index, path, line, column));
    SessionManager::Instance().NotifyChanged();
    return true;
}

void ActionHelper::JumpBack() {
    TakeStep([](NavigationStack &stack, const NavigationStack::Entry &current) {
        return stack.Back(current);
    });
}

void ActionHelper::JumpForward() {
    TakeStep([](NavigationStack &stack, const NavigationStack::Entry &current) {
        return stack.Forward(current);
    });
}

void ActionHelper::SwitchToNamedView(const std::string &viewName) {
    auto &rvBase = RuntimeConfig::Instance().GetRootView();
    RootView *rootView = static_cast<RootView *>(&rvBase);

    rootView->SetActiveTopViewByName(viewName);
}