#!/usr/bin/env python3
"""
Scripted versions of the 'To try in the app' list in docs/hitlist.md - the editor runs on its ANSI backend in a pty
and the screen (text + colours) is read back. The editor logic is the one the SDL app runs; what is NOT covered is the
SDL backend itself (its key decoding, its drawing) - those items stay hand checks.

    python3 scripts/handtests/checks.py --exe cmake-build-debug/goatedit            # all (~2.5 min)
    python3 scripts/handtests/checks.py --exe cmake-build-debug/goatedit cmake goto_line
    python3 scripts/handtests/checks.py --list

Each check runs in its own sandbox <tmp>/goatedit-handtests/<name>: its own HOME and XDG dirs (so the user's config,
session and shell history are never read or written) with the work folder inside that HOME (a project root must be
below $HOME). Every check prints what it saw, so a failure can be read without re-running it.
"""
import argparse
import platform
import re
import shutil
import tempfile
import os
import subprocess
import sys
import time
import traceback

sys.dont_write_bytecode = True        # no __pycache__ in the repo
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from goatpty import (Goat, fresh_dir, ESC, ENTER, BACKSPACE, DOWN, UP, END, HOME, PGDN, SHIFT_DOWN, SHIFT_END,
                     ctrl, alt)

SANDBOX_ROOT = os.path.join(tempfile.gettempdir(), 'goatedit-handtests')
CHECKS = []


def check(hitlist):
    def wrap(fn):
        fn.hitlist = hitlist
        CHECKS.append(fn)
        return fn
    return wrap


class Skip(Exception):
    pass


class Notes(list):
    def __call__(self, ok, what):
        self.append(('PASS' if ok else 'FAIL', what))
        return ok

    def info(self, what):
        self.append(('info', what))

    def skip(self, why):
        self.append(('SKIP', why))
        raise Skip()

    @property
    def ok(self):
        return all(s != 'FAIL' for s, _ in self)

    @property
    def skipped(self):
        return any(s == 'SKIP' for s, _ in self)


def sandbox(name):
    sb = fresh_dir(os.path.join(SANDBOX_ROOT, name))
    # Inside the sandbox's HOME: a project root must be below $HOME, and a cwd outside it is moved to $HOME
    work = os.path.join(sb, 'home', 'work')
    os.makedirs(work)
    return sb, work


def write(path, data, mode='w', encoding='utf-8'):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    if 'b' in mode:
        with open(path, mode) as f:
            f.write(data)
    else:
        with open(path, mode, encoding=encoding) as f:
            f.write(data)


def user_config(sb, text):
    write(os.path.join(sb, 'home', '.config', 'goatedit', 'config.yml'), text)


def git(work, *args):
    env = dict(os.environ, GIT_AUTHOR_NAME='t', GIT_AUTHOR_EMAIL='t@t', GIT_COMMITTER_NAME='t',
               GIT_COMMITTER_EMAIL='t@t', HOME=work)
    return subprocess.run(['git', '-C', work, *args], capture_output=True, env=env)


def modified(g):
    parts = g.status_line().split('|')
    return len(parts) > 2 and parts[2].strip().startswith('*')


# ---------------------------------------------------------------------------------------------------------------
@check('#39 Undo clears the *')
def undo_star(n):
    sb, work = sandbox('undo_star')
    write(os.path.join(work, 'a.txt'), 'hello\nworld\n')
    # Autosave (default 1000 ms) would write the undone text to disk - the * then goes, correctly. Off here.
    user_config(sb, 'main:\n  autosave_timeout_ms: 0\n')
    n.info('autosave off (autosave_timeout_ms: 0) - with autosave on, edit/save/undo is autosaved ~1 s later and the * goes')
    g = Goat(sb, ['a.txt'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        g.send(alt('e'), 0.5)
        n(not modified(g), f'fresh file: no * ({g.status_line().strip()[:70]})')
        # mistype, undo
        g.send(END, 0.2)
        g.type('x', wait=0.3)
        n(modified(g), 'after typing: *')
        g.send(ctrl('z'), 0.3)
        g.pump(1.5)
        n(not modified(g), f'mistype + undo: * gone after a pause ({g.status_line().strip()[:60]})')
        # a char + backspace
        g.type('y', wait=0.2)
        g.send(BACKSPACE, 0.3)
        g.pump(1.5)
        n(not modified(g), 'char + backspace: * gone after a pause')
        # edit, save, undo: * stays
        g.type('z', wait=0.2)
        g.quick('.s', 0.8)
        n(not modified(g), 'after .s: no *')
        with open(os.path.join(work, 'a.txt')) as f:
            n(f.read().startswith('helloz'), 'saved to disk')
        g.send(alt('e'), 0.3)
        g.send(ctrl('z'), 0.3)
        g.pump(1.5)
        n(modified(g), f'edit, save, undo: * stays ({g.status_line().strip()[:60]})')
        with open(os.path.join(work, 'a.txt')) as f:
            n(f.read().startswith('helloz'), 'and the disk still has the saved edit')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#16 Text encodings')
def latin1(n):
    sb, work = sandbox('latin1')
    git(work, 'init', '-q')
    write(os.path.join(work, 'l1.txt'), 'Räksmörgås\nåäö ÅÄÖ\nplain line\n'.encode('cp1252'), 'wb')
    git(work, 'add', '.')
    git(work, 'commit', '-qm', 'x')
    g = Goat(sb, ['l1.txt'], cwd=work)
    try:
        g.ready()
        g.send(alt('e'), 0.5)
        n(g.screen.find('Räksmörgås') is not None and g.screen.find('åäö ÅÄÖ') is not None, 'å/ä/ö shown right')
        n('cp1252' in g.status_line(), f'status line says cp1252 ({g.status_line().strip()[:70]})')
        g.send(DOWN, 0.1)
        g.send(DOWN, 0.1)
        g.send(END, 0.1)
        g.type(' edited', wait=0.3)
        g.quick('.s', 1.0)
        n(g.quit(), 'quits')
    finally:
        g.kill()
    diff = git(work, 'diff', '-U0', '--no-color').stdout.decode('latin-1')
    changed = [l for l in diff.splitlines() if l[:1] in '+-' and not l.startswith(('+++', '---'))]
    n(changed == ['-plain line', '+plain line edited'], f'git diff shows only the edit: {changed}')
    with open(os.path.join(work, 'l1.txt'), 'rb') as f:
        data = f.read()
    n(data.startswith('Räksmörgås\nåäö ÅÄÖ\n'.encode('cp1252')), 'still cp1252 bytes on disk')


@check('#42 #43 Go to line')
def goto_line(n):
    sb, work = sandbox('goto_line')
    write(os.path.join(work, 'many.txt'), ''.join(f'    line {i}\n' for i in range(1, 201)))
    g = Goat(sb, ['many.txt'], cwd=work)
    try:
        g.ready()
        g.send(alt('e'), 0.5)

        def on_42(how):
            rows = [r for r in g.editor_rows() if r[1] == 42]
            n(g.cursor_line() == 42, f'{how}: status l: 42 (got {g.cursor_line()})')
            n(bool(rows) and rows[0][2].endswith('line 42'), f'{how}: gutter 42 is the line "line 42"')
            m = re.search(r'c\((\d+):(\d+)\)', g.status_line())
            n(bool(m) and m.group(2) == '5', f'{how}: on the first non-blank, column 5 ({g.status_line().split("id:")[-1].strip()})')

        g.send(ESC, 0.3)
        g.type('g', wait=0.2)
        n('.g' in g.status_line(), f'Esc g types ".g " ({g.status_line().strip()[:60]})')
        g.type('42', wait=0.1)
        g.send(ENTER, 0.6)
        on_42('Esc g 42 Enter')
        g.send(ctrl('['), 0.6)
        n(g.cursor_line() == 1, f'Ctrl+[ comes back (l: {g.cursor_line()})')

        g.quick('.g 42', 0.6)
        g.send(alt('e'), 0.3)
        on_42('.g 42 in quick mode')
        g.send(ctrl('['), 0.6)
        n(g.cursor_line() == 1, f'Ctrl+[ comes back again (l: {g.cursor_line()})')

        # in the terminal: a '.'-line names a command
        g.send(alt('t'), 0.4)
        g.type('.g 42', wait=0.1)
        g.send(ENTER, 0.8)
        g.send(alt('e'), 0.4)
        n(g.cursor_line() == 42, f'.g 42 typed in the terminal: l: 42 (got {g.cursor_line()})')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('open-bugs #29 Theme that can\'t load')
def missing_theme(n):
    sb, work = sandbox('missing_theme')
    user_config(sb, 'main:\n  theme: nosuch.theme.yml\n')
    write(os.path.join(work, 'a.txt'), 'x\n')
    g = Goat(sb, ['a.txt'], cwd=work, skip_user_config=False)
    try:
        n(g.ready(), 'starts (status line + prompt drawn)')
        err = g.stderr()
        hit = [l for l in err.splitlines() if 'theme' in l.lower()]
        n(any('no such theme file' in l and 'default.theme.yml' in l for l in hit),
          f'stderr says "no such theme file - using default.theme.yml": {hit[:2]}')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#51 Console output above the shell prompt')
def prompt_above(n):
    sb, work = sandbox('prompt_above')
    write(os.path.join(work, 'a.txt'), 'x\n')
    g = Goat(sb, ['a.txt'], cwd=work, env={'PS1': r'\[\e[32m\]bash\$\[\e[0m\] '})
    try:
        n(g.ready(), 'start-up: the banner, then the prompt below it - no key pressed')
        g.send(alt('t'), 0.5)
        g.type('echo hal', wait=0.5)
        g.send(alt('e'), 0.5)
        g.quick('.ll', 1.0)
        lines = [l.rstrip() for l in g.screen.lines() if l.strip()]
        n(any('built-in' in l for l in lines), 'the .ll output is in the terminal')
        n(lines and lines[-1] == 'bash$ echo hal', f'the half-typed line stays below it: {lines[-1:]}')
        g.send(alt('t'), 0.5)
        g.type('f', wait=0.3)
        g.send(ENTER, 1.0)
        lines = [l.rstrip() for l in g.screen.lines() if l.strip()]
        n(lines[-2:] == ['half', 'bash$'], f'finished and run: {lines[-3:]}')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#29 Quick mode without ] / [')
def quick_bracket(n):
    sb, work = sandbox('quick_bracket')
    write(os.path.join(work, 'a.txt'), 'x\n')
    g = Goat(sb, ['a.txt'], cwd=work)
    try:
        g.ready()
        g.send(alt('e'), 0.5)
        before = g.view()
        g.send(ESC, 0.3)
        g.type(']', wait=0.4)
        g.send(ESC, 0.4)
        n(g.view() == before, f'Esc, ] : no view switch ({before} -> {g.view()})')
        g.send(ESC + b']', 0.5)
        n(g.view() != before, f'Alt+] still cycles ({before} -> {g.view()})')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#10 Languages as data: json, .ll')
def json_ll(n):
    sb, work = sandbox('json_ll')
    write(os.path.join(work, 'a.json'), '{\n  "name": "goat",\n  "count": 42,\n  "ok": true,\n  "none": null\n}\n')
    g = Goat(sb, ['a.json'], cwd=work)
    try:
        g.ready()
        g.send(alt('e'), 0.5)
        r = g.screen.find('"name"')
        if n(r is not None, 'json file shown'):
            row = r[0]
            key, val = g.screen.colour_of(row, '"name"'), g.screen.colour_of(row, '"goat"')
            rc = g.screen.find('42')[0]
            num = g.screen.colour_of(rc, '42')
            rt = g.screen.find('true')[0]
            kw = g.screen.colour_of(rt, 'true')
            brace = g.screen.colour_of(g.screen.find('{')[0], '{', g.screen.find('{')[1])
            n.info(f'colours: key {key}, string {val}, number {num}, true {kw}, brace {brace}')
            n(len({key, val, num, kw}) >= 3 and None not in (key, val, num, kw), 'json is highlighted (key/string/number/true coloured, at least 3 colours)')
        g.quick('.ll', 1.0)
        text = '\n'.join(g.screen.lines())
        has_json = 'json' in text
        has_path = '.yml' in text
        n(has_json and has_path, 'the .ll output lists json with a .yml path')
        lines = [l for l in g.screen.lines() if '.yml' in l][:4]
        n.info('ll: ' + ' / '.join(l.strip()[:90] for l in lines))
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#10 Languages as data: tabsize')
def tabsize(n):
    def tab_col(cfg):
        sb, work = sandbox('tabsize_' + ('8' if cfg else 'default'))
        if cfg:
            user_config(sb, cfg)
        write(os.path.join(work, 'a.json'), '{\n\t"t": 1\n}\n')
        write(os.path.join(work, 'b.cpp'), 'int f() {\n\treturn 1;\n}\n')
        g = Goat(sb, ['a.json', 'b.cpp'], cwd=work, skip_user_config=False)
        cols = {}
        try:
            g.ready()
            g.send(alt('e'), 0.5)
            for _ in range(2):
                name = g.status_line().split('|')[2].strip() if g.status_line().count('|') > 2 else '?'
                for needle, key in (('"t"', 'json'), ('return', 'cpp')):
                    r = g.screen.find(needle)
                    if r:
                        r1 = g.screen.find('{') if key == 'json' else g.screen.find('int f')
                        cols[key] = r[1] - r1[1]
                g.send(ESC + b'\\', 0.6)       # next buffer
            g.quit()
        finally:
            g.kill()
        return cols

    plain = tab_col(None)
    eight = tab_col('languages:\n  default:\n    tabsize: 8\n')
    n.info(f'tab width (column of the tabbed text minus line 1): default {plain}, tabsize 8 {eight}')
    n(eight.get('json') == 8, f'json: tab is 8 wide with languages.default.tabsize 8 (got {eight.get("json")})')
    n(eight.get('cpp') == 8, f'cpp: tab is 8 wide with languages.default.tabsize 8 (got {eight.get("cpp")})')
    n(plain.get('json') != 8 or plain.get('cpp') != 8, 'and not 8 without the setting (so the setting did it)')


SELECTED_BG = '474642'


def tree_cursor_row(g):
    """The project tree's cursor row: drawn inverted (bg = the default fg)"""
    for r in range(g.rows):
        runs = g.screen.runs(r, 0)
        if runs and runs[0][2] == 'd7dde8' and '│' not in runs[0][0]:
            return r, runs[0][0].strip()
    return None, None


def tree_select(g, name, limit=60):
    """Move the tree cursor onto the row named 'name' (Up first, then Down)"""
    for key in (UP, DOWN):
        for _ in range(limit):
            _, text = tree_cursor_row(g)
            if text and text.lstrip('+-').rstrip('/').strip() == name:
                return True
            before = text
            g.send(key, 0.12)
            if tree_cursor_row(g)[1] == before:
                break
    return False


def selected_rows(g):
    return [r for r in range(g.rows) if any(bg == SELECTED_BG for _, _, bg in g.screen.runs(r, 0))]


@check('#47 Promote to terminal: line, selection')
def promote(n):
    sb, work = sandbox('promote')
    write(os.path.join(work, 'p.sh'), 'echo promoted-$((40+2))\necho one\necho two\necho last\n')
    g = Goat(sb, ['p.sh'], cwd=work)
    try:
        g.ready()
        g.send(alt('e'), 0.5)
        g.send(ctrl('enter'), 0.3)
        n(g.wait_text('promoted-42', 4), 'a line, no selection: runs in the terminal (promoted-42)')
        n(g.view() == 'EDT', f'focus stays in the editor ({g.view()})')
        g.send(DOWN, 0.2)
        g.send(SHIFT_DOWN, 0.2)
        g.send(SHIFT_END, 0.3)
        sel_before = selected_rows(g)
        g.send(ctrl('enter'), 0.3)
        ran = g.wait_for(lambda g: g.screen.find('one') is not None and
                         any(t.strip() == 'one' for t in g.screen.lines()) and
                         any(t.strip() == 'two' for t in g.screen.lines()), 4)
        n(ran, 'a selection of two lines: both run (output "one" and "two")')
        n(g.view() == 'EDT', f'focus stays in the editor ({g.view()})')
        sel_after = selected_rows(g)
        n(len(sel_before) == 2 and sel_after == sel_before, f'the selection stays (selected rows {sel_before} -> {sel_after})')
        # send again
        count = sum(1 for t in g.screen.lines() if t.strip() == 'two')
        g.send(ctrl('enter'), 0.3)
        again = g.wait_for(lambda g: sum(1 for t in g.screen.lines() if t.strip() == 'two') > count, 4)
        n(again, 'send again: runs again')
        n(g.quit(), 'quits')
    finally:
        g.kill()


PASTE_READER = r"""
import os, sys, termios, tty, time, select
out = sys.argv[1]
fd = sys.stdin.fileno()
old = termios.tcgetattr(fd)
tty.setraw(fd)
os.write(1, b'\x1b[?2004h')
os.write(1, b'READER-READY\r\n')
data = b''
end = time.time() + float(sys.argv[2])
while time.time() < end:
    r, _, _ = select.select([fd], [], [], 0.1)
    if r:
        data += os.read(fd, 4096)
os.write(1, b'\x1b[?2004l')
termios.tcsetattr(fd, termios.TCSADRAIN, old)
open(out, 'wb').write(data)
print('READER-DONE')
"""


@check('#47 Promote to terminal: a paste into a program (as claude gets it)')
def promote_paste(n):
    def run(cfg):
        sb, work = sandbox('promote_paste' + ('_noenter' if cfg else ''))
        if cfg:
            user_config(sb, cfg)
        write(os.path.join(work, 'reader.py'), PASTE_READER)
        write(os.path.join(work, 'prompt.md'), 'Explain this:\nline two of the prompt\n')
        out = os.path.join(work, 'got.bin')
        g = Goat(sb, ['prompt.md'], cwd=work, skip_user_config=False)
        try:
            g.ready()
            g.send(alt('t'), 0.4)
            g.type(f'python3 reader.py {out} 6', wait=0.1)
            g.send(ENTER, 0.3)
            g.wait_text('READER-READY', 5)
            g.send(alt('e'), 0.4)
            g.send(SHIFT_DOWN, 0.2)
            g.send(SHIFT_END, 0.2)
            g.send(ctrl('enter'), 0.5)
            g.wait_text('READER-DONE', 10)
            g.quit()
        finally:
            g.kill()
        try:
            return open(out, 'rb').read()
        except OSError:
            return None

    got = run(None)
    n.info(f'the program read: {got!r}')
    n(got is not None and got.startswith(b'\x1b[200~') and b'\x1b[201~' in got,
      'the selection arrives as one bracketed paste (\\e[200~ ... \\e[201~)')
    n(got is not None and b'Explain this:' in got and b'line two of the prompt' in got, 'both lines are in the paste')
    n(got is not None and got.endswith(b'\x1b[201~\r'), 'and is sent: Enter right after the paste')
    got = run('terminal:\n  promote_sends_enter: no\n')
    n.info(f'with promote_sends_enter: no, the program read: {got!r}')
    n(got is not None and got.endswith(b'\x1b[201~'), 'promote_sends_enter: no - the paste waits, no Enter')


@check('Big folder: start-up, quit, deep folders, session')
def big_folder(n):
    sb, work = sandbox('big_folder')
    nm = os.path.join(work, 'node_modules')
    t0 = time.time()
    for i in range(1500):
        d = os.path.join(nm, f'pkg{i:04d}', 'lib')
        os.makedirs(d)
        for j in range(40):
            open(os.path.join(d, f'f{j}.js'), 'w').close()
    deep = os.path.join(work, 'a', 'b', 'c', 'd', 'e', 'f')
    os.makedirs(deep)
    write(os.path.join(deep, 'deep.txt'), 'deep file\n')
    write(os.path.join(work, 'top.txt'), 'top\n')
    n.info(f'tree: 1500 packages x 40 files = 60000 files in node_modules (not ignored), a/b/c/d/e/f/deep.txt; made in {time.time() - t0:.1f}s')

    # .q! right after start-up
    g = Goat(sb, ['.'], cwd=work)
    try:
        up = g.wait_for(lambda g: 'GoatEdit v' in g.status_line(), 10)
        t_up = time.time() - g.started
        n(up, f'status line up in {t_up:.2f}s')
        g.send(ESC, 0.2)
        g.type('.q!', per_key=0.02, wait=0.0)
        t_q = time.time()
        g.send(ENTER, 0.0)
        exited = g.wait_exit(5)
        n(exited and time.time() - t_q < 1.0, f'.q! right after start-up exits at once ({time.time() - t_q:.2f}s from Enter)')
    finally:
        g.kill()

    g = Goat(sb, ['.'], cwd=work)
    try:
        g.ready(10)
        # answers at once: a view switch while the scan/index runs
        lat = []
        for key, want in ((alt('t'), 'TRM'), (alt('e'), 'EDT'), (alt('p'), 'WSP')):
            t = time.time()
            g.send(key, 0.0)
            g.wait_for(lambda g: g.view() == want, 3)
            lat.append(time.time() - t)
        n(max(lat) < 0.5, f'the window answers at once while scanning (view switches in {", ".join(f"{x * 1000:.0f}" for x in lat)} ms)')
        # deep folders open when expanded
        path_ok = True
        for name in ('a', 'b', 'c', 'd', 'e', 'f'):
            if not tree_select(g, name):
                path_ok = False
                n(False, f'tree: found folder {name}')
                break
            g.send(ENTER, 0.4)
        if path_ok:
            n(g.screen.find('deep.txt') is not None, 'folders below the fourth level open when expanded (deep.txt shows)')
            if tree_select(g, 'deep.txt'):
                g.send(ENTER, 0.6)
        g.send(alt('e'), 0.4)
        n('deep.txt' in g.status_line(), f'deep.txt opened ({g.status_line().split("|")[2].strip() if g.status_line().count("|") > 2 else "?"})')
        t_q = time.time()
        g.quick('.q!', 0.0)
        exited = g.wait_exit(5)
        n(exited, f'quits ({time.time() - t_q:.2f}s)')
    finally:
        g.kill()

    g = Goat(sb, ['.'], cwd=work)
    try:
        g.ready(10)
        g.send(alt('e'), 0.5)
        n(g.wait_text('deep file', 3), f'the session file deep in the tree reopens ({g.status_line().split("|")[2].strip() if g.status_line().count("|") > 2 else "?"})')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#23 Single-folder project')
def single_folder(n):
    sb, work = sandbox('single_folder')
    git(work, 'init', '-q')
    write(os.path.join(work, '.goatedit', 'config.yml'), 'main:\n  tabsize: 4\n')
    write(os.path.join(work, 'a.txt'), 'file a\n')
    write(os.path.join(work, 'b.txt'), 'file b\n')
    # a session: open b.txt in a project launch
    g = Goat(sb, ['.'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        g.quick('.o b.txt', 0.8)
        g.quit()
    finally:
        g.kill()
    n.info(f'session made by "goatedit ." with b.txt open: {sorted(os.listdir(os.path.join(work, ".goatedit")))}')

    g = Goat(sb, ['a.txt', '.'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        err = g.stderr()
        want = f"not a single-folder launch - project settings in '{os.path.realpath(work)}/.goatedit' not used"
        hit = [l for l in err.splitlines() if 'single-folder' in l]
        n(any(want in l for l in hit) or any("not a single-folder launch" in l and ".goatedit" in l for l in hit),
          f'goatedit a.txt . : stderr says "not a single-folder launch ..." : {hit[:1]}')
        g.send(alt('e'), 0.4)
        n(g.screen.find('file b') is None, 'no session restored (b.txt not open)')
        g.send(alt('t'), 0.4)
        g.type('touch made-in-terminal.txt', wait=0.1)
        g.send(ENTER, 0.3)
        n(g.wait_text('made-in-terminal.txt', 4), 'a file created in the terminal shows in the tree')
        g.quit()
    finally:
        g.kill()

    g = Goat(sb, ['.'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        g.send(alt('e'), 0.5)
        hit = [l for l in g.stderr().splitlines() if 'single-folder' in l]
        n(not hit, f'goatedit . : no warning {hit[:1]}')
        n(g.wait_text('file b', 3), 'goatedit . : the session restores (b.txt open)')
        n(g.quit(), 'quits')
    finally:
        g.kill()


CMAKE_SAMPLE = """cmake_minimum_required(VERSION 3.28)
project(demo VERSION 1.0)
ADD_EXECUTABLE(app main.cpp)
add_library(lib STATIC a.cpp)
target_link_libraries(app PUBLIC lib)
find_package(Threads REQUIRED)
set(public_name x)
if(WIN32)
  foreach(item IN LISTS ITEMS)
    message(STATUS "item ${item} in ${PROJECT_NAME}")
  endforeach()
endif()
function(do_it arg)
  set(${arg}_${SUFFIX} ON)
  set(NESTED ${A_${B}})
  option(USE_X "use x" OFF)
endfunction()
target_compile_definitions(app PRIVATE $<$<CONFIG:Debug>:DBG=1>)
#[[ a block
comment over lines ]]
# plain comment
"""


@check('#11 CMake highlighting')
def cmake(n):
    sb, work = sandbox('cmake')
    write(os.path.join(work, 'CMakeLists.txt'), CMAKE_SAMPLE)
    write(os.path.join(work, 'extra.cmake'), 'add_subdirectory(sub)\nIF(UNIX)\nENDIF()\n')
    g = Goat(sb, ['CMakeLists.txt', 'extra.cmake'], cwd=work, rows=40, cols=140)
    try:
        g.ready()
        g.send(alt('e'), 0.6)
        lines = g.screen.lines()

        def row_of(text):
            for r, t in enumerate(lines):
                if '│' in t and text in t:
                    return r
            return None

        def col(text, word, nth=0):
            r = row_of(text)
            if r is None:
                return None
            line = ''.join(c for c, _, _ in g.screen.cells[r])
            start = line.find('│') + 1
            pos = start
            for _ in range(nth + 1):
                pos = line.find(word, pos)
                if pos < 0:
                    return None
                pos += 1
            return g.screen.cells[r][pos - 1][1]

        blue = col('cmake_minimum_required', 'cmake_minimum_required')
        default = col('project(demo', 'demo')
        c = {
            'cmake_minimum_required': blue,
            'ADD_EXECUTABLE (upper)': col('ADD_EXECUTABLE', 'ADD_EXECUTABLE'),
            'add_library': col('add_library', 'add_library'),
            'if': col('if(WIN32)', 'if'),
            'foreach': col('foreach(item', 'foreach'),
            'function': col('function(do_it', 'function'),
            'endforeach': col('endforeach', 'endforeach'),
            'PUBLIC': col('target_link_libraries', 'PUBLIC'),
            'REQUIRED': col('find_package', 'REQUIRED'),
            'VERSION': col('cmake_minimum_required', 'VERSION'),
            'public_name (lower)': col('set(public_name', 'public_name'),
            'string': col('message(STATUS', '"item'),
            '${item} in string': col('message(STATUS', '${item}'),
            '${arg}': col('set(${arg}', '${arg}'),
            '${A_${B}} nested': col('set(NESTED', '${A_'),
            'inner ${B}': col('set(NESTED', '${B}'),
            '$<...>': col('target_compile_definitions', '$<'),
            'ON': col('set(${arg}', 'ON'),
            'OFF': col('option(USE_X', 'OFF'),
            '#[[ first line': col('#[[ a block', '#[['),
            'block comment 2nd line': col('comment over lines', 'comment'),
            '# comment': col('# plain comment', '#'),
            'default text (demo)': default,
        }
        for k, v in c.items():
            n.info(f'{k:24s} {v}')
        red = c['VERSION']
        n(blue == '6699cc', f'commands blue (cmake_minimum_required {blue})')
        n(c['ADD_EXECUTABLE (upper)'] == blue and c['add_library'] == blue, 'commands blue in any case (ADD_EXECUTABLE, add_library)')
        kw = {c['if'], c['foreach'], c['function'], c['endforeach']}
        n(len(kw) == 1 and None not in kw and blue not in kw and default not in kw, f'if/foreach/function/endforeach as keywords (one colour, not command blue): {kw}')
        n(red == 'ec5f66' and c['PUBLIC'] == red and c['REQUIRED'] == red, f'PUBLIC / REQUIRED / VERSION red ({c["PUBLIC"]}, {c["REQUIRED"]}, {red})')
        n(c['public_name (lower)'] != red, f'public_name (lower case) not red ({c["public_name (lower)"]})')
        var = c['${arg}']
        n(var not in (None, default), f'${{VAR}} coloured ({var})')
        n(c['${item} in string'] == var and c['string'] not in (var, None), f'${{VAR}} inside a string coloured as a variable ({c["${item} in string"]} vs string {c["string"]})')
        n(c['${A_${B}} nested'] == var and c['inner ${B}'] == var, f'nested ${{A_${{B}}}} coloured ({c["${A_${B}} nested"]}, inner {c["inner ${B}"]})')
        n(c['$<...>'] not in (None, default), f'$<...> coloured ({c["$<...>"]})')
        com = c['# comment']
        n(c['#[[ first line'] == com and c['block comment 2nd line'] == com, f'#[[ ]] comment over lines ({c["#[[ first line"]}, 2nd line {c["block comment 2nd line"]}, # {com})')
        n(c['ON'] not in (None, default) and c['ON'] == c['OFF'], f'ON / OFF coloured ({c["ON"]}, {c["OFF"]})')
        # the .cmake file
        g.send(ESC + b'\\', 0.8)
        r = g.screen.find('add_subdirectory')
        n(r is not None and g.screen.cells[r[0]][r[1]][1] == blue, f'a *.cmake file highlights too (add_subdirectory {g.screen.cells[r[0]][r[1]][1] if r else None})')
        r = g.screen.find('IF(UNIX)')
        n(r is not None and g.screen.cells[r[0]][r[1]][1] in kw, 'IF (upper case) as keyword in *.cmake')
        # .ll
        g.quick('.ll', 1.0)
        ll = [t for t in g.screen.lines() if t.strip().startswith('cmake')]
        n(any('CMakeLists.txt' in t for t in ll), f'.ll lists cmake with CMakeLists.txt: {[t.strip()[:60] for t in ll[:1]]}')
        # Ctrl+/ comments with #
        g.send(alt('e'), 0.4)
        g.send(ESC + b'\'', 0.8)        # back to CMakeLists.txt
        g.send(ctrl('/'), 0.6)
        first = [t for r, num, t in g.editor_rows() if num == 1]
        n(bool(first) and first[0].startswith('#') and 'cmake_minimum_required' in first[0], f'Ctrl+/ comments with #: {first[:1]}')
        g.send(ctrl('/'), 0.6)
        first = [t for r, num, t in g.editor_rows() if num == 1]
        n(bool(first) and first[0].startswith('cmake_minimum_required'), f'Ctrl+/ again uncomments: {first[:1]}')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#35 Short-file scroll window')
def short_scroll(n):
    sb, work = sandbox('short_scroll')
    g = Goat(sb, ['new.txt'], cwd=work, rows=40)
    try:
        g.ready()
        g.send(alt('e'), 0.5)
        for i in range(1, 13):
            g.type(f'line {i}', per_key=0.01, wait=0.02)
            if i < 12:
                g.send(ENTER, 0.05)
        g.pump(0.3)
        rows = g.editor_rows()
        top0 = next((num for r, num, t in rows if num is not None), None)
        n(top0 == 1, f'a dozen lines: line 1 at the top ({top0})')
        g.send(PGDN, 0.4)
        rows = g.editor_rows()
        top1 = next((num for r, num, t in rows if num is not None), None)
        n.info(f'after PageDown: top line {top1}, cursor l: {g.cursor_line()}')
        for _ in range(6):
            g.send(UP, 0.05)
        g.send(END, 0.1)
        tops = []
        for i in range(10):
            g.send(ENTER, 0.15)
            rows = g.editor_rows()
            tops.append(next((num for r, num, t in rows if num is not None), None))
        n.info(f'top line after each Enter: {tops}')
        n(all(t == 1 for t in tops), 'Enter in the middle: the top line stays (view not full yet)')
        n(g.quit(), 'quits')
    finally:
        g.kill()



def repo_root():
    return os.path.abspath(os.path.join(os.path.dirname(Goat.EXE), '..'))


@check('#24 Stale assets: install.sh --user')
def install_stale(n):
    if platform.system() != 'Linux':
        n.skip('install.sh is the Linux install')
    sb, _ = sandbox('install_stale')
    home = os.path.join(sb, 'home')
    env = dict(os.environ, HOME=home)
    build = os.path.dirname(Goat.EXE)
    script = os.path.join(repo_root(), 'scripts', 'install.sh')
    r = subprocess.run([script, '--user', '--build-dir', build], env=env, capture_output=True, text=True)
    n(r.returncode == 0, f'install --user into a sandbox HOME (exit {r.returncode})')
    assets = os.path.join(home, '.local', 'share', 'goatedit')
    manifest = os.path.join(assets, 'install_manifest.txt')
    stale = os.path.join(assets, 'zz_removed_asset.yml')
    # what a previous install shipped and this build no longer has: on disk and in the manifest
    write(stale, 'x: 1\n')
    with open(manifest, 'a') as f:
        f.write('\n' + stale + '\n')
    r = subprocess.run([script, '--user', '--build-dir', build], env=env, capture_output=True, text=True)
    n(r.returncode == 0, f'install again (exit {r.returncode})')
    n(not os.path.exists(stale), 'a file the build no longer has is gone from ~/.local/share/goatedit')
    n('zz_removed_asset' not in open(manifest).read(), 'and from the manifest')
    exe = os.path.join(home, '.local', 'bin', 'goatedit')
    r = subprocess.run([exe, '--help'], env=env, capture_output=True, text=True)
    n('cmdline startup options' in (r.stdout + r.stderr), f'the installed goatedit runs (--help prints its options; exit {r.returncode})')


@check('#46 / #49 SDL backend with no display')
def sdl_no_display(n):
    if platform.system() != 'Linux':
        n.skip('Linux only (the no-display fallback is SDL2/X11)')
    sb, work = sandbox('sdl_no_display')
    env = {'HOME': os.path.join(sb, 'home'), 'PATH': '/usr/bin:/bin', 'TERM': 'xterm-256color', 'XDG_SESSION_TYPE': 'x11'}
    p = subprocess.Popen([Goat.EXE, '--backend', 'sdl', '--skip-user-config', '.'], cwd=work, env=env,
                         stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    try:
        out, _ = p.communicate(timeout=6)
        rc = p.returncode
        n(rc == 1 or (rc not in (None,) and rc > 0 and rc < 128), f'fails with a message (exit {rc}): {out.decode(errors="replace").strip()[-200:]!r}')
    except subprocess.TimeoutExpired:
        maps = open(f'/proc/{p.pid}/maps').read()
        driver = 'kmsdrm (gbm + /dev/dri render node)' if 'libgbm' in maps else 'unknown'
        n(False, f'still running after 6 s with no display - SDL picked {driver}, nothing visible')
        p.kill()
        p.communicate()
    # The way out: SIGTERM arrives as SDL_QUIT. A driver asked for by name is let through, so 'offscreen' runs the
    # editor with no display - it used to exit() past the screen's Close and crash in ~SDLFontManager (SIGSEGV).
    p = subprocess.Popen([Goat.EXE, '--backend', 'sdl', '--skip-user-config', '.'], cwd=work,
                         env=dict(env, SDL_VIDEODRIVER='offscreen'),
                         stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(3)
    if p.poll() is not None:
        n(False, f'SDL_VIDEODRIVER=offscreen: still running after 3 s (exit {p.returncode})')
        return
    p.terminate()
    try:
        rc = p.wait(timeout=5)
    except subprocess.TimeoutExpired:
        p.kill()
        rc = p.wait()
    n(rc == 0, f'SIGTERM (SDL_QUIT): quits cleanly (exit {rc}{", SIGSEGV" if rc == -11 else ""})')


@check('#46 In the desktop session: a window')
def desktop_window(n):
    if platform.system() != 'Linux' or shutil.which('xdotool') is None:
        n.skip('needs Linux, X11 and xdotool')
    display = os.environ.get('DISPLAY') or ':0'
    sb, work = sandbox('desktop_window')
    env = dict(os.environ, DISPLAY=display)
    env.pop('WAYLAND_DISPLAY', None)
    p = subprocess.Popen([Goat.EXE, '--skip-user-config', '.'], cwd=work, env=env,
                         stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        wids = []
        for _ in range(20):
            time.sleep(0.25)
            r = subprocess.run(['xdotool', 'search', '--pid', str(p.pid)], env=env, capture_output=True, text=True)
            wids = r.stdout.split()
            if wids:
                break
        mine = []
        for w in wids:
            pid = subprocess.run(['xprop', '-id', w, '_NET_WM_PID'], env=env, capture_output=True, text=True).stdout
            name = subprocess.run(['xdotool', 'getwindowname', w], env=env, capture_output=True, text=True).stdout.strip()
            if pid.strip().endswith(str(p.pid)):
                mine.append((w, name))
        n(bool(mine), f'plain goatedit in the desktop session ({display}) opens a window: {mine}')
        n(p.poll() is None, 'and keeps running (no ANSI fallback, no exit)')
        # SIGTERM arrives as SDL_QUIT - the same way out as closing the window (#49: it crash-dumped)
        p.terminate()
        try:
            rc = p.wait(timeout=5)
        except subprocess.TimeoutExpired:
            rc = None
        n(rc == 0, f'SIGTERM / window close: quits cleanly (exit {rc}{", SIGSEGV" if rc == -11 else ""})')
    finally:
        if p.poll() is None:
            p.kill()
        p.wait()


# ---------------------------------------------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--exe', help='the goatedit binary (e.g. cmake-build-debug/goatedit)')
    ap.add_argument('--list', action='store_true', help='list the checks and the hitlist items they cover')
    ap.add_argument('names', nargs='*')
    args = ap.parse_args()
    if args.list:
        for c in CHECKS:
            print(f'{c.__name__:18s} {c.hitlist}')
        return 0
    if not args.exe:
        ap.error('--exe is required')
    unknown = [name for name in args.names if name not in {c.__name__ for c in CHECKS}]
    if unknown:
        ap.error(f'no such check: {", ".join(unknown)} (see --list)')
    Goat.EXE = os.path.abspath(args.exe)
    selected = [c for c in CHECKS if not args.names or c.__name__ in args.names]
    summary = []
    for fn in selected:
        n = Notes()
        t0 = time.time()
        try:
            fn(n)
        except Skip:
            pass
        except Exception:
            n.append(('FAIL', 'exception: ' + traceback.format_exc().strip().splitlines()[-1]))
            traceback.print_exc()
        dt = time.time() - t0
        verdict = 'FAIL' if not n.ok else 'SKIP' if n.skipped else 'PASS'
        print(f'\n== {verdict}  {fn.__name__}  ({fn.hitlist}, {dt:.1f}s)')
        for s, what in n:
            print(f'   [{s}] {what}')
        summary.append((verdict, fn.__name__))
    print('\n' + ' '.join(f'{v}:{name}' for v, name in summary))
    return 0 if all(v != 'FAIL' for v, _ in summary) else 1


if __name__ == '__main__':
    sys.exit(main())
