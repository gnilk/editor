# Agent integration

Status: **design, exploring** (2026-11-21), on `explore/agentic-questions`. Grew out of
[`agentic-questions.md`](agentic-questions.md): once the questions were listed, the next question was where their answers'
sources live, and how the editor and an agent agree on that. Nothing here is built.

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
- Being two processes is also what lets the editor tell the turns apart: the shadow repo's snapshots (§5.2) are the
  user's agent's changes; the question agent makes none.

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
- Idle detection for the PTY channel, per agent.
- Is the lock-file format documented anywhere stable; can a project-scoped MCP server be one the editor runs (not one the
  agent launches).
- How the question agent is started read-only, per agent (Claude Code: permission mode / allowed tools), and whether its
  answer file can be the only write it's allowed.
- An "answered by" column (tool / agent / both) in `agentic-questions.md` §1.
