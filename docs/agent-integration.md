# Agent integration

Status: **design, exploring** (2026-11-21), on `explore/agentic-questions`. Grew out of
[`agentic-questions.md`](agentic-questions.md): once the questions were listed, the next question was where their answers'
sources live, and how the editor and an agent agree on that.

**Built so far (2026-11-24, branch `feature/agent-integration`, on top of #77 - for review):** the seam only - nothing is
started or sent yet.
- `AgentIntegration` (`src/Core/Agent/`): what the editor needs from an agent - its name and status, whether its command
  is found, the working agent's and the question agent's command lines (§6.1, read-only), where it keeps this project's
  transcripts (§5.2: session ids), a question put as a prompt that names its answer file (§4, PTY channel). Plain data -
  nothing in it runs a process.
- `NoAgent` - **`none`, the default**: every answer is "nothing". The editor as it was (Unaware).
- `ClaudeAgent` - `claude`: `claude` / `claude --permission-mode plan` (plan mode reads, never edits - whether the answer
  file can be its one write is still §8), transcripts in `$CLAUDE_CONFIG_DIR` or `~/.claude` + `projects/<the root, every
  non-alphanumeric as '-'>`.
- **`main.ai_integration: none | claude`** picks it, user config only - `ProjectConfigPolicy` strips it from a project's
  config (a folder never switches an agent on; this is *which* agent - *whether* a project is Integrated stays the
  profile's, `project-profile.md`). An unknown name: a start-up notice, `none`. `Editor::GetAgent()` is never null.
- `.agent` prints it: which, found or not, what would be started for this project, the transcript folder.
- Tests: `test_agent`; app check `agent_integration`.

**Step 1 - the Editor Agent's tab (2026-11-24, branch `feature/editor-agent-tab`, on the seam - for review):** `.agent
start` (typing it is the consent) opens it in the bottom panel - §6.2 items 2-5:
- **Locked name** "Editor Agent" - the agent's terminal title doesn't replace it (`PanelView::AddEditorAgent`, a tab kind
  `editor_agent` that neither `panel.tabs` nor a session can name).
- **Marker:** its label is bold in the theme's `tab_editor_agent` (default theme: pink; the VS Code import: its magenta;
  a theme without it: a violet of the panel's own) - `TabbedView::SetTabMark`, generic.
- **Banner:** a line of its own under the tab row, outside the terminal (`TabbedView::SetTabInfoLine`, in the mark's
  colour) - whose agent it is, what it runs, that closing the tab stops it. First written into the terminal, but the
  agent goes full screen and cleared it (user, 11-24).
- **Lifecycle:** not saved in the session (so not brought back); `.panel move` and the layout switch leave it at the
  bottom; Cmd/Ctrl+W asks ("closing its tab stops it"); a second `.agent start` shows the running one. It is never the
  output console (`output_console: no` in its settings) nor the promote target, and the bottom panel doesn't count it
  as its terminal.
- Its settings: the user's `terminal` section copied, the bootstrap replaced by the agent's command
  (`AgentIntegrations::EditorAgentSettings` - plan mode for Claude). Nothing is sent to it yet.

**Next (decided 2026-11-24, §6.2):** no `view: agent` tab that follows `ai_integration` - the **User Agent** stays a
plain terminal tab the user configures (`config: ai_agent`, its bootstrap runs `claude`), and the editor knows nothing
about it. The **Editor Agent** - the question agent of §6.1 - is the editor's own tab, started on the first question the
user consents to and never from `panel.tabs`, so a project can't start it. First build: its fixed name, its marker, its
banner, its lifecycle (§6.2 items 2-5); then the prompt / answer-file round trip (§6). **Step 2** (the `.ask` round trip) is designed in §6.3, for review. Codex: a second implementation once
its read-only flags are checked. Whether the code and config names follow the new terms (the key, `WorkingAgentCommand`)
is the user's to decide after reading the code - not yet.

The short version: **tools gather, the agent judges.** The editor runs cheap, repeatable tools for the facts (blame chain,
type-diff, coverage, call stacks), puts them in a prompt, sends it to an agent with the user's consent, and shows the answer.
A hard question - "was this decided?", "should these two be one?" - goes to the agent instead of to a purpose-built tool.

---

## 1. Principles

- **Agent calls cost money - consent comes first.** The user's agent runs on the user's account. During project setup
  every send is shown and accepted before it goes. After setup, the user's settings decide (§3.3).
- **Nothing is hidden.** Every prompt the editor sends lands in an agent tab, also when it was sent automatically, so what
  was asked - and what it cost - is visible where the user would look.
- **The agent guesses, the user approves, the editor stores.** The editor does not infer where a project keeps its bugs
  or plans; it asks the agent to survey the project, and the user approves the result.
- **The editor does not write into the user's setup unasked.** The project's own files and git are the truth; the editor's
  derived data lives in its own cache, outside the repo (§5). Each place the editor does leave a trace (an MCP
  registration, a line in `AGENTS.md`) is a step in setup, shown and accepted.

## 2. Modes

Per project, like VS Code's "Trust this folder" - but more is behind it.

- **Unaware** - the editor as it is today: files and git. No transcripts, no shadow repo, no channel to an agent.
- **Integrated** - the user has gone through setup (§3): the project has a manifest (where its truth lives), settings
  for what may run without asking, and **the editor runs its own agent in its own tab** - the question agent (§6) - over
  the PTY channel, with the IDE channel connecting by itself (§4).

**Focus (2026-11-21): these two.** The variants in between - an agent outside the editor reached over `/ide`, headless
calls, an MCP server registered by hand - are noted in §4 but not designed for now. Where the user runs their *own*
working agent (an editor tab, Ghostty) is their business either way.

## 3. Setup - a guided process

A few modals, once per project. Every agent call in it is prepared by the editor, shown, accepted, then sent (the editor
presses Enter).

1. **Turn on Integrated** for this project - what it means, what it may cost.
2. **Where the meta lives** - a few questions pick it (§5.3): working alone or with others? on several machines? is the
   repo public?
3. **The survey.** The agent is asked where the project keeps its known problems, plan, design docs, conventions, how `#N`
   references resolve. It writes a **proposed manifest** to a file.
4. **Review.** The user sees the manifest, edits, approves. From then on it is the contract.
5. **Tell the agent.** `AGENTS.md` / `CLAUDE.md` gets the pointers (open-bugs is `docs/open-bugs.md`, ...) - again a prompt,
   accepted. Or the MCP server's instructions carry it (§4) and nothing goes into the project; open which.
6. **Channels** - which of §4 to allow; the editor offers the commands (e.g. the MCP registration), the user runs or
   accepts them.
7. **Settings for after** (§3.3).

### 3.3 After setup

The user's settings decide, per kind of question - from *ask every time* to *auto*, and `agent_mode: full_auto` (name
open) for questions that run in the background. Revocable at any time. Changing the manifest or `AGENTS.md` later follows
the same settings.

## 4. Channels

| Channel | Carries | Agent outside (own terminal) | Agent inside (editor tab) |
|---|---|---|---|
| **IDE** | selection, @-mentions, diffs to accept / reject, diagnostics | yes, after `/ide` in the agent | yes, connects by itself (`CLAUDE_CODE_SSE_PORT` in the tab's env) |
| **MCP server** (the editor's) | the manifest, the index, unsaved buffers, build messages, "where is X" | yes, registered once | yes, registered once |
| **PTY** | prompts the editor writes into the agent's input line | no | yes |
| **Headless** (`claude -p`) | one job, structured output, no conversation | yes | yes |

- **PTY** works with any agent (Claude Code, Codex, Aider - anything with a prompt), no protocol, and the prompt sits in
  the transcript like any other. The answer comes back as **a file**, not as screen text: the prompt names the file, the
  editor watches it and shows it (or its diff). Before sending, the editor must know the agent is idle and the user isn't
  mid-prompt - the agent's terminal state (title, progress OSC) via gvt; per agent.
- **Headless** is the agent as a command; agent-specific, and each call is a cost - consent as everything else.

### 4.1 The IDE channel (Claude Code)

No public spec; read from the 2.1.329 binary - internal, may change. A neovim plugin implementing it would be a reference.

- The editor writes `~/.claude/ide/<port>.lock` (or under `$CLAUDE_CONFIG_DIR`): JSON
  `{pid, workspaceFolders, ideName, transport: "ws", authToken}`. The port is the file name.
- Claude Code picks a lock whose `workspaceFolders` contain its cwd - or the one whose port is `CLAUDE_CODE_SSE_PORT`.
  Only inside a VS Code / JetBrains terminal does it require the IDE to be its ancestor process; from another terminal any
  match qualifies.
- It connects automatically only with `autoConnectIde`, `CLAUDE_CODE_SSE_PORT`, or inside a supported IDE terminal;
  otherwise the user types `/ide` - the agent-side consent.
- MCP over `ws://127.0.0.1:<port>`, header `X-Claude-Code-Ide-Authorization: <authToken>`. On connect: `ide_connected
  {pid}`. Claude Code calls `openDiff` (-> `FILE_SAVED` / `DIFF_REJECTED` / `TAB_CLOSED`), `close_tab`,
  `closeAllDiffTabs`, `getDiagnostics`; the IDE notifies `selection_changed`, `at_mentioned`.
- **The catch:** of the IDE server's tools only `mcp__ide__getDiagnostics` and `mcp__ide__executeCode` reach the model. So
  the IDE channel is plumbing between the two programs; what the agent should be able to *ask* (manifest, index) needs the
  MCP server.

## 5. Where the truth lives

### 5.1 Three layers

1. **Truth** - the user's and the agent's: the working tree, git, the docs, the known-problems list, the tracker, commit
   messages, the agent's transcripts. The editor reads it; writes only through setup or the user's settings.
2. **Derived** - the editor's: index, coverage, stacks, type-diffs, snapshots. In `<root>/.goatedit/cache/`, keyed by
   commit or content hash, kept out of git by `.git/info/exclude`. Delete it and nothing is lost.
3. **Meta** - the manifest: where this project keeps what. Proposed by the agent, approved by the user (§3). Where it is
   stored is chosen at setup (§5.3).

A project is contained: all of it lives in the project folder - the layout, and how trust is approved, are in
[`project-profile.md`](project-profile.md).

### 5.2 The shadow repo - history between commits

An agent works in turns; git sees commits. At each turn boundary the editor snapshots the work tree into its own repo:
`git --git-dir=<root>/.goatedit/shadow.git --work-tree=<root> add -A && commit`. A real git history of the
agent's turns, the user's `.git` untouched. Type-diff and "what did it change that I didn't ask for" then work per turn,
before anything is committed. Turn boundaries from the channel (or the transcript); without an agent, when the files
settle.

**Each snapshot says whose turn it was** (review feedback, 2026-11-21). More than one thing changes the tree: the user's
agent (maybe two), the user in the editor, and - in setup - the question agent. So every shadow commit carries trailers:
`Session: <id>` + `Agent: claude-code`, or `Source: editor-save` for the user's own edits. "What did the agent I work
with change" is then a filter on the shadow log.

- **The session id without requiring hooks:** for the user's agent, the transcript just written (its file is named by the
  session); for the question agent, the editor started it and knows it (passed in where the agent accepts one, else read
  from the transcript it creates). Hooks (the Stop hook's `session_id`) are the precise source once the user opts in.
- **The editor's own output never shows up as a change:** answer files go under `.goatedit/`, and the shadow repo's own
  exclude (`shadow.git/info/exclude`) leaves `.goatedit/` out. Trailers tell agents apart; the exclude keeps the editor out.

### 5.3 Where the meta lives - open (the user is thinking about it)

Not everyone wants their todo list and known bugs in a public repo.

| Answers at setup | Meta |
|---|---|
| Alone, one machine | in `.goatedit/`, not committed |
| Alone, several machines, public repo | a **sidecar repo** - a separate private git repo for the meta |
| With others | in the repo, committed |

## 6. Questions through the agent

- **Tools gather:** free, repeatable, deterministic - the probes (`why.py`, `typediff.py`, tcov). A question asked fifty
  times should not cost fifty agent calls to collect facts.
- **The agent judges:** was this decided, should these be one, does the explanation match the code, what was tried and
  rejected (a transcript), where does X happen when the index can't say.
- A question is then: run the tools, fill a prompt with their output, send it (consent per §3.3), the answer written to a
  file the editor shows. The probes' plain-text output is both the person's reading and the agent's input.
- **A question agent, in its own tab.** Questions don't go into the conversation the user is working in - they would eat
  its context and derail it. A separate agent tab per project takes them; its transcript is the record of what was asked.
  Under `full_auto` it runs in the background, still a tab (for inspection); otherwise the tab is where a prompt is
  confirmed and the answer inspected.

### 6.1 The question agent is its own instance

The editor starts it, in its tab, as **a separate agent process and conversation** from the one the user works with -
whether that one runs in another editor tab or in a terminal outside.

- **Separate:** the conversation and its context; the questions never show up in the user's conversation, nor the user's
  work in the question agent's.
- **Shared:** the account (its usage is the user's - consent, §1), the working tree, `AGENTS.md` / `CLAUDE.md`, the agent's
  config. With Claude Code also its per-project auto-memory directory and transcript folder (different session files) -
  two instances can write the same memory; to watch.
- **It reads, it doesn't edit.** Two agents changing one working tree is a race; the question agent is started read-only
  (permission mode / allowed tools), writing only its answer file and - in setup, accepted - the manifest and `AGENTS.md`.
- Being two processes, with their own session ids, is what lets the editor tell the turns apart - each shadow snapshot is
  tagged with the session that ended the turn (§5.2), and the question agent's output sits in `.goatedit/`, outside the
  snapshots.

### 6.2 Editor Agent and User Agent - telling them apart

Terms (2026-11-24, for the design - the code and config names are not changed yet):
- **User Agent** - the one the user works with. A plain terminal tab with a custom command: `panel_right.tabs` with
  `config: ai_agent`, whose bootstrap runs `claude`. The editor is unaware of it - it is a shell tab like any other.
- **Editor Agent** - the question agent (§6.1): started and driven by the editor, through the `AgentIntegration` adapter
  picked in the user config. Templated prompts (the questions in `agentic-questions.md`, filled with the tools' output),
  answers into files under `.goatedit/`.

Both are visible - full transparency is the default (§1) - so they must never be mistaken for each other:

1. **Placement by owner.** The bottom panel holds the editor's things (Messages, build output) and the right panel the
   user's work: by default the Editor Agent's tab goes to the bottom, the User Agent to the right. In portrait both are at
   the bottom, so placement alone is not enough - hence 2 and 3.
2. **A locked name.** The tab is always "Editor Agent" - not the agent's own (changing) terminal title, not renamable.
3. **A visual marker.** Its tab is drawn with its own theme key (a colour / mark), so the tab row tells the two apart at
   a glance. 2 and 3 both - one alone is too easy to miss.
4. **A banner in the tab.** Before the agent starts, the editor writes one line into it: who started it and why - the
   Editor Agent, started by GoatEdit to answer questions, read-only, answers in `.goatedit/...`. Read once, nod, get to
   work.
5. **Its own lifecycle.** Never in `panel.tabs`, never restored from the session like a configured tab: the editor creates
   it on the first question the user consents to. Closing it stops it. It doesn't move to the right panel (`.panel move`
   leaves it), so it never turns into "another Claude tab".
6. **A status-line indicator** - `EA: answering...` / `EA: idle` - what it is doing without opening it, and one place to
   click to jump there.
7. **Typing in it is allowed**, but visibly: typing while the editor has a prompt waiting gives a short notice (the PTY
   channel needs the input line - §4). Not settled; start like this and see where it ends up.

Build order: 2, 3, 4 and 5 with the first Editor Agent tab; then 1, 6 and 7.

### 6.3 Step 2 - one question, all the way round (`.ask`)

Status: **design, for the user's review** (2026-11-24, branch `feature/editor-agent-ask`, on step 1). One free-text
question, from the command line to an answer on screen, through the Editor Agent's tab. No templates, no tools' output in
the prompt yet (step 3) - this is the round trip they will ride on. The user agreed to the design on 2026-11-24 and took
decisions 1 and 2; one point (**[user]**) is still open.

**The flow:**

1. **`.ask <question>`** - free text. (Later the templates call the same path with a filled prompt.)
2. **The question is written down, then the prompt built** - the editor writes the question file `q_<id>.md` (below),
   then `AgentIntegration::PromptWithAnswerFile(question, answerFile)` (exists) gives the question followed by "write
   your answer as Markdown to `a_<id>.md`, change no other file".
3. **Consent** - a modal shows *exactly* what will be typed, which agent gets it, and whether the agent must be started
   first: **Send** / **Cancel**. Every question asks, in step 2. "Always for this kind" needs kinds - step 3 with the
   templates, and the settings of §3.3. **Decided (user):** ask every time to begin with - it's easier to follow what
   happens, and to stop and reflect.
4. **Start if needed** - no Editor Agent running: the consent covered starting it; it starts as in step 1 (`.agent start`),
   and the question waits for it to be idle (below).
5. **Send** - when the agent is idle and the user isn't typing in its tab: the prompt goes in as one bracketed paste
   (`ESC[200~ ... ESC[201~`, the agent asked for it with `?2004`), then Enter (CR) a moment later. As a paste, the prompt's
   line breaks stay in the prompt instead of sending it early. The user sees it arrive in the tab like typed text (§1:
   nothing hidden).
6. **Wait** - the status line says so (below). The tab is not brought to front unless the user clicks there.
7. **The answer** - the answer file appears and the agent is idle again: the editor opens the file in an editor tab
   (Markdown is highlighted already) - an ordinary file from then on. A notice if the agent went idle without writing it ("no answer file - see the Editor Agent tab").

**Where it goes - the question is kept with its answer (decided, user):** `<root>/.goatedit/agent_qa/`, one pair per
question with a shared id `<yyyymmdd-hhmmss>-<n>`:
- `q_<id>.md` - written by the editor before sending: the question as Markdown, with a short header (front matter: time,
  agent, session id, answer file) and the prompt exactly as sent.
- `a_<id>.md` - written by the agent: the answer.

Plain Markdown pairs, so a good question can later be turned into a template or promoted into something like our own
"skills". The agent's transcript is the third record. Not `cache/` - an answer can't be rebuilt. Nothing in `.goatedit/`
is the project's truth (§5.1), and the shadow repo leaves it out (§5.2). Keeping it out of the user's `git status` (a
line in `.git/info/exclude`) is a write into their setup - not in step 2; noted for setup (§3).

**The agent starts read-only, writing only its answer files** (§6.1, closes the §8 item for Claude): plan mode can't write the
answer file, so the question agent becomes, for Claude Code (2.1.331, from `claude --help`, not yet tried for real):

    claude --permission-mode dontAsk --allowedTools "Read Grep Glob Edit(.goatedit/agent_qa/a_*)" --session-id <uuid>

`dontAsk` denies anything not allowed up front - no permission prompts the user must answer in the tab, no Bash, no edits
but `a_*` files in `agent_qa/` - it can't change a question. (`Edit(...)` rules cover Write in Claude Code's rule syntax - to confirm.) `--session-id` is the editor
choosing the id, so the question file and the shadow trailers (§5.2) know it without reading transcripts.
`AgentIntegration::QuestionAgentCommand` gains the `agent_qa` folder and the session id. **This needs one real start to
confirm** - a paid call, so only when the user says so; the checks run on the fake `claude`.

**Idle - when may the editor type?** Per agent (`AgentIntegration`), from what the terminal shows, never from the screen
text:
- **Generic (any agent):** the agent has asked for bracketed paste (`?2004` - its input line is reading) **and** no output
  for a quiet time (1.5 s, setting). Cheap, works for the fake.
- **Claude Code:** its terminal title changes while it works (a spinner glyph first) and settles when done - a sharper
  signal, *if* it holds; to read off a real run before relying on it. Until then the generic rule.
- **The user is typing in it:** a key in the Editor Agent's tab within the last 3 s, or the input line not empty as far as
  the editor can tell (it can't, for a full-screen agent) - so: the last-key rule, and a waiting question shows a notice
  in the tab's info line ("a question is waiting - it's sent when you stop typing") - §6.2 item 7, first version.
- **[user], open:** the user asked why there is a timeout - `.ask` is typed at the normal terminal's prompt and goes
  when Enter is pressed. Neither time is about that: the quiet time is the *agent* finishing (its start-up, the previous
  question) before the editor may type into its tab; the typing window only matters if the user types in the Editor
  Agent's own tab. Proposed instead of the typing window: don't send while the Editor Agent's tab has the keyboard - the
  info line says a question waits, it goes when focus leaves (no timer). The quiet time stays until the agent gives a
  clear "done" signal.

**One question at a time.** A second `.ask` while one is out: queued, the status line counts them. No parallel questions
in one agent - its conversation is sequential anyway.

**Status line (§6.2 item 6), first version:** a short field on the right while the Editor Agent exists - `EA: starting`,
`EA: idle`, `EA: waiting to send`, `EA: answering`, `EA: answered` (until the answer is opened), `EA: stopped`. In the
Editor Agent's mark colour. A click jumps to its tab - with the mouse bindings (`mouse:`), a binding like any other.

**What gets built (sketch, names open):**
- `AgentQuestion` - one question: id, question file, answer file, prompt, state (`Consent` → `Starting` → `WaitingToSend` →
  `Sent` → `Answered` / `NoAnswer` / `Cancelled`). Plain data, unit-tested as a state machine fed by events (idle,
  file seen, tab closed) - no pty in the tests.
- The question queue + driver in the editor: owns the queue, polls the answer file and the idle signal on the runloop's
  timer, writes into the Editor Agent's pty through `TerminalController` (a `SendAsPaste` that wraps by `?2004`).
- `TerminalController`: when the last output came, whether `?2004` is on, whether its tab has the keyboard - read under
  `screenLock`.
- `.ask` in `agent.js`; `docs/keys.md` regenerated.
- Tests: the state machine; the paste wrapping; `ClaudeAgent`'s new command line. App check `editor_agent_ask`: a fake
  `claude` that turns on `?2004`, reads a line, writes the file it names, and the check sees the answer opened and the
  status line go `answering` → `answered`.

**Not in step 2:** templates and the tools' output in the prompt (step 3); "always" consent and `full_auto` (§3.3); the
headless channel; answers shown anywhere but an editor tab; `.git/info/exclude`.

## 7. Code-care and output-care

Some projects are about their code (the editor, testrunner, an Amiga intro), some about their output (a golf app) - the
code matters less than what it produces. That changes which questions are asked, not where the truth lives; output-care
adds one that `agentic-questions.md` lacks: *did the output change, and was that what I asked for?* (runtime - golden
files, screenshots, responses, diffed per turn over the shadow snapshots). The more the manifest says about a project's
structure and access, the more questions can be answered.

## 8. Open

- Where the meta lives (§5.3).
- `AGENTS.md` vs the MCP server's instructions for telling the agent where the truth lives (§3 step 5).
- The settings' names and granularity (`agent_mode`, per kind of question).
- Idle detection for the PTY channel, per agent - generic rule and Claude's title in §6.3, the title to be confirmed.
- Is the lock-file format documented anywhere stable; can a project-scoped MCP server be one the editor runs (not one the
  agent launches).
- How the question agent is started read-only, per agent - Claude Code: `dontAsk` + allowed tools (§6.3), to confirm in
  one real start.
- An "answered by" column (tool / agent / both) in `agentic-questions.md` §1.
