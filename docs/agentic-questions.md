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

Grouped by **when you ask** - that decides what an answer has to show - not by kind. Rewritten 2026-11-21 from a first
list grouped Why / How / What / Confidence / Review; nothing is prioritised or dropped yet, that comes once the list has
settled.

Columns:
- **Asker**: *me* (the person, reading or reviewing), *agent* (before or while it writes), *both*. An answer for an agent is
  plain text it can act on; only the person's questions may need a view.
- **Source**: **code** (the index: symbols, edges), **runtime** (what actually ran), **history** (git), **record** (commit
  messages, `#N`, docs, `open-bugs.md`, the hitlist, the agent's conversation), **tests**.
- **Probe**: what has been tried (§3), what is planned (§4), or blank.

### Reading cold - "three months later"

| Question | Asker | Source | Probe | Notes |
|---|---|---|---|---|
| Why is this line here? | me | history + record | `why.py` (3.3) - yes | blame -> commit -> `#N` -> design doc section -> the conversation. The audit's question |
| Was this decided, or did it just happen? | me | record | | the sharp form of the one above: is there a `#N` / doc section behind the commit, or only the code? |
| What was tried and rejected? | me | record | | often only in the conversation - the strongest case for transcripts as a source (§2) |
| Is this known to be wrong? | both | record | | `open-bugs.md`, the hitlist. "Why is this line here" should end there when the line is listed |
| Why was this class hierarchy extended this way? When did this type get its base? | me | history + code | | the commit that added the `inherits` edge, then as above |
| What did this used to do? | me | history | | a function's own history across renames and moves, not the file's |
| How does a keystroke end up in the line? / a character get from the line to the screen? | me | runtime | tcov stacks (4.1) | a path through layers: SDL / gansi -> `KeyPress` -> `Runloop` -> keymap -> action -> controller -> `Document` -> `Line`; and `Line` -> attributes -> `EditorView` -> `DrawContext` -> backend. Output as one path with the layers named |

### Before a change

| Question | Asker | Source | Probe | Notes |
|---|---|---|---|---|
| Where does X happen? (the tab row is drawn, the bell sounds) | both | runtime + code | tcov stacks (4.1) | feature location - the most common first question of a task. Trigger the feature, see what ran |
| Does this already exist? | agent | code | | code-index §10.3 |
| Is there a pattern for this? | agent | record + code | | precedent: CLAUDE.md's established patterns, "`NewDocument` is the template". The companion to "does this exist" |
| What depends on this - what breaks if I change it? | both | code (+ tests) | | `holds` / `inherits` reversed now; references with code-index step 6 |
| Which tests should I run after changing this? | both | tests + code | tcov stacks (4.1) | the reverse of coverage - under `trun` the test case is in every stack |
| Where are the extension points? | both | code | | an interface with several implementations that something holds |
| Is this function covered by a test? Which one? How much of this class? | both | tests (runtime) | `tests_mention.py` (3.2) - a pointer; tcov (4.1) | "no test names it" as the cheap first cut, per function / per class from coverage |
| Is this in the wrong folder? | both | code | (4) | share of its includes / holds that point into its own folder; the outliers |
| This block is almost identical to that one - is it a duplicate? | both | code | (4) | near-duplicate runs of line hashes (the hashes exist) |

### After a change - review

| Question | Asker | Source | Probe | Notes |
|---|---|---|---|---|
| What did this change do to the structure? | me | code at two revisions | `typediff.py` (3.1) - yes | types, edges, folder includes; the edges and includes are the signal, the function list is noise |
| What did the agent change that I didn't ask for? | me | record + history | | the prompt (transcript) against the diff |
| Does its explanation match the code? | me | record + code | | the commit message against the type-diff |
| What did it read before deciding? | me | record | | the tool calls in the transcript - what it looked at, and what it didn't |
| Did it add something that already existed? | me | code | | the new names against the index before the change |
| Did it leave something dead behind? | both | code + tests | | removed callers, unreferenced functions |
| What did this change add that no test touches? | both | tests + history | `tests_mention.py` (3.2) | the change's new functions against coverage |
| Does the code still follow the rules we wrote down? | both | code | `typediff.py` caught one (3.1) | CLAUDE.md's layering rules as checks (`Views/` may not include `Modals/`; SDL2 and SDL3 kept in sync) |
| Is the why written down? | both | record | | the commit, the `#N`, the doc |
| Was this verified, and where? | me | record | | Linux / Mac, SDL2 / SDL3, hand-tested or only unit-tested - today only the agent's notes answer it |

### When it's wrong - debugging

| Question | Asker | Source | Probe | Notes |
|---|---|---|---|---|
| How does this function get invoked? (`EditorView::ReInitView`) | both | runtime, code | tcov stacks (4.1) | **async, timer and event paths matter most** - static calls stop at a callback, a virtual call or a posted message (code-index §8.1) |
| Who posts this, and when does it run? | both | runtime | tcov stacks + post breakpoint (4.1) | for an async call the stack ends at the runloop - the poster is the answer |
| Which thread runs this, and what guards it? | both | runtime + code | tcov stacks (4.1) | the pty reader / parse thread / git thread / main; `screenLock`, `linesLock` |
| Who owns this, and when does it die? | both | code + runtime | | `holds(owns / borrows)` edges exist - the type-diff found `PanelTab` borrowing what `TabbedView::Tab` owns. The terminal-resize UAF was this shape |
| Where does this value come from? | both | code + runtime | | data flow, not control flow: a config value from default / user / project / `inherit:`; a colour from the theme or a language token |
| What does this cost? | both | runtime | `gedit-bench` (partly) | keystroke latency on `sqlite3.c`, a recolour, a parse |

### Split from the first list

- "Why is this file in this folder, when it clearly belongs somewhere else?" -> *Is this in the wrong folder?* (before a
  change, code) + *why is this line here* on its include / move (history).
- "Almost identical - copied or grown alike? Should they be one?" -> *is it a duplicate?* (code) + *why* on both blocks
  (history); "should they be one" is the judgement, the person's.

### Where the probes are

The three probes that earned a "yes" (§3) all answer questions the **record** already holds - the commits were written to
carry the why. The questions with no probe yet are the hard ones: **feature location, data flow, ownership**, and the
agent-review questions whose source is the transcript. That is where the next probes go.

More will come; add them here, with an asker and a source.

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
| How does this get invoked (async too)? | **tcov with a stack mode** (§4.1), on `trun` or on the editor itself. Try on `EditorView::ReInitView` |
| Keystroke -> line, line -> screen | the same, with breakpoints along the path; output as one path with the layers named |
| Covered by a test? How much? | **tcov as it is** (§4.1) on `libutests`, per function; against 3.2's list. clang's source-based coverage (`llvm-cov`) as a cross-check |
| Almost identical - why? | near-duplicate runs of line hashes across the tree, then `why.py` on both |
| In the wrong folder? | per file: share of its includes / holds that point into its own folder; the outliers |
| The conversation behind a commit | find the commit hash in the local transcripts, print the turns before it |

### 4.1 tcov - the runtime probe already exists

`tcov` (testrunner repo, `src/coverage` + `src/app/tcov`) runs a target under lldb through the SB API - `trun` (it adds
`--sequential`) or another executable - resolves the symbols asked for (`--symbols gedit::PanelView::*`, globs), puts a
breakpoint on every line of them, and reports which lines were hit (console, LCOV, a diff report). Each breakpoint is
disabled at its first hit (`CheckBreakPointHit`), which keeps it fast. So:

- **"Is this covered, how much?"** is tcov as it stands, on `libutests`, per function - no new tool.
- **"How does this get invoked?"** is one step from it, so it belongs next to coverage: on a hit at a function's *first*
  line, walk the thread's frames (`SBThread::GetFrameAtIndex`: function, file, line) and record the stack, with the thread's
  name, before going on. Keep that breakpoint enabled until N distinct stacks (or N hits) are seen, then disable it, so the
  cost stays bounded. Output: per function, its distinct call stacks with counts, folded into a caller tree - plain text,
  as every intermediate here.
- **One run answers three questions.** Under `trun` the test case's function is in every stack (`test_panelview_...`),
  so the stacks also say **which tests reach this function** - "which tests should I run after changing this" - and the
  thread answers "which thread runs this".
- **Async:** the stack of a posted call ends in the `Runloop`'s dispatch. The poster is a second breakpoint, on the post,
  whose stack is recorded with the message's address; the dispatch's stack is matched to it by that address. That makes
  the editor's `Runloop` (and the timer / pty / parse threads) the place to look first - a known, small set of seams.
- **On the editor itself** (not trun): the target is `goatedit --backend ansi` in a pty, driven by a script as the
  handtests do (`scripts/handtests/goatpty.py`), so "keystroke -> line" is: send a key, collect the stacks at
  `Document`'s insert.

The extension lives in tcov (testrunner repo), not in the editor; the editor reads its text output. To decide with the
testrunner work: a `--stacks` mode with an N, and the stack format.

When a probe has earned it, it gets a hitlist row - and `agentic-features.md`'s views get revisited against what the probes
showed.
