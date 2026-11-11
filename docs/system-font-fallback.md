# System font fallback

**Status:** planned 2026-11-11. Branch `feature/system-font-fallback`.

## Why

`GlyphFont` takes a glyph the theme font lacks from a fixed list of files, `sdl.font_fallback` in `config.yml`. A
character none of those files has is drawn as an empty cell. Claude Code's `/context` grid (`⛀ ⛁ ⛶ ⛝`,
U+26C0/26C1/26F6/26DD) is such a case (open-bugs #26): none of Andale Mono, Menlo, Arial Unicode or Apple Symbols has
them. CLion and Ghostty draw them, because they ask the system for a font that does - on this Mac that is
`/System/Library/Fonts/Supplemental/STIXTwoMath.otf`, the only installed font with outlines for all four. A fixed
list only covers what someone thought to put in it.

## What

After the configured list, ask the operating system for a font that has the character:

1. the theme font
2. `sdl.font_fallback`, in order - unchanged, so a chosen font still wins
3. **the system** - new, `sdl.system_font_fallback: true` (default on; `false` restores the old behaviour)

What the system returns is used like a configured fallback: read once, kept, packed into the same page as the rest.
It is taken only when stb_truetype can draw the glyph - a colour emoji font (bitmaps, no outlines) is skipped and the
cell stays empty, as today.

## How

**A lookup, injected.** Finding a font file for a code point is a graphics concern; it lives beside `GlyphFont` as
`SystemFonts::FindFaceFor(char32_t) -> std::optional<SystemFontFace>` (`src/Core/Graphics/SystemFonts.h`), with a
platform implementation each. `GlyphFont` does not call it directly: it gets a plain `std::function` through
`SetSystemFallback`, so it knows nothing of CoreText or fontconfig and the tests can hand it a fake. The SDL screens
wire it when the config key is on, next to `SetFallbackFiles`.

`SystemFontFace` is the file, plus which face in a collection: an index when the platform knows it (fontconfig), else
a PostScript name (CoreText) that `GlyphFont` matches against the collection's name tables. No match → the first face
with the glyph.

- **macOS - CoreText.** `CTFontCreateForString` with a base font (Menlo) and the one character returns the font the
  system's cascade would use; `kCTFontURLAttribute` is its file, `CTFontCopyPostScriptName` its face. `LastResort` is
  refused - it has an outline for everything, the "no glyph" box.
- **Linux - fontconfig.** A pattern for `monospace` with a charset of the one character, `FcFontSort`, the first
  font whose charset has it: `FC_FILE` and `FC_INDEX`. Optional at build time: without the fontconfig headers the
  lookup finds nothing (a build message says so) and the configured list is all there is.

**Cost.** A page is built once; each code point in it the font and the list lack costs one system lookup, once. Faces
are cached by (file, face), so a font the system returns for many characters (CJK) is read once. A file is read whole
(stb_truetype needs the bytes), the same as a configured fallback.

**Also fixed on the way:** `PackPage` packed every face with font index 0. A face from a collection other than its
first (any `.ttc` the system hands back) needs its own index there.

## Tests

- `glyphfont/systemfallback` - a fake lookup supplies a font for a code point the font and list lack; it is asked
  only for those, the face is opened once for several code points, and nothing is drawn when it returns a face
  without the glyph.
- `glyphfont/systemfallback_order` - a configured fallback that has the glyph wins; the lookup is not asked.
- `glyphfont/systemlookup` - the real lookup: on macOS `⛁` resolves to a file that draws it (not `LastResort`); on
  Linux it runs, and finds something when fontconfig is there.
