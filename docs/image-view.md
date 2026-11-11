# Image view - quick-view of images in the SDL editor

Status: **plan** (2026-11-11), for review. Nothing built.

Projects carry graphical resources (icons, screenshots, sprites). Opening a `.png` today shows it as text: mangled bytes,
and lossy (open bug #15 - the text loader is not byte-faithful). The goal is to open an image and see it, with a few
keys to zoom and pan. SDL2 and SDL3 only. The ANSI backend shows a summary of the image instead.

Text and Hex are view modes of a document today (`DocumentViewMode`, swapped by `EditorViewContainer`); Image becomes
the third.

---

## 1. What exists

- **Decoding:** `stb_image` is built in (`src/ext/stb`, used for the app icon in `SDL2/SDL3 SDLScreen.cpp`). It reads
  PNG, JPEG, BMP, GIF (first frame), TGA, PSD and HDR. No new dependency.
- **Pixels to the screen:** `PixelImage` (`src/Core/UI/Graphics/PixelImage.h`, RGBA, with a revision) and
  `DrawContext::DrawImage(cellRect, image)` in both SDL backends, which the minimap uses (usability-round §7).
  Limits:
  - the target is a **cell** rect;
  - it keeps **one** cached texture per draw context - the minimap's;
  - it always scales nearest-pixel.
- **View modes:**
  - `DocumentViewMode { kText, kHex }` is per document, kept in the session (`SessionSerializer`: "text"/"hex").
  - `EditorViewContainer` holds a `primaryItem` (EditorView) and an `alternateItem` (HexView) and picks between them
    with a ternary in `ApplyViewMode`. Its own comment already expects N items.
  - `kActionViewModeText` / `kActionViewModeHex` (`Alt+R` / `Alt+H`) switch the mode.
- **Byte source:** HexView reads the **TextBuffer** (`ByteStreamReader::Create(document->GetTextBuffer())`). So hex of a
  binary file shows what the text loader made of it, not the file.

## 2. The model - where does an image live?

Composition, not a Document class hierarchy, and **2.4 C - `BufferBase`** with `TextBuffer` / `ImageBuffer` (both
decided 2026-11-11). 2.6 says where it leads.

### 2.1 The facts

- **`Document` is the text document,** 2,000 lines of it: cursor, selection, undo, syntax, indent and reformat.
  - It uses its buffer's text side in 167 places (`textBuffer`, `Lines()`, `LineAt()`).
  - Everything holds `Document::Ref`: `Editor`'s open list, the workspace node, the session, open files (`Cmd+E`),
    buffer cycling and close.
- **`GetTextBuffer()` is public,** with 43 call sites outside Document.
- **`TextBuffer` is about 90% text:** lines, language, the parse thread, structure, line hashes and workspace names.
  - The rest is generic to any file: the state (`Empty` / `FileRef` / `Loaded` / `Changed`), `Load`, `Save`,
    `SaveForce`, read-only, `CanEdit` and `Close`.
- **"Not read in yet" is already a state every document has.**
  - `Workspace::EnsureDocumentForNode` creates every document with `CreateFileReferenceBuffer()` (an empty
    `kBuffer_FileRef`), and the text is read in later.
  - The callers already cope with it (`ActionHelper.cpp`: `if (buffer->IsEmpty()) break; // not read in yet`).

### 2.2 What an unloaded buffer does at each caller

This is what an image document's TextBuffer would meet if it stays unloaded.

| Caller | With an unloaded buffer |
|---|---|
| Save (`.s`, save all, quit) | `TextBuffer::DoSave` refuses a `kBuffer_FileRef` buffer, so the file on disk is never written. |
| Status markers (`Workspace::StatusMarkers`) | Not changed and not read-only, so no `*`. |
| Read-only on open (`Editor`, `Workspace`) | `SetReadOnly` works on any state. |
| Code index wiring (`Editor::OnDocumentOpened` / `Closed`) | It wires handlers that never fire, since nothing parses. |
| Overview / minimap | No lines, so nothing is drawn. |
| Go to definition, local scan, `OpenAt` | `NumLines() == 0` / a null `LineAt`, both already handled: nothing found. |
| EditController, EditorView | Not reached: in image mode the container's active view is ImageView. |
| JS API (`.fmf`, `.sl`) | `ReindentLineRange` returns early on no lines. |
| HexView | It shows an empty buffer. Today it shows the mangled text instead. |

**The real risk is not null but a text load behind our back.** Text mode on a PNG would run it through the text loader
(garbage, #15), and an edit plus save would write the mangled bytes over the image.

### 2.3 Text mode is refused for image kinds (decided)

- `UIViewModeText` on a document whose kind is image stays where it is and says "binary file - Hex is `Alt+H`".
- It is a property of the kind (the `file_kinds` table, 2.5), not a check in the container: a kind lists the view modes
  it allows, and the container only switches to a mode the active document allows.
- So nothing ever loads an image document's text. The lazy load (2.5) and the refusal together close the
  write-over-the-image path.

### 2.4 How the image hangs off the document - C decided

**A. `std::variant<TextBuffer::Ref, ImageBuffer::Ref>` as Document's private buffer member.**
- `GetTextBuffer()` becomes null for an image, so its 43 callers need to cope with null.
- Document's own 167 text uses each need a `get_if` (or a guard per action).
- Rejected: the type question spreads to everyone who touches the buffer.

**B. Two members: `TextBuffer::Ref textBuffer` (always there, unloaded for an image) plus `ImageBuffer::Ref imageBuffer`.**
- No call site changes, and no new state (2.2).
- The smell: Document carries a buffer that it never fills for an image, next to the one it does fill.

**C. `BufferBase`, with `TextBuffer` and `ImageBuffer` specialising it. Document holds one `BufferBase::Ref`.**
- **What moves to the base** (about 100 lines): the state, `Load` / `Save` / `SaveForce` (virtual), read-only, `CanEdit`,
  `Close`, `IsEmpty` and the path. `ImageBuffer` implements `Load` (read the bytes, decode into a `PixelImage`), and
  `Save` refuses (read-only kind).
- **Document:** one member, `BufferBase::Ref buffer`.
  - `GetBuffer()` serves the generic callers. Read-only on open, status markers, save, save all and reload become
    plain virtual calls, the same for every kind and with no checks.
  - `GetTextBuffer()` is `std::dynamic_pointer_cast<TextBuffer>(buffer)` - null for an image.
  - `GetImageBuffer()` likewise.
- **Document's 167 internal text uses** go through one private typed accessor. That is a mechanical rename, and it is
  safe because the text actions are reached only through EditorView, EditController and the API, and an image never
  gets Text mode (2.3). One guard at the entry of `Document::OnAction` and of the reformat API methods catches anything
  that still slips through.
- **The 43 outside callers:**
  - About 8 move to `GetBuffer()`, because they only need the generic part (`SetReadOnly`, the state, save).
  - About 6 can see an image document and want the text side: the code index wiring, the overview, HexView,
    `GetDocumentFromTextBuffer` and the status language. They get a null check, which is the right meaning there -
    "this service is for text".
  - The rest live in EditController, EditorView and text-only ActionHelper paths that an image never reaches. They
    stay as they are.
- **Cost:** about a day. The base extraction is half a day, and the Document accessor and the caller sweep the other
  half. Then the full green set, and one new test per guarded site.
  - It touches TextBuffer, Document, Workspace, Editor and three views, but changes no behaviour for text files.
  - It can go first, as phase 0, on its own branch, before any image code.
- **What it buys beyond images:** a later binary kind (`OtherDocument` in the earlier sketch, say a raw-bytes buffer
  for byte-faithful hex) is one more specialisation, not another member.

**Decided: C.** It removes the two-member smell, puts the generic file operations behind one polymorphic interface,
and confines the "is this text?" question to the handful of places where it really means "text-only service".

### 2.5 The rest of the model

- **Each view mode's view is its rendering engine.** `ImageView` reads the document's `ImageBuffer` (`PixelImage` for the
  Graphics layer); `HexView` and `EditorView` are unchanged.
- **The container maps mode -> view:** `std::map<DocumentViewMode, ViewBase *>` replaces primary/alternate. So
  `ApplyViewMode` is a lookup, and a mode with no view (Image on ANSI) falls back to a lookup too.
- **The kind of a file comes from a table, not code:**
  - a `file_kinds:` section in `config.yml` maps extensions to a kind, as `languages:` does:
    `image: { extensions: [".png", ".jpg", ...], view: image, allow: [image, hex] }`;
  - `Workspace::EnsureDocumentForNode` creates the buffer that the kind names (with C) and sets the default view mode;
  - a session-stored mode wins over the default, if the kind allows it.
- **The text load is lazy for a non-text default.** A `.png` must not run through the text loader. Phase 1 starts by
  checking where `LoadData` runs on open today.

### 2.6 Document's two roles - where C leads

Looked at on 2026-11-11: putting the abstraction **above** Document, with a variant in the open list or in `Node`.

**What sits above Document:**
- `Workspace` holds the open list (`std::vector<Document::Ref> openDocuments`) and `activeDocument`.
- `Node` holds the document.
- `Editor` passes them through.
- There are 106 `Document::Ref` mentions, 35 `node->GetDocument()` calls and 51 `GetActiveDocument()` calls.

**What a variant above would give:** a null `GetActiveDocument()` for an image is nearly free, because "no document
open" is already a valid state and almost every caller checks for it. But an image still has to **be an open file**:
- `Cmd+E` and the header tabs;
- `Cmd+1..9` cycling;
- `.cb` / `.ca` close;
- session restore;
- reload on change on disk;
- status markers;
- the JS `GetDocuments`.

All of it is written against `Document::Ref`, so a `std::variant<Document::Ref, ImageItem::Ref>` open list needs a
visit (or a second path) in every one. That is more sites than C. Rejected.

**What's really lurking:** `Document` has two roles.
1. **An open file:** path and name, disk stamp, reload and conflict on disk, modified state, view mode, session state,
   a place in the open list.
2. **The text editor:** cursor, selection, undo, the edit actions, reformat - most of its 2,000 lines.

An image needs role 1 and none of role 2. That is why every placement smelled:
- the variant below puts role 2 behind checks;
- two members make role 1 carry an empty text half;
- a variant above duplicates role 1.

**The direction - split the roles by composition:**
- `Document` keeps role 1 and holds a `BufferBase` (C).
- Role 2 becomes a component that the document owns only for text kinds (say `TextEditing`: cursor, selection, undo,
  actions).
- An image document is then a Document with an `ImageBuffer` and no editing component, and everything in role 1 works
  for it unchanged.

**Incremental:**
1. **C** (phase 0, about a day) is the first step: the file operations become polymorphic, and "is this text?" is asked
   at about 6 text-only services.
2. **Moving role 2 out** is the big step, several days: Document's 167 internal text uses, and EditorView, EditController
   and ActionHelper going through `document->GetTextEditing()`. Do it when a second **editable** kind arrives, or when
   Document's size hurts on its own - not for quick-view.

The polymorphism the image feature needs above Document is the view container's mode -> view map (2.5). Each view is
the renderer for its kind of content.

## 3. Drawing - `DrawImage` grows two things

- **A pixel destination with a source crop:** `DrawImagePixels(const PixelImage &, srcRect, dstPixelRect, scaleMode)`
  next to the cell form. Zoom and pan are a src/dst pair, nothing more.
- **A texture per image, not one per context:** a small cache keyed by `PixelImage *` and revision, so the minimap and
  the image view don't evict each other. Trim it when a document closes (or a few entries, LRU).
- **Scale mode:**
  - zoomed in (>= 1:1): nearest, so pixel art stays crisp;
  - zoomed out: linear.
- **A transparent image** is drawn over a checkerboard of two theme colours, so alpha is visible.
- **Both backends together** (SDL2 `SDL_RenderCopyF` / SDL3 `SDL_RenderTexture`), per CLAUDE.md.

## 4. ImageView and its keys

`ImageView` lives in `src/Core/Editor/Views/`, a sibling of HexView.

**Zoom and pan state:**
- the zoom factor (fit, or an explicit 1/8 ... 32);
- the image pixel at the view's centre.

It is per document, so it is kept in the document's view state like the hex scroll. Phase 3 decides whether it goes
into the session.

**Status line:** `PNG 512x384 · 34 KB · 200%`.

**Keymap:** `imageview_keymap.yml` (both OS dirs), inheriting `default_keymap`, like `hexview_keymap.yml`. Zooming and
panning are UI control, so they go on `UINavigationModifier` (`Option` / `Alt`).

| Action | Key (proposal) |
|---|---|
| `ImageZoomIn` / `ImageZoomOut` | `Option+=` / `Option+-` (and `+`/`-` with no modifier - nothing to type into) |
| `ImageZoomFit` | `Option+0` (also `0`) |
| `ImageZoomActual` (1:1) | `Option+1` (also `1`) |
| pan | the arrows; `Shift`+arrow is a bigger step; the mouse wheel; dragging (phase 2) |

Zooming steps in powers of two around the view centre. The wheel zooms around the mouse pointer.

The switch: `UIViewModeImage`, next to `UIViewModeText` and `UIViewModeHex` - `Option+I` (free today; check macOS doesn't take it). On a non-image file it reports
"not an image" and stays where it is.

## 5. The edges

- **Large images:** decode once, keep the full image, and let the texture do the scaling. Above a size cap (config,
  e.g. 8192 px on a side or 256 MB decoded), decode a downscaled copy and say so in the status line.
- **A failed decode** (corrupt file, an unsupported PNG variant): the view shows the reason (`stbi_failure_reason`)
  and offers Hex.
- **Changed on disk:** the folder monitor already flags the node, so ImageView re-decodes when the file stamp differs
  from `ImageBuffer`'s. That's useful while iterating on an asset. No conflict question is needed: nothing in an
  image is edited.
- **Read-only:** no edit actions reach ImageView. `.s` on an image document is a no-op that says so.
- **ANSI (Gansi):**
  - There is no `DrawImagePixels`, so the image mode's view is a summary: format, size, file size, and "Hex:
    `Alt+H`".
  - Later and out of scope: the kitty graphics protocol (Ghostty, kitty, WezTerm) could draw the real image.
- **Not in stb:**
  - SVG would need nanosvg (single header) - a later phase, a table row and a decoder;
  - WebP and ICO aren't read either.

## 6. When a Document hierarchy becomes right

The rejected alternative was a `Document` base with `TextDocument` / `ImageDocument` subclasses. If a second **editable**
non-text kind arrives - with its own undo, save and modified state (an image editor, a hex editor that writes) - then
"an open file" really has two behaviours, and that hierarchy may beat composition. Quick-view is read-only, so a
specialised buffer carries it. `BufferBase` (2.4 C) doesn't block the hierarchy later: it is the part they would
share.

## 7. Phases

**Phase 0 - the buffer model (if 2.4 C).** `BufferBase` extracted, Document holds one buffer, the caller sweep. Text
behaviour is unchanged: the full green set passes, plus a test per guarded site. On its own branch.

**Phase 1 - open and show (SDL2 + SDL3).**
- `ImageBuffer` (the file's bytes + the decoded `PixelImage`).
- The `file_kinds` table: the default view mode and the allowed modes set at open; Text refused for images (2.3).
- The lazy text load for non-text kinds.
- The container's mode -> view map.
- `ImageView` drawing fit-to-view, centred on the checkerboard.
- `DrawImagePixels` and the per-image texture cache.
- The status line.

**Phase 2 - zoom and pan.**
- The keymap and actions.
- The wheel and dragging.
- Nearest vs linear scaling.

**Phase 3 - the rest.**
- Re-decode on change on disk.
- The decode-failure view.
- The size cap.
- The Gansi summary view.
- Session persistence of the mode (`"image"`) and possibly the zoom.

**Later.** SVG (nanosvg), a raw-bytes buffer (`BufferBase` specialisation) so hex is byte-faithful for binaries, and the kitty graphics
protocol on Gansi.

## 8. Tests

Headless, without SDL. The drawing itself is checked in the app.

- **`imageview` module:**
  - `ImageBuffer` decodes a small PNG fixture (in `Assets/testfiles/`: a 4x3 PNG with alpha) to the right size and
    pixels.
  - A corrupt file gives a failure with a reason.
  - The stamp check re-decodes after a rewrite.
- **Zoom/pan maths as a pure function** (`ImageViewport`: image size, view size, zoom and centre in; src and dst rects
  out):
  - fit keeps the aspect ratio and fits inside;
  - 1:1 maps one pixel to one pixel;
  - zoom keeps the centre pixel still;
  - panning clamps at the edges;
  - an image smaller than the view is centred, not stretched, at fit.

  A fit that stretches, or a zoom that drifts the centre, fails them.
- **`document` / `workspace`:**
  - A `.png` opened through the workspace gets image mode, and its TextBuffer is still unloaded.
  - A session-stored "text" wins over the table.
  - Text mode is refused for an image document; the buffer stays unloaded after the attempt.
- **`session`:** "image" round-trips.

## 9. Open for review

1. ~~2.4: C or B?~~ Decided: C (`BufferBase`), with 2.6 as the direction.
2. **Keys:** the `+` / `-` / `0` / `1` with no modifier in ImageView (nothing to type there), or `Option+` only?
3. **The `file_kinds` table:** in `config.yml` next to `languages:`, or its own asset file?
4. **Phase 3's zoom in the session:** worth it, or reopen at fit?
