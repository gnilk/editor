# VS Code's default themes - source material

Untouched copies from [microsoft/vscode](https://github.com/microsoft/vscode), commit
`8888f9073f635b38814f79f00c3cfdedcbc56bba` (main, 2026-11-18), MIT licensed (`LICENSE.txt` here is the repository's):

- `package.json` - `extensions/theme-defaults/package.json` (`"license": "MIT"`, the repository the importer records)
- `themes/2026-dark.json` and what it includes: `dark_modern.json` -> `dark_plus.json` -> `dark_vs.json`
  (`extensions/theme-defaults/themes/`)
- `LICENSE.txt` - the repository's `LICENSE.txt` ("Copyright (c) 2015 - present Microsoft Corporation")

Kept as they are so an improved importer can convert them again: `scripts/gen-themes.py` writes
`Assets/Resources/Themes/` from these plus our overlays (`Assets/ThemeSources/overlays/`) - see
`docs/theme-collection.md` §4.1. Don't edit these files; change the overlay. To take a newer upstream, replace them from
one commit and update the commit above.

The terminal palette in the overlay is VS Code's default dark ANSI colours from
`src/vs/workbench/contrib/terminal/common/terminalColorRegistry.ts` at the same commit (the themes don't set them).
