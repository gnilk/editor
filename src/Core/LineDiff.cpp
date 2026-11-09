//
// LineDiff - see LineDiff.h
//
#include <algorithm>

#include "LineDiff.h"

using namespace gedit;

namespace {
    enum class kOp : uint8_t {
        kEqual,
        kDelete,        // a base line
        kInsert,        // a current line
    };

    // The shortest edit script from a to b, Myers' greedy forward search. False when it is longer than maxEdits.
    // 'trace' keeps V for each d, only its [-d, d] window: O(D^2) memory, not O(D * (N + M)).
    bool ShortestScript(const uint64_t *a, int n, const uint64_t *b, int m, size_t maxEdits, std::vector<kOp> &outOps) {
        int max = n + m;
        int dLimit = (int)std::min<size_t>((size_t)max, maxEdits);
        std::vector<int> v(2 * (size_t)max + 3, 0);
        const int offset = max + 1;
        std::vector<std::vector<int>> trace;

        int dFound = -1;
        for (int d = 0; (d <= dLimit) && (dFound < 0); d++) {
            trace.emplace_back(v.begin() + (offset - d), v.begin() + (offset + d + 1));
            for (int k = -d; k <= d; k += 2) {
                int x;
                if ((k == -d) || ((k != d) && (v[offset + k - 1] < v[offset + k + 1]))) {
                    x = v[offset + k + 1];
                } else {
                    x = v[offset + k - 1] + 1;
                }
                int y = x - k;
                while ((x < n) && (y < m) && (a[x] == b[y])) {
                    x++;
                    y++;
                }
                v[offset + k] = x;
                if ((x >= n) && (y >= m)) {
                    dFound = d;
                    break;
                }
            }
        }
        if (dFound < 0) {
            return false;
        }

        // Back from the end, through the V each step started from
        std::vector<kOp> reversed;
        int x = n;
        int y = m;
        for (int d = dFound; d >= 0; d--) {
            auto &vd = trace[d];
            auto at = [&vd, d](int k) {
                return vd[k + d];
            };
            int k = x - y;
            int kPrev;
            if ((k == -d) || ((k != d) && (at(k - 1) < at(k + 1)))) {
                kPrev = k + 1;
            } else {
                kPrev = k - 1;
            }
            // d == 0: the diagonal from the start
            int xPrev = (d > 0) ? at(kPrev) : 0;
            int yPrev = (d > 0) ? (xPrev - kPrev) : 0;
            while ((x > xPrev) && (y > yPrev)) {
                reversed.push_back(kOp::kEqual);
                x--;
                y--;
            }
            if (d > 0) {
                reversed.push_back((x == xPrev) ? kOp::kInsert : kOp::kDelete);
            }
            x = xPrev;
            y = yPrev;
        }
        outOps.assign(reversed.rbegin(), reversed.rend());
        return true;
    }

    // A run of removed and added lines between equal ones, at current line 'idxCurrent': as many changed as there
    // are on both sides, the rest added; more removed than added leaves a removal mark after it
    void MarkHunk(std::vector<uint8_t> &marks, size_t idxCurrent, size_t numRemoved, size_t numAdded) {
        size_t numModified = std::min(numRemoved, numAdded);
        for (size_t i = 0; i < numAdded; i++) {
            marks[idxCurrent + i] |= (i < numModified) ? LineDiff::kModified : LineDiff::kAdded;
        }
        if (numRemoved <= numAdded) {
            return;
        }
        size_t idxAfter = idxCurrent + numAdded;
        if (idxAfter < marks.size()) {
            marks[idxAfter] |= LineDiff::kRemovedAbove;
        } else if (!marks.empty()) {
            marks.back() |= LineDiff::kRemovedBelow;
        }
    }
}

bool LineDiff::Changes::IsEmpty() const {
    return std::all_of(marks.begin(), marks.end(), [](uint8_t mark) {
        return mark == kNone;
    });
}

LineDiff::Changes LineDiff::Compute(const std::vector<uint64_t> &base, const std::vector<uint64_t> &current, size_t maxEdits) {
    Changes changes;
    changes.marks.assign(current.size(), kNone);

    // The same start and end take most of a file with a few changes
    size_t numStart = 0;
    while ((numStart < base.size()) && (numStart < current.size()) && (base[numStart] == current[numStart])) {
        numStart++;
    }
    size_t numEnd = 0;
    while ((numEnd < base.size() - numStart) && (numEnd < current.size() - numStart) &&
           (base[base.size() - 1 - numEnd] == current[current.size() - 1 - numEnd])) {
        numEnd++;
    }
    int n = (int)(base.size() - numStart - numEnd);
    int m = (int)(current.size() - numStart - numEnd);
    if ((n == 0) && (m == 0)) {
        return changes;
    }

    std::vector<kOp> ops;
    if (!ShortestScript(base.data() + numStart, n, current.data() + numStart, m, maxEdits, ops)) {
        MarkHunk(changes.marks, numStart, (size_t)n, (size_t)m);
        return changes;
    }

    size_t idxCurrent = numStart;
    size_t numRemoved = 0;
    size_t numAdded = 0;
    auto flush = [&]() {
        if ((numRemoved > 0) || (numAdded > 0)) {
            MarkHunk(changes.marks, idxCurrent - numAdded, numRemoved, numAdded);
        }
        numRemoved = 0;
        numAdded = 0;
    };
    for (auto op : ops) {
        if (op == kOp::kEqual) {
            flush();
            idxCurrent++;
        } else if (op == kOp::kDelete) {
            numRemoved++;
        } else {
            numAdded++;
            idxCurrent++;
        }
    }
    flush();
    return changes;
}

LineDiff::Changes LineDiff::AllAdded(size_t numLines) {
    Changes changes;
    changes.marks.assign(numLines, kAdded);
    return changes;
}
