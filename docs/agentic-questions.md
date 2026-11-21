# Questions about code - what to answer before what to build

Status: **exploring** (2026-11-21), on `explore/agentic-questions`. Started from an audit of `agentic-features.md`: its
features (graphs, type-diff, diff views) came first and the questions they answer were left implicit. The audit's line: what
you will actually want is "why is this line here?" three months later. So this doc turns it around: the questions first,
where each answer lives, a cheap probe for each, and a feature only where a probe shows it is worth one.
`agentic-features.md` stays as the first ideas.

**The test for a probe:** run it on a real change, read the plain-text output, and ask: *did this tell me something I didn't
know, or would have missed?* Yes - worth building a view. Noise - we learned that from a script, not from a drawn graph.

**Tools outside the editor are fine** (`agentic-features.md` Status): the editor writes X, a tool makes Z, the editor shows Z.
Probes are Python in `scripts/explore/`, every intermediate format readable - by a person and by an agent - so each step can
be checked on its own.

---

## 1. The questions

Each with where the answer has to come from. Sources: **code** (the index: symbols, edges), **runtime** (what actually ran),
**history** (git), **record** (commit messages, `#N`, docs, the agent's conversation), **tests**.

### Why - intent

| Question | Source | Notes |
|---|---|---|
| Why is this line here? | history + record | blame -> commit -> `#N` -> design doc section -> the conversation. The audit's question. |
| Why was this class hierarchy extended this way? When did this type get its base? | history + code | the commit that added the `inherits` edge, then as above |
| This block is almost identical to that one - copied, or grown alike? Should they be one? | code + history | near-duplicate lines (the line hashes exist), then blame both: same commit, or one copied from the other |
| Why is this file in this folder, when it clearly belongs somewhere else? | code + history | where its includes / holds point, per folder; when it moved, or why it never did |
| Was this decided, or did it just happen? | record | is there a `#N` / doc section behind the commit, or only the code? |

### How - flow

| Question | Source | Notes |
|---|---|---|
| How does this function get invoked? (`EditorView::ReInitView`) | runtime, code | **async, timer and event paths matter most** - static calls stop at a callback, a virtual call or a posted message (code-index §8.1) |
| How does a keystroke end up in the line? | runtime | a path through layers: SDL / gansi -> `KeyPress` -> `Runloop` -> keymap -> action -> controller -> `Document` -> `Line` |
| How does a character get from the line to the screen? | runtime | `Line` -> attributes -> `EditorView` -> `DrawContext` -> backend |
| Which thread runs this, and what guards it? | runtime + code | the pty reader / parse thread / git thread / main; `screenLock`, `linesLock` |
| Who posts this, and when does it run? | runtime | for an async call the stack ends at the runloop - the poster is the answer |

### What - structure

| Question | Source | Notes |
|---|---|---|
| Does this already exist? (before writing) | code | code-index §10.3: the agent's question |
| What depends on this - what breaks if I change it? | code (+ tests) | `holds` / `inherits` reversed now; references with code-index step 6 |
| Where are the extension points? | code | an interface with several implementations that something holds |
| Does the code still follow the rules we wrote down? | code | CLAUDE.md's layering rules as checks |

### Confidence - tests

| Question | Source | Notes |
|---|---|---|
| Is this function covered by a unit test? Which one? | tests (runtime) | coverage of a `libutests` run; "no test names it" as the cheap first cut |
| How much of this class is covered? | tests (runtime) | per function, per class |
| Which tests should I run after changing this? | tests + code | the reverse of coverage |
| What did this change add that no test touches? | tests + history | the change's new functions against coverage |

### A change - review

| Question | Source | Notes |
|---|---|---|
| What did this change do to the structure? | code at two revisions | the type-diff - types, edges, folder includes |
| Did it add something that already existed? | code | the new names against the index before it |
| Did it leave something dead behind? | code + tests | removed callers, unreferenced functions |
| Is the why written down? | record | the commit, the `#N`, the doc |

More will come; add them here, with a source.

## 2. Agent aware

The editor is language aware; should it be agent aware? A first answer from the probes (§3):

- **The record is written at change time, and it does not need an agent to read.** In this repo it is already rich: 665
  commits co-authored by Claude, most with a long message that names the `#N`, which leads to a design doc section. The lookup
  chain (`why.py`) is plain git + grep.
- **The conversation is the one part only the agent's side has.** The `Claude-Session` link in a commit is coarse: one
  session spans weeks (77 commits share this session's link, 119 another's), so it points at a whole conversation, not the
  moment. But the local transcripts (`~/.claude/projects/<project>/*.jsonl`) contain the commit hashes, so the turn where a
  commit was made can be found - on the machine that made it. That is a real "why" source: the discussion before the
  decision.
- **So: decision handling does not depend on talking to an agent.** The agent makes the record cheap to write (it already
  writes the commit messages and docs) and cheap to read when it is prose - "here is the chain for this line, what was the
  reason?" sent to the agent's tab (promote to terminal exists). Being agent aware means knowing where an agent leaves its
  traces (commit footers, transcripts, docs) and reading them, not requiring one to be running.
- To think about: whether commits should carry a finer pointer than the session (a transcript id + the turn), and whether
  transcripts belong to the project's record at all (they are private, local, and large - 150 MB here).

## 3. Probes

### 3.0 The test case: the Panel refactor (#52 / #53)

**Finding before any probe: a feature merged in steps is not `merge-base..HEAD`.** #53 went in as four merges (`af535056`
steps 1-2, `763da101` step 3, `5121ab1a` step 4, `4969b78e` step 5) among some twenty unrelated ones. The range between the
first and last would mostly measure other work. So the probes take a feature as **its merges**, each merge's own change
(`M^1..M`) - `scripts/explore/feature_files.py`. 29 C/C++ files. Still leaks: a file the feature touched also carries what
other merges did to it in the same span (`Editor::FontStyleFromLanguageToken` shows up).

### 3.1 Type-diff - `typediff.py` (2026-11-21)

`gedit-bench --dump` of the feature's files at both ends, compared by qualified name. Plain text, ~220 lines, architecture
first:

```
types +8 -0, functions +109 -1, members +50 -0
## inherits / holds added (15)          e.g. PanelView inherits TabbedView, TabbedView::Tab holds(owns) ViewBase,
                                             PanelView::PanelTab holds(borrows) ViewBase, TerminalController holds(owns) ConfigNode
## folder includes added (13)           e.g. src/Core/Editor/Views -> Core/UI/Modals/ConfirmModal.h
```

**Verdict: yes, worth it** - the top 35 lines are the architecture of the change, readable in a minute:
- **A rule broken, unnoticed:** `PanelView.cpp` includes `Core/UI/Modals/ConfirmModal.h`; CLAUDE.md says `RootView` is the
  only file under `Views/` that may include `Modals/`. Came in with step 3 (`6450fa67`, the "a job runs - close it?"
  question). Not caught in review.
- **A question to ask:** `TabbedView::Tab` *owns* each tab's view, and `PanelView::PanelTab` keeps a *borrowed* pointer to
  the same views - two lists of the same tabs.
- The 109 functions below are noise for review; the edges and includes are the signal. A view should lead with them.

### 3.2 No test names it - `tests_mention.py` (2026-11-21)

Reads `typediff.py`'s text (the readable intermediate in practice), counts each new name in `utests/`.

```
 8/23  gedit::PanelView        ActiveTerminal, AddMessages, AddPanelTab, AddTerminal, CloseTab, NewTerminal, ...,
                               RestoreTabs, SaveTabs, SelectTabNumber, ShowMessages, Terminals
 5/14  gedit::TerminalController   ArmBuildQuietTimer, ForgetSession, HasRunningJob, OnBuildQuiet, PostBuildSummary, ...
```

**Verdict: yes, as a pointer.** `PanelView`'s tab persistence (`SaveTabs` / `RestoreTabs`) and the close-tab job question
(`CloseTab`, `HasRunningJob`) are named by no unit test - covered by the scripted app checks (`terminal_tabs`) and by hand,
which this probe can't see. Limits: a common name (`Refresh`, `OnAction`) counts as mentioned when it isn't; a mention is
not coverage. Real coverage (§4) answers it properly.

### 3.3 Why is this line here - `why.py` (2026-11-21)

blame -> commit message -> the merge -> the `#N` -> the docs that name them -> the session. On three Panel lines (the
`ConfirmModal` include, `PanelTab::view`, `HasRunningJob`) each came back with the commit's reasoning (several lines of it),
the merge, `#53` / `#52` and `panel-tabs.md` / `build-messages.md` - **the why was there every time**, two commands away.

**Verdict: yes** - cheap and already rich, because the commits were written to carry it. Noise: names in the line are
looked up in docs too, and common ones (`Core`, `ViewBase`) list half the docs; keep only rare names. The session link is
coarse (§2).

## 4. Next probes

| Question | Probe |
|---|---|
| How does this get invoked (async too)? | lldb on the editor (the ANSI backend in a pty, no window), a breakpoint that logs the stack and continues; use it a while; fold the stacks into a caller tree. For posted work, the posting stack: a debug-only record in `Runloop`. Try on `EditorView::ReInitView` |
| Keystroke -> line, line -> screen | the same, with breakpoints along the path; output as one path with the layers named |
| Covered by a test? How much? | clang source-based coverage (`-fprofile-instr-generate -fcoverage-mapping`) on a `libutests` run in its own build dir; `llvm-cov report` per function, folded per class. Against 3.2's list |
| Almost identical - why? | near-duplicate runs of line hashes across the tree, then `why.py` on both |
| In the wrong folder? | per file: share of its includes / holds that point into its own folder; the outliers |
| The conversation behind a commit | find the commit hash in the local transcripts, print the turns before it |

When a probe has earned it, it gets a hitlist row - and `agentic-features.md`'s views get revisited against what the probes
showed.
