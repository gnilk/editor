# Project profile

Status: **design, exploring** (2026-11-21), on `explore/agentic-questions`. Nothing is built. Grew out of
[`agent-integration.md`](agent-integration.md): agent mode, channels, a shadow repo, where the meta lives, git allowed or
not, which folders an agent may see - a project configuration is forming, and more options will come.

## 1. A project is contained

Everything the editor knows or keeps about a project lives **in the project folder**, under `.goatedit/`. Archive or
move the folder and everything comes along; nothing about a project is scattered in `~/.config` or `~/.cache`. The one
exception is the trust approval (§3), and its loss is harmless by design.

## 2. Three kinds of setting

They differ in who may write them, and in what they are allowed to do.

| Kind | What | Example | File |
|---|---|---|---|
| **Trust** | what the editor may do in this project | Integrated or not, channels, what runs on auto, git allowed, shadow repo on / off, where the meta lives | `profile.yml` |
| **Description** | where things are in this project | the manifest: known problems, plan, design docs, how `#N` resolves; rules for md files (in / out of the repo); folders an agent may / may not see | `manifest.yml` |
| **Editor settings** | how the editor behaves | fonts, keymaps, terminal, `workspace.exclude` | `config.yml` (as today) |

- **Not in `config.yml`.** `config.yml` is layered (default -> user -> project) and settings in it are preferences; the
  project layer is already filtered for anything that runs commands (`ProjectConfigPolicy`). Trust is not a preference
  to merge - it is a decision the user made, about this folder.
- `workspace.exclude` stays in `config.yml` - it is what the editor never touches. The folders an *agent* may see are a
  different list and belong to the description.

## 3. Trust is approved outside the folder

A folder can arrive with a `profile.yml` in it - a clone, an archive, a colleague's commit. Settings that arrive with the
folder must not grant permissions (agent calls cost money - [`agent-integration.md`](agent-integration.md) §1).

- The editor acts on `profile.yml` only when **its hash is approved**. The approval is the one thing outside the
  folder: `~/.config/goatedit/trusted.yml` - per project: path, hash of `profile.yml`, when approved.
- A profile that isn't approved - new, changed by a pull, restored from a backup - opens the setup modals with it as the
  proposal: "this project proposes these settings". Nothing runs until accepted.
- Changes made through the editor's modals are approved as they are made. A hand edit asks once, on the next load.
- Losing `trusted.yml` costs one confirmation per project - the right behaviour for a folder the editor hasn't seen.

## 4. Layout

```
<root>/.goatedit/
    config.yml       # editor settings (project layer, as today)
    profile.yml      # trust - acted on only when approved (§3)
    manifest.yml     # description - unless the meta lives in a sidecar (agent-integration §5.3)
    shadow.git/      # the agent's turns (agent-integration §5.2)
    cache/           # index, coverage, type-diffs, stacks - derived, disposable
~/.config/goatedit/trusted.yml   # approvals: path + hash of profile.yml
```

- **Out of git without touching the user's files:** `shadow.git/` and `cache/` go into `.git/info/exclude` (git's local
  ignore), never into `.gitignore`.
- **Whether `.goatedit/` is committed is the user's choice:** committed when working with others, ignored for a public
  repo, or the meta moved to a sidecar repo.
- The project id - needed only for a sidecar that follows the project between machines - is the repo's root commit hash
  (the same in every clone); the path for a folder that isn't a git repo.

## 5. Options so far

Collected as they come up; names are placeholders.

| Option | Kind | Notes |
|---|---|---|
| mode: unaware / integrated | trust | `agent-integration.md` §2 |
| channels: ide, mcp, pty, headless | trust | §4 there |
| what runs without asking (per kind of question), `full_auto` | trust | §3.3 there |
| git: allowed or not | trust | today the editor runs `git status` etc. unasked - this would gate it |
| shadow repo: on / off | trust | |
| where the meta lives: here / sidecar | trust | sidecar: path or remote of the private repo |
| sources: known problems, plan, design docs, `#N` | description | proposed by the agent's survey, approved by the user |
| md files: in / out of the repo | description | |
| folders an agent may / may not see | description | |

## 6. Open

- Format of `profile.yml` / `manifest.yml` - plain YAML, readable, the agent writes the manifest.
- Does Unaware need a profile at all, or is "no approved profile" Unaware? (Likely the latter.)
- Git gating: what counts - read-only queries (`status`, `blame`) vs anything that writes.
- How the sidecar is checked out next to the project, and found again on another machine.
