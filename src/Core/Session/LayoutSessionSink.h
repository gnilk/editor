//
// AI-5 (docs/ui-refactor.md §8): adapts the toolkit's ILayoutSink to the app's LayoutSession DTO
// (docs/session-cache.md §4.1). This is the app-side half of the seam - it's fine for this header to
// depend on Core/Session/SessionState.h; only src/Core/UI/* must stay free of it.
//

#ifndef GEDIT_LAYOUTSESSIONSINK_H
#define GEDIT_LAYOUTSESSIONSINK_H

#include "Core/UI/ILayoutSink.h"
#include "Core/Session/SessionState.h"

namespace gedit {

    class LayoutSessionSink : public ILayoutSink {
    public:
        explicit LayoutSessionSink(LayoutSession &layout) : layout(layout) {
        }

        void PutSplitter(const std::string &id, int absolutePos, float relativePos) override {
            SplitterSession s;
            s.id = id;
            s.absolute = absolutePos;
            s.relative = relativePos;
            layout.splitters.push_back(s);
        }
        bool GetSplitter(const std::string &id, int &absolutePos, float &relativePos) const override {
            for (const auto &s : layout.splitters) {
                if (s.id == id) {
                    absolutePos = s.absolute;
                    relativePos = s.relative;
                    return true;
                }
            }
            return false;
        }

        void PutFocusedTopView(const std::string &name) override {
            layout.focusedTopView = name;
        }
        bool GetFocusedTopView(std::string &name) const override {
            if (layout.focusedTopView.empty()) {
                return false;
            }
            name = layout.focusedTopView;
            return true;
        }

    private:
        LayoutSession &layout;
    };

}

#endif //GEDIT_LAYOUTSESSIONSINK_H
