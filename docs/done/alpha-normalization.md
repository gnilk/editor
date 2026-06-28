# Alpha normalization — get `ColorRGBA::a` onto a single 0..1 convention

**Status:** ✅ **FIXED 2026-06-28.** Tracked in [`open-bugs.md`](open-bugs.md) (bug 11). **Discovered**
2026-06-27 while fixing the Gansi overlay selection color (open-bugs bug 10): the selection rendered
**green**, traced straight back to `fgColor.A()` returning **224** instead of a 0..1 fraction.

> ## Resolution (what shipped — read this first)
>
> The diagnosis below stands, but the conclusion changed once the **Sublime spec** was checked: `alpha()`
> is a **0..1 fraction** (`color(var(base_green) alpha(0.9))` = "alpha to 90%"), and the theme already
> authored every other alpha 0..1 (`hsla(…, 0.7)`, `hsla(…, 0.25)`). So `alpha(224)` was a **malformed
> value** (a stray 0..255 hand edit), not a convention — the *writer* was right, the *value* was wrong.
> That makes this **Option B's spirit**, not Option A: we did **not** add a `/255` to `ExecuteAlpha`.
>
> **What changed:**
> 1. Theme value corrected: `alpha(224)` → `alpha(0.12)` in `Assets/Resources/colors.json` **and**
>    `Assets/testfiles/colors.json` (a valid 0..1 alpha, consistent with the file's own `hsla` alphas,
>    reproducing the old faint look at a *real* opacity).
> 2. `ExecuteAlpha` is now a faithful 0..1 import boundary: store verbatim **+ clamp to `[0,1]`** exactly
>    as Sublime does, with a `Warn:` on out-of-range (would have caught the original `224`).
> 3. Both Gansi stopgaps removed (`if (a>1) a/=255` and the `1.0 - a` invert); `fgColor.A()` is the blend
>    fraction directly.
> 4. **No SDL / JS / serialization change needed** — alpha ≤ 1 now ⇒ `AlphaAsInt() ≤ 255`, no Uint8 wrap.
>    (Note: the touch-point inventory below lists a `ColorRGBA::ToUint32` — that method does not exist;
>    `operator==`/`Hash()` are the only `AlphaAsInt`-packers and both are effectively unused.)
> 5. Pinned by `test_theme_alpha` (alpha stays 0..1; out-of-range clamps; full `color(... alpha(x))`
>    path) and the updated `test_gansibackend_overlay` (discriminating 0.25 alpha → `{191,191,255}`).

---

## TL;DR

`ColorRGBA::a` is *meant* to be a 0..1 fraction, and is — **everywhere except one place**. The
`alpha()` color-script function stores its raw argument, and the shipped theme calls `alpha(224)`. That
single value (a 0..255 magnitude) flows into the content `selection` color and then into every consumer
that correctly assumes 0..1, where it either overflows or is silently rescued by an accidental integer
wrap. The fix is small to *write* but cross-cutting to *verify* — it touches the color value type, both
SDL backends, the Gansi backend, the JS color API, color serialization, and the theme itself — so it
gets its own branch.

---

## The intended convention is 0..1 (evidence)

`ColorRGBA` (`src/Core/ColorRGBA.h`) stores `float r,g,b,a`, all defaulting to `1.0f`, and every other
part of the codebase treats `a` as a 0..1 fraction:

- `AlphaAsInt(mul = 255)` returns `a * 255` — only meaningful if `a ∈ [0,1]` (the existence of this
  helper is itself the strongest signal of intent).
- `FromRGBA(int r,g,b,a)` divides **all four** channels (incl. alpha) by 255 → 0..1.
- `FromRGBA(float …)` / `FromHSLA(float …)` store alpha verbatim, i.e. expect a 0..1 caller.
- Theme color script: `hsla(210,13%,40%,0.7)`, `hsla(210,40%,50%,0.25)` — alpha authored 0..1.
- `TerminalView` block-marker tints: `SetAlpha(0.25f)`, `SetAlpha(1.0f)` — 0..1.
- Serialization round-trips alpha as a float (`TextBuffer.cpp` `WritePod<float>(c.A())` /
  `FromRGBA(r,g,b,a)`), assuming the stored value is already the canonical fraction.

## The defect (one writer, one theme value)

`SublimeConfigColorScript::ExecuteAlpha` (`src/Core/Sublime/SublimeConfigColorScript.cpp`):

```cpp
col.SetAlpha(args[0].Number());   // stores the raw argument — NOT normalized
```

The shipped theme (`Assets/Resources/colors.json`) uses it as a 0..255 magnitude:

```json
"selection_alpha" : "alpha(224)",
"selection": "color(var(orange), var(selection_alpha))"
```

`color(orange, selection_alpha)` resolves through `ExecuteColor`’s color-adjuster branch as a
component-wise `ColorRGBA::operator*` — including alpha — so `orange{…,a=1.0} * white{…,a=224.0}`
yields the content selection color with **`a = 224.0`**. `LineRender::DrawLines` hands that color to the
draw context as the overlay color, so `fgColor.A() == 224` at overlay-draw time.

> The mislabel matters: bug 11’s one-liner says "alpha is stored 0..255". More precisely, alpha is
> 0..1 *by convention everywhere*, and `alpha()` is the single function that violates it (plus the one
> theme value that feeds it a 0..255 number). The write side is tiny; the read side is where it bites.

## Downstream consequences (the read side)

| Consumer | Reads alpha as | Effect of `a = 224` |
| --- | --- | --- |
| **SDL2/SDL3 `SDLColor`** (`SDLColor.h`) | `AlphaAsInt()` → `a*255` into `SDL_SetRenderDrawColor` (Uint8) | `224*255 = 57120` wraps to a Uint8 of **32** → the selection renders *faint*. "Works" only by accident; the visual was effectively tuned against this wrap. |
| **Gansi `GansiDrawContext::DrawLineOverlays`** | `a` as a 0..1 blend factor | `c*(1-224)` overflows the `uint8` cast → **green** selection (open-bugs bug 10). Now carries two stopgaps: `if (a > 1) a /= 255` and `a = 1.0 - a` ("invert for now", to match SDL’s accidental faintness). |
| **JS color API** (`NamedColorsAPIWrapper.cpp`) | exposes `IntAlpha` = `AlphaAsInt()` as JS property `a` | a script reading `color.a` on the selection sees **57120**, not `0..255`. |
| **`ColorRGBA::operator==` / `ToUint32`** | compares/packs `AlphaAsInt()` | `57120` (vs a 0..255 expectation) — equality/serialization of any out-of-range-alpha color is off. |
| **Serialization** (`TextBuffer.cpp`) | writes/reads `a` as a float | round-trips faithfully *as 224*; harmless only because the persisted span colors are theme syntax colors (opaque, `a=1.0`), not the selection. |

The crux: **the theme alpha value was effectively authored against SDL’s wrapped output** (`alpha(224)`
→ ~32/255 ≈ 0.12 on screen). So normalizing the magnitude *changes the look* unless the value is
re-tuned — this is not a pure no-op refactor.

## Touch-point inventory (what the branch must sweep)

- `src/Core/ColorRGBA.{h,cpp}` — `SetAlpha`, `AlphaAsInt`, `IntAlpha`, `FromRGBA(int)` (already ÷255),
  `FromRGBA(float)`, `FromHSLA`, `operator*` (multiplies alpha), `operator==`, `ToUint32`.
- `src/Core/Sublime/SublimeConfigColorScript.cpp` — `ExecuteAlpha` (the fix site), and audit
  `ExecuteColor`’s `* number` / `* color` adjuster paths and `ExecuteRGBA` (RGB ÷255, alpha not).
- `src/Core/Graphics/SDL2/SDLColor.h`, `src/Core/Graphics/SDL3/SDLColor.h` — `AlphaAsInt()` into
  `SDL_SetRenderDrawColor`; correct once `a ∈ [0,1]`, but re-verify the blend visually.
- `src/Core/Graphics/Gansi/GansiDrawContext.cpp` — remove **both** stopgaps (`a > 1` guard + `1.0 - a`
  invert) once the convention holds; re-point `test_gansibackend_overlay`.
- `src/Core/JSEngine/Modules/NamedColorsAPIWrapper.cpp` — `IntAlpha` exposed as JS `a`; decide whether JS
  sees 0..255 (status quo) or 0..1, and document it.
- `src/Core/TextBuffer.cpp` — alpha (de)serialization; confirm nothing persists an out-of-range alpha.
- `Assets/Resources/colors.json` (+ `Assets/testfiles/colors.json`) — `selection_alpha`; re-tune after
  normalizing (e.g. `alpha(224)` → ~`alpha(32)` if `alpha()` keeps 0..255 semantics, or `alpha(0.12)` if
  it switches to 0..1).

## Fix plan

The two candidate conventions for `alpha(N)` both converge on `a ∈ [0,1]` downstream; pick one and make
it consistent with the sibling script functions:

- **Option A — `alpha(N)` takes 0..255** (consistent with `rgb()`/`rgba()` RGB channels): normalize in
  `ExecuteAlpha` → `col.SetAlpha(n / 255.0f)`. Theme keeps `alpha(224)` but it now means 0.88.
- **Option B — `alpha(N)` takes 0..1** (consistent with `hsla()`/`rgba()` alpha channels): leave
  `ExecuteAlpha` as-is and change the theme to `alpha(0.x)`.

Recommended: **Option A** — least surprising for an `alpha(224)`-style value and matches how `rgb()`
already treats 0..255 inputs. Then:

1. Normalize at the write site (Option A or B).
2. **Re-tune** `selection_alpha` so the selection looks right at a *correct* 0..1 alpha (SDL will jump
   from the wrapped ~0.12 to the literal value, so expect to lower the number).
3. Remove the Gansi stopgaps (`a > 1` guard + `1.0 - a` invert) in `DrawLineOverlays`; update
   `test_gansibackend_overlay` to assert the real (no-invert) blend.
4. Re-verify SDL2 + SDL3 selection rendering, and confirm the JS `a` property’s documented range.
5. Add a `ColorRGBA` / color-script unit test pinning `alpha(...)` → `a ∈ [0,1]` so this can’t regress.

## Verification

- Unit: a color-script test that `alpha(224)` (or `alpha(0.88)`) yields `a ≈ 0.88`, and `selection`
  resolves to a 0..1 alpha; updated `test_gansibackend_overlay`.
- Visual: selection highlight in **all three** backends (SDL2, SDL3, Gansi) reads as the same faint
  tint, text legible — the screenshots `screenshots/Blend_SDL.png` / `screenshots/Blend_ansi.png` are the
  before-reference (ANSI green vs SDL faint).

## Cross-references

- [`open-bugs.md`](open-bugs.md) bug 10 — Gansi overlay color (FIXED, carries the two stopgaps this
  effort removes) and bug 11 (this issue’s tracker stub).
- `src/Core/Editor/LineRender.cpp` — sets `fgColor` to the theme `selection` color right before
  `DrawLineOverlays`, the path that surfaces the bad alpha.
