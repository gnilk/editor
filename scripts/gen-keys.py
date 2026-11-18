#!/usr/bin/env python3
"""
Writes docs/keys.md - every shipped key binding, mouse binding and command, from the files the editor reads:
the keymaps named in Assets/Resources/config.yml (macos: / linux: sections) and src/Plugins/jsplugs.yml.
Generated so it can't drift from them (hitlist #40); docs/usage.md is the short hand-written guide.

    python3 scripts/gen-keys.py           # write docs/keys.md
    python3 scripts/gen-keys.py --check   # exit 1 if docs/keys.md is not what this would write

No dependencies: the keymaps are a small subset of flow-style YAML ({ } [ ] , : scalars, # comments), parsed here;
anything else is an error, not a guess.
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RESOURCES = os.path.join(ROOT, 'Assets', 'Resources')
PLUGINS = os.path.join(ROOT, 'src', 'Plugins')
OUT = os.path.join(ROOT, 'docs', 'keys.md')

# The keymaps a user meets, in the order the reference lists them
KEYMAPS = [
    ('default_keymap', 'Editor (and every view without its own keymap)'),
    ('workspace_keymap', 'Project tree'),
    ('terminal_keymap', 'Terminal'),
    ('quickmode_keymap', 'Quick mode (after Esc)'),
    ('modal_keymap', 'Dialogs (find, open, go to symbol, ...)'),
    ('hexview_keymap', 'Hex view'),
]
PLATFORMS = [('macos', 'macOS'), ('linux', 'Linux')]


# --- a parser for the YAML subset the keymaps use ------------------------------------------------------------------

class ParseError(Exception):
    pass


def tokenize(text, path):
    tokens = []
    i, line = 0, 1
    n = len(text)
    while i < n:
        c = text[i]
        if c == '\n':
            line += 1
            i += 1
        elif c in ' \t\r':
            i += 1
        elif c == '#':
            while i < n and text[i] != '\n':
                i += 1
        elif c in '{}[],':
            tokens.append((c, None, line))
            i += 1
        elif c == ':' and (i + 1 >= n or text[i + 1] in ' \t\r\n'):
            tokens.append((':', None, line))
            i += 1
        elif c == '"':
            i += 1
            value = ''
            while i < n and text[i] != '"':
                if text[i] == '\\' and i + 1 < n:
                    i += 1
                value += text[i]
                i += 1
            if i >= n:
                raise ParseError(f'{path}:{line}: unterminated "string"')
            i += 1
            tokens.append(('scalar', value, line))
        elif c == "'":
            i += 1
            value = ''
            while i < n:
                if text[i] == "'":
                    if i + 1 < n and text[i + 1] == "'":
                        value += "'"
                        i += 2
                        continue
                    break
                value += text[i]
                i += 1
            if i >= n:
                raise ParseError(f"{path}:{line}: unterminated 'string'")
            i += 1
            tokens.append(('scalar', value, line))
        else:
            # A plain scalar: up to , ] } a newline, a ': ' or ' #'. Quotes inside it (CopyPasteModifier + 'c') are text.
            start = i
            while i < n and text[i] not in ',]}\n':
                if text[i] == ':' and (i + 1 >= n or text[i + 1] in ' \t\r\n'):
                    break
                if text[i] == '#' and text[i - 1] in ' \t':
                    break
                if text[i] == "'":
                    end = text.find("'", i + 1)
                    if end < 0 or text.find('\n', i) < end:
                        raise ParseError(f"{path}:{line}: unterminated ' in a plain scalar")
                    i = end + 1
                    continue
                i += 1
            tokens.append(('scalar', text[start:i].strip(), line))
    return tokens


def parse(text, path):
    tokens = tokenize(text, path)
    pos = 0

    def peek():
        return tokens[pos] if pos < len(tokens) else (None, None, -1)

    def take(kind):
        nonlocal pos
        tok = peek()
        if tok[0] != kind:
            raise ParseError(f'{path}:{tok[2]}: expected {kind!r}, got {tok[0]!r} {tok[1]!r}')
        pos += 1
        return tok

    def value():
        tok = peek()
        if tok[0] == '{':
            return mapping()
        if tok[0] == '[':
            return sequence()
        if tok[0] == 'scalar':
            take('scalar')
            return tok[1]
        raise ParseError(f'{path}:{tok[2]}: unexpected {tok[0]!r}')

    def mapping():
        take('{')
        out = {}
        while peek()[0] != '}':
            key = take('scalar')[1]
            take(':')
            out[key] = value()
            if peek()[0] == ',':
                take(',')
        take('}')
        return out

    def sequence():
        take('[')
        out = []
        while peek()[0] != ']':
            out.append(value())
            if peek()[0] == ',':
                take(',')
        take(']')
        return out

    # The top level is block style: 'keymap: { ... }' / 'commands:' lists are not used here
    out = {}
    while peek()[0] is not None:
        key = take('scalar')[1]
        take(':')
        out[key] = value()
    return out


# --- config.yml: just the two keymap sections (block style, 'name: file' lines) -----------------------------------

def keymap_files():
    sections, current = {}, None
    with open(os.path.join(RESOURCES, 'config.yml'), encoding='utf-8') as f:
        for raw in f:
            line = raw.rstrip('\n')
            top = re.match(r'^([a-z_]+):\s*(#.*)?$', line)
            if top:
                current = top.group(1) if top.group(1) in ('macos', 'linux') else None
                if current:
                    sections[current] = {}
                continue
            if current and re.match(r'^\S', line):
                current = None
            entry = re.match(r'^\s+([a-z_]+):\s*(\S+)', line)
            if current and entry:
                sections[current][entry.group(1)] = entry.group(2)
    return sections


def load_keymap(relpath):
    path = os.path.join(RESOURCES, relpath)
    with open(path, encoding='utf-8') as f:
        doc = parse(f.read(), relpath)
    if 'keymap' not in doc:
        raise ParseError(f'{relpath}: no keymap')
    return doc['keymap']


# --- bindings as text ----------------------------------------------------------------------------------------------

KEY_NAMES = {
    'KeyCode_Return': 'Enter', 'KeyCode_Escape': 'Esc', 'KeyCode_Tab': 'Tab', 'KeyCode_Backspace': 'Backspace',
    'KeyCode_Delete': 'Delete', 'KeyCode_Home': 'Home', 'KeyCode_End': 'End', 'KeyCode_PageUp': 'PgUp',
    'KeyCode_PageDown': 'PgDn', 'KeyCode_UpArrow': 'Up', 'KeyCode_DownArrow': 'Down', 'KeyCode_LeftArrow': 'Left',
    'KeyCode_RightArrow': 'Right', 'KeyCode_Space': 'Space', 'KeyCode_Insert': 'Insert',
}
MODIFIER_ORDER = ['Ctrl', 'Cmd', 'Super', 'Opt', 'Alt', 'Shift']


def modifier_name(code, platform):
    if code in ('KeyCode_Ctrl', 'KeyCode_Control'):
        return 'Ctrl'
    if code in ('KeyCode_Cmd', 'KeyCode_Command'):
        return 'Cmd' if platform == 'macos' else 'Super'
    if code in ('KeyCode_Alt', 'KeyCode_Option'):
        return 'Opt' if platform == 'macos' else 'Alt'
    if code == 'KeyCode_Shift':
        return 'Shift'
    return None


def binding_text(expr, modifiers, platform, where):
    """'CopyPasteModifier + SelectionModifier + 'b'' -> 'Cmd+Shift+B'; '@Mod' (optional) -> '[Shift]+...'"""
    mods, optional, key = [], [], None
    for part in [p.strip() for p in expr.split('+')] if expr.strip() != "'+'" else [expr.strip()]:
        if not part:
            continue
        isOptional = part.startswith('@')
        name = part[1:] if isOptional else part
        if name in modifiers:
            name = modifiers[name]
        mod = modifier_name(name, platform)
        if mod:
            (optional if isOptional else mods).append(mod)
            continue
        if isOptional:
            raise ParseError(f'{where}: optional {part!r} is not a modifier')
        if key is not None:
            raise ParseError(f'{where}: two keys in {expr!r}')
        if re.fullmatch(r"'.'", name) or re.fullmatch(r"'\\.'", name):
            ch = name[1:-1].lstrip('\\') or '\\'
            key = ch
        elif name in KEY_NAMES:
            key = KEY_NAMES[name]
        elif re.fullmatch(r'KeyCode_F\d+', name):
            key = name[len('KeyCode_'):]
        elif name in ('Click', 'DoubleClick', 'TripleClick', 'MiddleClick'):
            key = {'Click': 'click', 'DoubleClick': 'double-click', 'TripleClick': 'triple-click',
                   'MiddleClick': 'middle-click'}[name]
        else:
            raise ParseError(f'{where}: unknown key {part!r} in {expr!r}')
    if key is None:
        raise ParseError(f'{where}: no key in {expr!r}')
    mods.sort(key=MODIFIER_ORDER.index)
    if mods and len(key) == 1 and key.isalpha():
        key = key.upper()           # Cmd+B, as menus write it; a plain letter stays as typed (quick mode's 'n')
    return '`' + '+'.join(mods + [f'[{m}]' for m in optional] + [key]) + '`'


def bindings_of(value):
    return value if isinstance(value, list) else [value]


def resolved_modifiers(name, files, cache):
    """A keymap's modifier names, with what its inherit chain defines (the child wins)"""
    if name in cache:
        return cache[name]
    keymap = load_keymap(files[name]) if name in files else {}
    mods = {}
    parent = keymap.get('inherit')
    if parent:
        mods.update(resolved_modifiers(parent, files, cache))
    mods.update(keymap.get('modifiers', {}) or {})
    cache[name] = mods
    return mods


def humanize(action):
    words = re.sub(r'(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])', ' ', action).split()
    if words and words[0] == 'UI':
        words = words[1:]
    text = ' '.join(words)
    return text[:1].upper() + text[1:].lower()


def switch_to_buffer(texts):
    """SwitchToBuffer has one binding per digit - '`Cmd+1`'..'`Cmd+9`' reads as '`Cmd+1..9`'"""
    out, seen = [], set()
    for t in texts:
        m = re.fullmatch(r'`(.*\+)1`', t)
        if m and all(f'`{m.group(1)}{d}`' in texts for d in range(1, 10)):
            if m.group(1) not in seen:
                out.append(f'`{m.group(1)}1..9`')
                seen.add(m.group(1))
        elif not re.fullmatch(r'`.*\+[2-9]`', t) or not any(t.startswith(f'`{p}') for p in seen):
            out.append(t)
    return out


# --- commands ------------------------------------------------------------------------------------------------------

def commands():
    """src/Plugins/jsplugs.yml - block style, '- name / short / description / script / arguments' entries"""
    out, current, inArgs = [], None, False
    with open(os.path.join(PLUGINS, 'jsplugs.yml'), encoding='utf-8') as f:
        for raw in f:
            line = raw.rstrip('\n')
            if re.match(r'^\s*-\s*$', line):
                current = {'arguments': []}
                out.append(current)
                inArgs = False
                continue
            m = re.match(r'^\s+(name|short|description|script):\s*(.*)$', line)
            if m and current is not None:
                current[m.group(1)] = m.group(2).strip().strip('"')
                inArgs = False
                continue
            if re.match(r'^\s+arguments:\s*$', line):
                inArgs = True
                continue
            m = re.match(r'^\s+-\s+(.*)$', line)
            if m and inArgs and current is not None:
                current['arguments'].append(m.group(1).strip())
    for c in out:
        if 'name' not in c:
            raise ParseError(f'jsplugs.yml: an entry without a name: {c}')
    return out


# --- the document --------------------------------------------------------------------------------------------------

def generate():
    files = keymap_files()
    for platform, _ in PLATFORMS:
        if platform not in files:
            raise ParseError(f'config.yml: no {platform}: section')

    lines = [
        '# Keys, mouse and commands - the full reference',
        '',
        '<!-- Generated by scripts/gen-keys.py from the shipped keymaps and src/Plugins/jsplugs.yml - do not edit;',
        '     run `python3 scripts/gen-keys.py` after changing a binding. -->',
        '',
        'Every binding the shipped keymaps make, per platform. The short guide is [`usage.md`](usage.md).',
        '',
        '- **macOS** is the window version on a Mac. **Linux** is the window version on Linux - and the **terminal',
        '  version on both** (`--backend ansi`, or no display): a terminal never sends `Cmd`, so it takes these keys on a',
        '  Mac too, with `Opt` as `Alt` (the terminal must send Option as Meta - see `usage.md`).',
        '- `[Shift]+End`: `Shift` is optional - held, it extends the selection. *inherited*: the keymap it inherits',
        '  decides (the Editor table).',
        '- A view\'s keymap is looked up first, then the one it inherits (named under each table).',
        '',
    ]
    caches = {p: {} for p, _ in PLATFORMS}
    for name, title in KEYMAPS:
        per = {}
        inherit = {}
        mice = {}
        order = []
        for platform, _ in PLATFORMS:
            if name not in files[platform]:
                raise ParseError(f'config.yml {platform}: names no {name}')
            keymap = load_keymap(files[platform][name])
            mods = resolved_modifiers(name, files[platform], caches[platform])
            inherit[platform] = keymap.get('inherit')
            for action, value in (keymap.get('actions') or {}).items():
                if action not in order:
                    order.append(action)
                texts = [binding_text(b, mods, platform, f'{files[platform][name]} {action}') for b in bindings_of(value)]
                per.setdefault(action, {})[platform] = switch_to_buffer(texts)
            for action, value in (keymap.get('mouse') or {}).items():
                texts = [binding_text(b, mods, platform, f'{files[platform][name]} mouse {action}') for b in bindings_of(value)]
                mice.setdefault(action, {})[platform] = texts

        lines += [f'## {title}', '']
        if order:
            lines += ['| Action | macOS | Linux / terminal |', '|---|---|---|']
            for action in order:
                cells = [', '.join(per[action].get(p, [])) or ('*inherited*' if inherit[p] else '-') for p, _ in PLATFORMS]
                lines.append(f'| {humanize(action)} (`{action}`) | {cells[0]} | {cells[1]} |')
            lines.append('')
        else:
            lines += ['No bindings of its own.', '']
        if mice:
            lines += ['Mouse:', '', '| Action | macOS | Linux / terminal |', '|---|---|---|']
            for action, per_platform in mice.items():
                cells = [', '.join(per_platform.get(p, [])) or '-' for p, _ in PLATFORMS]
                lines.append(f'| {humanize(action)} (`{action}`) | {cells[0]} | {cells[1]} |')
            lines.append('')
        parents = sorted({v for v in inherit.values() if v})
        if parents:
            lines += [f'Inherits `{", ".join(parents)}`.', '']
        lines.append(f'Files: ' + ', '.join(f'`Assets/Resources/{files[p][name]}`' for p, _ in PLATFORMS))
        lines.append('')

    lines += [
        '## Commands',
        '',
        'Typed in quick mode (`Esc`, then `.name` or `.short`, `Enter`) or at the terminal\'s prompt. `!` after a command',
        '(`.q!`) forces it where it asks first.',
        '',
        '| Command | Short | Arguments | Does |',
        '|---|---|---|---|',
    ]
    for c in commands():
        script = c.get('script', '')
        exists = os.path.exists(os.path.join(PLUGINS, script)) if script else False
        does = c.get('description', '')
        if not exists:
            does += f' - **not available: `{script}` is missing**'
        args = '; '.join(a.split(',', 1)[-1].strip() for a in c['arguments']) or ''
        cell = lambda text: text.replace('|', '\\|')
        lines.append(f'| `.{c["name"]}` | `.{c.get("short", "")}` | {cell(args)} | {cell(does)} |')
    lines.append('')
    return '\n'.join(lines)


def main():
    try:
        text = generate()
    except ParseError as e:
        print(f'gen-keys: {e}', file=sys.stderr)
        return 2
    if '--check' in sys.argv[1:]:
        try:
            with open(OUT, encoding='utf-8') as f:
                current = f.read()
        except FileNotFoundError:
            current = ''
        if current != text:
            print('gen-keys: docs/keys.md is out of date - run python3 scripts/gen-keys.py', file=sys.stderr)
            return 1
        print('gen-keys: docs/keys.md is up to date')
        return 0
    with open(OUT, 'w', encoding='utf-8') as f:
        f.write(text)
    print(f'gen-keys: wrote {os.path.relpath(OUT, ROOT)}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
