//
// AI-5 (docs/ui-refactor.md §8): the toolkit (src/Core/UI) persists layout through this interface
// instead of including the app's Core/Session/* headers directly. The app (SessionManager/Editor)
// supplies a concrete sink backed by its own LayoutSession DTO (see Core/Session/LayoutSessionSink.h).
//

#ifndef GEDIT_ILAYOUTSINK_H
#define GEDIT_ILAYOUTSINK_H

#include <string>

namespace gedit {

    // "Stable id -> splitter pos" + "focused top-view", nothing else - the toolkit knows no more about
    // layout persistence than this. Splitter position is carried both absolute and relative-to-content
    // (the splitter computes both from its own rect; the sink just stores/returns them) so a restore
    // onto a different window size can prefer the ratio.
    class ILayoutSink {
    public:
        virtual ~ILayoutSink() = default;

        virtual void PutSplitter(const std::string &id, int absolutePos, float relativePos) = 0;
        virtual bool GetSplitter(const std::string &id, int &absolutePos, float &relativePos) const = 0;

        virtual void PutFocusedTopView(const std::string &name) = 0;
        virtual bool GetFocusedTopView(std::string &name) const = 0;
    };

}

#endif //GEDIT_ILAYOUTSINK_H
