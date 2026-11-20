//
// The last build's errors and warnings as a list (docs/build-messages.md §2, hitlist #52) - Cmd/Ctrl+F8. Errors
// first, then warnings, each in the order the build printed them, each with the code it points at in its colours on
// the line under it; typing filters (file name or message). Enter goes
// to the place - a jump, Cmd/Ctrl+[ comes back - and F8 goes on from there. With a Messages tab in the lower panel
// (MessagesView) Cmd/Ctrl+F8 shows that instead.
//

#ifndef GOATEDIT_BUILDMESSAGESMODAL_H
#define GOATEDIT_BUILDMESSAGESMODAL_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Core/Build/BuildMessage.h"
#include "Core/UI/Modals/QueryListModal.h"

namespace gedit {
    class BuildMessagesModal : public QueryListModal {
    public:
        using Ptr = std::unique_ptr<BuildMessagesModal>;

    public:
        BuildMessagesModal(const std::string &newCaption, std::vector<BuildMessage> newMessages);
        virtual ~BuildMessagesModal() = default;

        // The last run's messages, as they are now; nullptr when there was no run
        static Ptr Create();

        // The messages to list, as indices, in BuildMessages::Order (errors first - as F8 steps). A query keeps those
        // whose row text holds it (any case).
        static std::vector<size_t> Filter(const std::vector<BuildMessage> &messages, const std::u32string &query);
        // 'error    a.cpp:3:16  use of undeclared identifier'
        static std::u32string RowText(const BuildMessage &message);

    protected:
        std::vector<std::u32string> OnQuery(const std::u32string &query) override;
        std::u32string OnRowDetail(int idxRow) override;
        void OnCommit(int idxRow) override;
        int LinesPerRow() const override {
            return 2;
        }
        void DrawRowLine(DrawContext &dc, int x, int y, int width, int idxRow, int idxLine) override;

    private:
        std::vector<BuildMessage> messages;
        std::vector<size_t> rowMessages;    // the message behind each row of the last query
        // The code of the messages drawn so far (nullptr: none), by index into 'messages' - read once, a copy
        std::map<size_t, Line::Ref> codeLines;
    };
}

#endif //GOATEDIT_BUILDMESSAGESMODAL_H
