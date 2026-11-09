//
// LineDiff - which lines of a text differ from a base, for the change marks in the gutter (docs/usability-round.md
// §0c). Plain: lines in as hashes (Line::HashOf), marks out; knows nothing of git or buffers.
//
// Myers' O(ND) diff - cheap when the differences are few, the common case - after the common start and end are
// taken off. Past 'maxEdits' differences it stops looking for the shortest script and marks the whole middle as
// changed: right about where the changes are, rough about which line is which.
//
#ifndef GEDIT_LINEDIFF_H
#define GEDIT_LINEDIFF_H

#include <cstdint>
#include <memory>
#include <vector>

namespace gedit {

    class LineDiff {
    public:
        // Per line of the current text; a line can carry a removal mark next to its own
        enum kMark : uint8_t {
            kNone = 0,
            kAdded = 1,
            kModified = 2,
            kRemovedAbove = 4,      // base lines were removed just above this line
            kRemovedBelow = 8,      // ... just below it - only on the last line, when the end was removed
        };

        struct Changes {
            using Ref = std::shared_ptr<const Changes>;
            std::vector<uint8_t> marks;     // one per current line

            bool IsEmpty() const;
            uint8_t MarkOf(size_t idxLine) const {
                return (idxLine < marks.size()) ? marks[idxLine] : (uint8_t)kNone;
            }
        };

        static constexpr size_t kDefaultMaxEdits = 2000;

    public:
        static Changes Compute(const std::vector<uint64_t> &base, const std::vector<uint64_t> &current,
                               size_t maxEdits = kDefaultMaxEdits);
        // Every line added: a file the base doesn't have
        static Changes AllAdded(size_t numLines);
    };

}

#endif //GEDIT_LINEDIFF_H
