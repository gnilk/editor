//
// Created by gnilk on 16.05.23.
//

#include <fstream>

#include "ActionHelper.h"
#include "Core/Editor.h"
#include "Core/UI/Views/RootView.h"
#include "Core/Editor/Modals/FindModal.h"
#include "Core/Editor/Modals/RelationshipModal.h"
#include "Core/Editor/Modals/OpenFilesModal.h"
#include "Core/Editor/Modals/SymbolSearchModal.h"
#include "Core/Editor/Modals/QuickOpenModal.h"
#include "Core/PathUtil.h"
#include "Core/Session/SessionManager.h"

using namespace gedit;

// The active document's path as the index knows it. Empty if it has no place on disk.
std::string ActionHelper::ActivePath() {
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
    return NavigationStack::MakeEntry(editor.GetCodeIndex().get(), ActionHelper::ActivePath(), (uint32_t)lineCursor.idxActiveLine, (uint32_t)lineCursor.cursor.position.x);
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
    auto modal = SymbolSearchModal::Create(editor.GetCodeIndex(), document, ActionHelper::ActivePath(), editor.GetIndexedRoots());
    auto symbolModal = modal.get();
    auto rootView = RuntimeConfig::Instance().GetRootViewAs<RootView>();
    rootView->ShowModal(std::move(modal));
    // After ShowModal - the list height comes from the laid out modal
    symbolModal->SetQuery(FindModal::SelectedText(document));
}

// Every file of the project roots - the list of the last walk at once, the new walk's when it is in
void ActionHelper::ShowQuickOpen() {
    if (!RuntimeConfig::Instance().HasRootView()) {
        return;
    }
    Editor::Instance().RefreshFileFinder();
    auto modal = QuickOpenModal::Create();
    auto quickOpenModal = modal.get();
    auto rootView = RuntimeConfig::Instance().GetRootViewAs<RootView>();
    rootView->ShowModal(std::move(modal));
    // After ShowModal - the list height comes from the laid out modal
    quickOpenModal->SetQuery(U"");
}

bool ActionHelper::OpenFile(const std::string &path) {
    auto &editor = Editor::Instance();
    auto from = CurrentPlace();
    auto document = editor.LoadDocument(path);
    if (document == nullptr) {
        return false;
    }
    editor.SetActiveDocument(document);
    if (RuntimeConfig::Instance().HasRootView()) {
        SwitchToNamedView(glbEditorView);
    }
    auto &lineCursor = document->GetLineCursor();
    editor.GetNavigationStack().Jump(from, NavigationStack::MakeEntry(editor.GetCodeIndex().get(), path,
                                                                      (uint32_t)lineCursor.idxActiveLine,
                                                                      (uint32_t)lineCursor.cursor.position.x));
    SessionManager::Instance().NotifyChanged();
    return true;
}

// The open files, the active one selected - docs/usability-round.md §1
void ActionHelper::ShowOpenFiles() {
    if (!RuntimeConfig::Instance().HasRootView() || Editor::Instance().GetWorkspace()->GetOpenDocuments().empty()) {
        return;
    }
    auto modal = OpenFilesModal::Create();
    auto openFilesModal = modal.get();
    auto rootView = RuntimeConfig::Instance().GetRootViewAs<RootView>();
    rootView->ShowModal(std::move(modal));
    // After ShowModal - the list height comes from the laid out modal
    openFilesModal->SetQuery(U"");
}

// The type under the cursor, or the one the cursor is in
void ActionHelper::ShowRelationships() {
    auto &editor = Editor::Instance();
    auto document = editor.GetActiveDocument();
    if ((document == nullptr) || (editor.GetCodeIndex() == nullptr) || !RuntimeConfig::Instance().HasRootView()) {
        return;
    }
    auto &lineCursor = document->GetLineCursor();
    auto line = document->GetTextBuffer()->LineAt(lineCursor.idxActiveLine);
    if (line == nullptr) {
        return;
    }
    auto path = ActionHelper::ActivePath();
    line->Lock();
    auto text = line->Buffer();
    line->Release();

    auto idxLine = (uint32_t)lineCursor.idxActiveLine;
    auto &index = *editor.GetCodeIndex();
    auto atCursor = GoToResolver::Resolve(index, text, (size_t)lineCursor.cursor.position.x, idxLine, path, GoToResolver::kMode::kDefinition);
    auto types = RelationshipTree::TypesAt(index, atCursor.name, path, idxLine);
    if (types.empty()) {
        editor.ShowStatusMessage(U"No type here");
        return;
    }
    auto modal = RelationshipModal::Create(editor.GetCodeIndex(), types, editor.GetIndexedRoots(), ActionHelper::SourceLine);
    RuntimeConfig::Instance().GetRootViewAs<RootView>()->ShowModal(std::move(modal));
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
    auto path = ActionHelper::ActivePath();
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
    } else if (result.hits.empty() && !result.libraryType.empty()) {
        editor.ShowStatusMessage(U"'" + result.name + U"' is a member of " + result.libraryType);
    } else if (result.hits.empty()) {
        bool isLibrary = IsLibraryName(*line, result.idxName);
        editor.ShowStatusMessage(U"'" + result.name + (isLibrary ? U"' is from a library" : U"' isn't in the index"));
    } else {
        GoToHits(std::move(result.hits), document, path);
    }
}

void ActionHelper::GoToRelatedAtCursor(GoToResolver::kRelated related) {
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
    auto path = ActionHelper::ActivePath();
    line->Lock();
    auto text = line->Buffer();
    line->Release();

    auto result = GoToResolver::ResolveRelated(*editor.GetCodeIndex(), text, (size_t)lineCursor.cursor.position.x, (uint32_t)lineCursor.idxActiveLine, path, related);
    if (result.hits.empty()) {
        bool isBase = related == GoToResolver::kRelated::kBase;
        editor.ShowStatusMessage(result.name.empty() ? U"Nothing here to go from" : U"'" + result.name + (isBase ? U"' has no base in the index" : U"' has no implementation in the index"));
        return;
    }
    GoToHits(std::move(result.hits), document, path);
}

// docs/code-index.md §11.11
void ActionHelper::GoToTypeAtCursor() {
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
    auto path = ActionHelper::ActivePath();
    line->Lock();
    auto text = line->Buffer();
    line->Release();

    auto result = GoToResolver::ResolveTypeOf(*editor.GetCodeIndex(), text, (size_t)lineCursor.cursor.position.x, (uint32_t)lineCursor.idxActiveLine, path);
    if (result.name.empty()) {
        return;
    }
    if (result.hits.empty()) {
        editor.ShowStatusMessage(result.libraryType.empty() ? U"No type in the index for '" + result.name + U"'" : U"'" + result.name + U"' is a " + result.libraryType);
        return;
    }
    GoToHits(std::move(result.hits), document, path);
}

// One place jumps, several are a list
void ActionHelper::GoToHits(std::vector<CodeIndex::Hit> hits, Document::Ref document, const std::string &path) {
    if (hits.size() == 1) {
        JumpTo(hits[0].path, hits[0].line, hits[0].column);
    } else if (RuntimeConfig::Instance().HasRootView()) {
        auto modal = SymbolSearchModal::CreateForHits(std::move(hits), document, path, Editor::Instance().GetIndexedRoots());
        auto symbolModal = modal.get();
        RuntimeConfig::Instance().GetRootViewAs<RootView>()->ShowModal(std::move(modal));
        // After ShowModal - the list height comes from the laid out modal
        symbolModal->SetQuery(U"");
    }
}

Line::Ref ActionHelper::SourceLine(const std::string &path, uint32_t line) {
    auto &editor = Editor::Instance();
    auto canonicalPath = pathutil::CanonicalPath(path);
    auto workspace = editor.GetWorkspace();
    for (auto &document : workspace->GetOpenDocuments()) {
        auto node = workspace->GetNodeFromDocument(document);
        if ((node == nullptr) || (pathutil::CanonicalPath(node->GetNodePath()) != canonicalPath)) {
            continue;
        }
        auto buffer = document->GetTextBuffer();
        if (buffer->IsEmpty()) {
            break;      // not read in yet
        }
        auto textLine = buffer->LineAt(line);
        if (textLine == nullptr) {
            return nullptr;
        }
        textLine->Lock();
        auto copy = textLine->CopyOfRange(0, textLine->Length());
        textLine->Release();
        return copy;
    }
    std::ifstream file(path);
    std::string text;
    for (uint32_t idxLine = 0; idxLine <= line; idxLine++) {
        if (!std::getline(file, text)) {
            return nullptr;
        }
    }
    UnicodeHelper::ReplaceInvalidUtf8(text);
    auto index = editor.GetCodeIndex();
    return ColouredLine(UnicodeHelper::utf8to32(text), std::filesystem::path(path).extension().string(), (index != nullptr) ? index->GetWorkspaceNames() : nullptr);
}

static bool IsNameChar(char32_t ch) {
    return ((ch >= U'a') && (ch <= U'z')) || ((ch >= U'A') && (ch <= U'Z')) || ((ch >= U'0') && (ch <= U'9')) || (ch == U'_');
}

Line::Ref ActionHelper::ColouredLine(const std::u32string &text, const std::string &extension, const WorkspaceNames::Ref &workspaceNames) {
    auto line = Line::Create(text);
    auto language = Editor::Instance().GetLanguageForExtension(extension);
    if (language == nullptr) {
        return line;
    }
    auto tokenizer = language->Tokenizer();     // a copy: the parser keeps its state in it
    tokenizer.ParseLineFromState("main", line);
    if (workspaceNames == nullptr) {
        return line;
    }
    for (auto &attrib : line->Attributes()) {
        if (attrib.tokenClass != kLanguageTokenClass::kRegular) {
            continue;
        }
        auto idxName = (size_t)attrib.idxOrigString;
        auto idxEnd = idxName;
        while ((idxEnd < text.size()) && IsNameChar(text[idxEnd])) {
            idxEnd++;
        }
        if (idxEnd == idxName) {
            continue;
        }
        auto tokenClass = WorkspaceNames::ClassFromSyntax(text, idxName, idxEnd);
        if (tokenClass == kLanguageTokenClass::kRegular) {
            tokenClass = workspaceNames->LookupAt(text, idxName, text.substr(idxName, idxEnd - idxName));
        }
        if (tokenClass != kLanguageTokenClass::kRegular) {
            auto [fgColor, bgColor] = Editor::Instance().ColorFromLanguageToken(tokenClass);
            attrib.tokenClass = tokenClass;
            attrib.foregroundColor = fgColor;
            attrib.backgroundColor = bgColor;
        }
    }
    return line;
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