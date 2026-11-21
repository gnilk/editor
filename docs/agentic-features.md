# Agentic features

This aims to ouline some features which I believe would help when working with AI Agents.
Features here are brief and should be treated as ideas for further discussion before planning and later implementation.

## Graphs
Ability to render an image/graph of dependencies through
- Composition
- Inheritance

The idea is to understand where a dependency is flexible or not.

Example:
a) Call through an interface
b) Call through a concrete class

Call through an interface may look like:

                         consumer A
                       /
publisher -> iconsumer - consumer B
                       \ 
                         consumer C
                        
This will tell us that iconsumer is an extension point.
Ability to show the extension interface of iconsumer should be possible (like a |+| next to it) and a fold-out below when clicked..

Note: Extension points can hide within other types, like a vector or other containers

The idea is to give an architectural diff.,

Examples are often found in disassemblers and similar tools.
Example: assets/example_callgraph.png


## Type-diff
Every larger change, should carry a type-diff..
- Types (Classes/structures/other) added and removed
- Functions (member, global, local) added and removed
- Type dependency changes
- Inheritance changes
- Call-graph changes

This can be visualized in many ways...

The main purpose is to find architectural changes and to sign them off (or not).

Thus, there need to a before and after.
For the editor project - the newly added 'Panel' (#52/#53) which was a refactor of the terminal view handling to a more generic version

It is important to recognize a graph-diff. Like when an extension point is added or removed..


With a type-diff several views are possible
- New/Removed types (X replaced Y)
- Which classes have been touched (affected by the diff)
    Ex: TextBuffer ->  'Line::Ref -> ListOfChars::Ref' -> expand to all see all points this changed applies to
- etc..    
    

## 'Regular'-Diff
Ability to see smaller diff's with better clarity.

The left/right paradigm is probably ok.
But I'd like a specific block singled out, +/- a couple of lines. Clearly, marked.. 
For small changes this would would to include a couple of lines before and a couple of lines after..

Or, as some have it - side by side - with a clear indication where lines have been inserted and not...
If original is left and changed right - the left would have gaps where right added code..  
Sometime with graphical elements showcasing it.

Examples: assets/example_diff_bubble.png


---

## Status: exploring (2026-11-21)

These features are being explored, not planned. What follows is a first look at what they stand on; the shape will change
as we try things.

**Separate tools for the grunt work are welcome.** A pipeline of the form *the editor writes X - a tool picks it up and
produces Z - the editor renders Z in a view of its own* gives freedom to iterate fast: the tool can start as Python (quick
turnaround), and move into C++ (a library + wrapper, compiled in) only once its shape has settled. The editor's part stays
small - export the data, read the result, draw it.

- **Every intermediate format is readable**, so each step can be inspected, verified and tested on its own - the editor's
  output without the tool, the tool without the editor, the view from a hand-written file. That is what makes the
  publisher / consumer loop quick to develop.
- **Human readable first; AI readable counts too** where it gives an edge - a format an agent can read, write and diff is
  one it can help build and check. Plain text, JSON or YAML, one fact per line where possible, stable ordering so two runs
  diff cleanly.
- **What it does to the order** (hitlist, 2026-11-21): a readable export of the index first (#74), the tools in Python over
  it, the image view as the first graph view (Graphviz PNG), native views last.
- Candidates: the graph generator (model in, layout out - Decisions 1), the type-diff (structure of two revisions in, the
  diff out). Graphviz as an external `dot -Tplain` fits the same pattern.

## Baseline - what it takes (pre-planning, 2026-11-21)

Read against `hitlist.md`, `image-view.md` and `code-index.md`. Not a plan yet - what each feature needs, what exists, and
the order that gets there. Hitlist rows #65-#73 hold the new pieces.

### What exists already

- **The data for the graphs:** the code index has `inherits` and `holds` edges, with ownership (owns / shares / borrows;
  a container of a type counts), over the whole tree and the open files, by **name** - chosen in code-index §10.3 so that two
  states of a tree can be compared. Hierarchy and composition trees exist (`RelationshipTree`, step 3).
- **Symbols with extents** (`beginLine` / `endLine`), so a changed line maps to the type / function it is in.
- **Scanning a text that isn't open:** `Scan(text)` - so the base side of a type-diff scans what `git show` gives, for the
  changed files only. No stored index needed (code-index step 5 is not on the path).
- **Git:** `GitBase` runs `git show HEAD:./<name>` (async) and the change marks hash the base's lines (`Document::SetChangeBase`).
  Only HEAD, only per open file, and the marks say *that* a line changed, not how the two sides line up.
- **Pixels:** `PixelImage` + `DrawContext::DrawImage(cellRect, image)` (the minimap). One texture per draw context, cell
  rects only, nearest scaling. Everything else is a character grid.
- **View modes:** `DocumentViewMode { kText, kHex }`, the container picks between two views with a ternary.

### What each feature needs

**Regular diff** (side by side, gaps where the other side added, a hunk with a few lines around, joining shapes):
- A line diff that aligns the two sides. Not there - the change marks only compare hashes. Own diff (Myers / histogram over
  the line hashes we already have) rather than parsing `git diff`: the right side is the *buffer*, unsaved edits included,
  as the change marks are. #67.
- The base text at any revision, not just HEAD (`GitBase` generalised), and a read-only TextBuffer made from a text (no file).
- A view **mode** of a document - `Diff` next to Text / Hex / Image. That needs the container's mode -> view map
  (image-view §2.5), split out of #13 as #65.
- Drawing: two panes kept in step by a `DiffProjection` (view row -> line or gap per side, like `WrapProjection` /
  `HexProjection`); the joining shapes between them are filled polygons in pixels (#66); the ANSI backend gets coloured
  bands instead. #68.

**Type-diff** (types / functions / edges added and removed, touched types, sign-off):
- The structure diff as **data**: the changed files' symbols and edges at the base and now, compared by qualified name. All the
  parts exist; the diff and the "which files changed between A and B" do not. #69.
- **The base:** HEAD for small changes; for "a larger change" (the Panel refactor, #52/#53) the branch point,
  `git merge-base main HEAD` - the change is a branch, not a commit.
- **Modules** (code-index step 4, #33) for "a new dependency between modules" - the most architectural line of a type-diff.
  The step was paused for design; it is now on the path.
- **Reachable from outside** - a dot-command and `gedit-bench --typediff <rev>`, so an agent can run it on its own change
  before handing it over (code-index §10.3's "ask before you write").
- A view: the diff as a tree (character grid, rows are jump targets, a changed type expands to its holders / derived via the
  step 3 queries). #70. A drawn graph diff comes after the graph view (#73).
- **"Expand to all points this change applies to"** in full needs references (code-index step 6, calls / uses / refs). The
  baseline answers it with `holds` / `inherits` in reverse; step 6 is later.
- **Sign-off** (approve an architectural change): where it is recorded is open; not baseline.

**Graphs** (composition / inheritance, extension points, `|+|` fold-out):
- **Extension points** are cheap from what exists: a type with two or more derived types, held by someone (directly or in a
  container) - `holds` + `inherits` reversed. `publisher -> iconsumer -> consumer A/B/C` is exactly that; no call edges
  needed. The scanner doesn't record pure virtuals (`= 0`); worth adding as a flag. #71.
- **Call through an interface vs a concrete class** needs `calls` edges (code-index step 6, the noisiest edge, §8). Later -
  the composition graph shows the same flexibility from the declarations.
- **A drawn graph**: a layered layout (pure, testable headless), nodes as boxes with text, edges as lines, zoom / pan,
  click a node to jump, `|+|` to fold out an interface's members. This revisits code-index D11 ("export first, a drawn view
  only if trees aren't enough") - the feature asks for the drawn view. Needs #66's primitives plus text at a pixel position,
  and the image viewport maths (#13 phase 2) for zoom / pan. #72.

### The shared architecture

Three things every feature leans on - this is where image-view overlaps, as guessed:

1. **The container's mode -> view map** (image-view §2.5, #65). Image, Diff and possibly Graph are modes; Text and Hex move
   onto the map.
2. **Pixel drawing in `DrawContext`** (#66): image-view §3 (`DrawImagePixels` with a source crop, a texture per image, the
   scale mode) plus filled rect / line / polygon and text at a pixel position. Both SDL backends; each view has a character
   fallback for ANSI.
3. **Buffers that aren't the file on disk.** Image-view's `BufferBase` (#7) is about a file of another kind. Diff needs a
   read-only TextBuffer from a revision's text, and a graph is not a file at all. #7 is needed for images; for diff and graph
   it is the question of where a non-file thing lives (below).

### Not on the path

From the hitlist: #56 (but useful - fixed window sizes for screenshots of the new views), #60, #63, #64, #12, #14, #6, #25-#27,
#8, #28, #30, #31, #34, #36, #17 / #62 / #61 (#61 is agent-adjacent - ready-made agent tabs - but not baseline). Image view
phase 3. From code-index: step 5 (persistence - only changed files are scanned twice; revisit if a whole-tree graph is slow
to build at start), step 7 (rules as data). Step 6 (calls / refs) only for the call-graph parts.

### Decisions (2026-11-21)

1. **A graph is a generated document in the editor area** - a document with no file, shown by its own view (the mode -> view
   map, #65), so it gets tabs, `Cmd+E`, close and the session as any open file does. Open to a better suggestion.
   - **The generator is a library with a small wrapper binary**, like `gedit-bench`: graph model in (nodes, edges, kinds),
     layout out (positions, edge routes). Compiled into the editor, and runnable on its own for tests and for an agent.
   - **Layout:** look at what exists before writing one. Graphviz is the obvious candidate - as an external binary
     (`dot -Tplain` / `-Tjson` gives node positions and edge splines; we draw them ourselves, nothing linked) when it is
     installed. A layered layout of our own (Sugiyama: layers, crossing reduction, positions) is a few hundred lines and can
     snap to the character grid, which the ANSI version needs (3). Likely both: our own for the grid and small graphs,
     Graphviz optional for big ones. Licences to check before anything is vendored.
2. **Two diff sources, one view.** Our own line diff for the live buffer (unsaved edits, the change marks' hashes); `git diff`
   parsed for history (two revisions, a branch). Both produce the same model - hunks and aligned rows - and the Diff view
   only reads that model.
3. **The drawn graph comes first** (this doc supersedes code-index D11's "export first"). The ANSI version - boxes, straight
   lines, scrolling both ways - is wanted but second priority; a grid-snapped layout keeps the door open.
4. **The base of a diff - deferred, with a leaning per use:**
   - **Type-diff:** the branch against the point where it left `main` (`merge-base`) - reviewing an architectural change
     in progress, all its commits together.
   - **Regular diff:** HEAD against a chosen point X, with X movable step by step towards HEAD (commit by commit) to see
     how a section evolved. So the view takes any two revisions, not a fixed base; picking and stepping X is UI to design
     later. Which default gives most value is decided when the views are in use.
