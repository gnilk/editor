# Theme collection - shipped themes, their licences, switching

Status: planned (2026-11-13); §3 (the importer's licence keys) built.

Today one theme ships (`Assets/Resources/default.theme.yml` + `colors.json`, Mariana-like). Themes the user imports
(`.theme --import`, `--import-theme`, `docs/code-index.md` §10.1) land next to the user config and exist on that machine
only. The rendering work is compared between the Mac and Linux boxes, so a theme used for that must come from git -
shipped - or be copied by hand.

## 1. Goals

- A handful of shipped themes, the same on every platform (they come with the build).
- Every shipped theme carries where it came from and its licence - in the file, so nothing else has to be kept in sync.
- Switch between them without editing YAML by hand (`.theme list`, `.theme use <name>`; restart is fine at first).
- Each theme gets a terminal palette that suits its background, not xterm's 16 defaults.

## 2. `Themes/` folder

- `Assets/Resources/Themes/<name>.theme.yml` + `<name>.colors.json`, installed like `Languages/` (`docs/install-layout.md`).
- The user's own go in `~/.config/goatedit/themes/` (the importer writes there); the flat user config dir stays read so
  existing imports keep working.
- Lookup of `main.theme` and of a theme's `colorfile:`: user `themes/`, user dir (legacy), system `Themes/`, system root
  (legacy - `default.theme.yml` moves into `Themes/`, the old place is read until configs are updated).
- `Themes/LICENSES.md` - one section per third-party theme: name, upstream, the licence text (MIT needs the copyright
  line and the licence kept with the file).

## 3. Licence keys in a theme file (built first)

Top-level keys in `<name>.theme.yml`, written by the importer, never read by the editor (only shown by `.theme list`):

```yaml
name: "2026 Dark"
author: "vscode"                                   # the theme's / package's author or publisher
license: "MIT"                                     # SPDX id as found; empty = not found
copyright: "Copyright (c) Microsoft Corporation"   # the line from the package's LICENSE; empty = not found
source: "https://github.com/microsoft/vscode.git"  # the package's repository, else the file imported
font: ...
colorfile: ...
```

`colors.json` gets the same `author` / `license` (it is the file with the colours in it, the one most likely copied on
its own).

Where the importer finds them, first hit wins per key:
1. The theme file itself - Sublime schemes carry `author`; any `license` / `copyright` key is taken.
2. The package around it - the nearest `package.json` in the theme's folder or up to two folders above (a VS Code
   extension: `themes/x.json` next to `../package.json`): `license`, `author` (string or `{name}`), else `publisher`,
   `repository` (string or `{url}`).
3. A plain `LICENSE` / `LICENSE.txt` / `LICENSE.md` in that package folder: its first `Copyright ...` line.

Never a product licence above the package (VS Code's app bundle has `LICENSE.rtf` for the product, which isn't the
theme's licence). A key not found is written empty and the import report says so - "no licence found: check before
sharing this theme".

**Built (2026-11-13, merged):** `ThemeImporter::Provenance` (`FromPackageJson`,
`CopyrightLine`, the theme's own keys at depth 0 - not an included parent's), `ThemesAPI` finds the package and
writes the keys. Importing VS Code's installed `2026-dark.json` gives `author: vscode`, `license: MIT`,
`source: https://github.com/microsoft/vscode.git`, `copyright: ""` (no LICENSE in the bundle's extension folder; the
product's `LICENSE.rtf` above it is correctly not read). Checked by `themeimport` `provenance`.

## 4. Licences of the candidates

- **VS Code built-in themes** (`extensions/theme-defaults`: Dark 2026, Dark Modern, Dark+, Light ...) - the package
  says `"license": "MIT"`, repository `microsoft/vscode`. Shippable with the MIT text and "Copyright (c) Microsoft
  Corporation". The app bundle has no copyright line in the package, so the importer leaves `copyright` empty there -
  fill it from the repo's `LICENSE.txt` before shipping. Name it so it isn't passed off as Microsoft's own: "Dark 2026
  (from VS Code)". Check the repo copy of the file (it may differ from the installed one) before vendoring.
- **Sublime's bundled schemes** - no licence text in the packages; prefer the open upstreams.
- **Night Owl** etc. - check each upstream (most popular VS Code themes are MIT).

## 5. Steps

1. Importer writes the licence keys (§3) - branch `feature/theme-licence-keys`.
2. `Themes/` folder, lookup order, install rule, `default.theme.yml` moved (§2).
3. `.theme list` (name, author, licence, user/system) and `.theme use <name>` (a `.cfg --user set main.theme`).
4. Vendor the first themes: Dark 2026 (from VS Code), with `LICENSES.md`; then a light one. Each with its own terminal
   palette.
