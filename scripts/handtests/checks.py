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
import json
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

F8, SHIFT_F8, CTRL_F8 = b'\x1b[19~', b'\x1b[19;2~', b'\x1b[19;5~'
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


def trust_claude(sb, folder):
    """Claude Code's own record that it was trusted in 'folder' (the sandbox HOME's ~/.claude.json) - the editor won't
    start the Editor Agent anywhere else: its trust dialog would be answered by the editor's Enter"""
    write(os.path.join(sb, 'home', '.claude.json'),
          '{"projects": {"%s": {"hasTrustDialogAccepted": true}}}\n' % os.path.realpath(folder))


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


@check('#58 Ctrl+S saves (the Linux keymaps)')
def save_key(n):
    sb, work = sandbox('save_key')
    path = os.path.join(work, 'a.txt')
    write(path, 'hello\n')
    user_config(sb, 'main:\n  autosave_timeout_ms: 0\n')
    g = Goat(sb, ['a.txt'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        g.send(alt('e'), 0.5)
        g.send(END, 0.2)
        g.type('x', wait=0.3)
        n(modified(g), 'after typing: *')
        g.send(ctrl('s'), 0.8)
        n(not modified(g), f'Ctrl+S in the editor: no * ({g.status_line().strip()[:60]})')
        with open(path) as f:
            n(f.read().startswith('hellox'), 'saved to disk')
        # From the terminal panel: still the active buffer
        g.send(alt('e'), 0.3)
        g.type('y', wait=0.3)
        g.send(alt('t'), 0.5)
        g.send(ctrl('s'), 0.8)
        with open(path) as f:
            n(f.read().startswith('helloxy'), 'Ctrl+S with the terminal focused saves the active buffer')
        # Changed on disk: asks, as '.s' does
        time.sleep(1.1)
        write(path, 'other\n')
        g.send(alt('e'), 1.5)
        g.type('z', wait=0.3)
        g.send(ctrl('s'), 0.8)
        n(g.wait_text('changed on disk', 2.0), 'changed on disk: asks first')
        g.send(ESC, 0.5)
        with open(path) as f:
            n(f.read() == 'other\n', 'Cancel leaves the file as the other program wrote it')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#52 Build messages: F8 / Shift+F8 / the list after a build in the terminal')
def build_messages(n):
    if shutil.which('cc') is None:
        n.skip('no cc')
    sb, work = sandbox('build_messages')
    write(os.path.join(work, 'notes.txt'), 'notes\n')
    # The warning in a function of its own: clang leaves out 'unused variable' in a function with errors
    write(os.path.join(work, 'bad.c'), 'int h(void) {\n    int unused;\n    return 0;\n}\n'
                                       'int f(int a) {\n    return a + nope;\n}\nint g(void) { return 1 }\n')
    g = Goat(sb, ['notes.txt'], cwd=work)
    try:
        g.ready()
        g.send(alt('t'), 0.5)
        # GCC also warns that f() (its return an error) ends without one - not this check's warning
        g.type('cc -Wall -Wno-return-type -c bad.c', wait=0.2)
        g.send(ENTER, 0.3)
        # GCC quotes it 'nope' in typographic quotes, clang in plain ones
        n(g.wait_text('nope', 8), 'the compiler ran in the terminal')
        n(g.wait_text('build: 2 errors, 1 warning', 4), 'when it ends: "build: 2 errors, 1 warning" on the status line')

        def step(key, line, what):
            g.send(key, 0.6)
            said = g.status_line().strip()
            # The place: the status message hides the line number while it shows - wait it out
            g.wait_for(lambda g: 'l: ' in g.status_line(), 6)
            n(what in said and g.cursor_line() == line,
              f'{what!r} at line {line} (status "{said[:80]}", then {g.status_line().strip()[-40:]})')

        step(F8, 6, 'error 1/3')         # the first error, not the warning printed before it
        step(F8, 8, 'error 2/3')
        step(F8, 2, 'warning 3/3')       # the warning after the errors
        step(SHIFT_F8, 8, 'error 2/3')
        g.send(CTRL_F8, 0.8)
        listed = g.wait_text('error    bad.c:6:', 3)
        rows = [r for r, t in enumerate(g.screen.lines()) if 'bad.c:' in t and ('error  ' in t or 'warning  ' in t)]
        texts = [g.screen.text(r) for r in rows]
        n(listed and len(rows) == 3 and 'warning' in texts[-1], f'Ctrl+F8: the list - errors, then the warning ({len(rows)} rows)')
        g.send(ESC, 0.4)
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#52 Build messages: trun\'s test failures - F8 to a failed assert')
def build_messages_trun(n):
    header = next((d for d in ('/usr/local/include', '/usr/include', '/opt/homebrew/include')
                   if os.path.exists(os.path.join(d, 'testinterface.h'))), None)
    if shutil.which('trun') is None or header is None or shutil.which('c++') is None:
        n.skip('no trun (or its testinterface.h, or c++)')
    sb, work = sandbox('build_messages_trun')
    write(os.path.join(work, 'notes.txt'), 'notes\n')
    write(os.path.join(work, 'src', 'test_demo.cpp'),
          '#include <testinterface.h>\nextern "C" {\n'
          'int test_demo(ITesting *t) { return kTR_Pass; }\n'
          'int test_demo_ok(ITesting *t) { TR_ASSERT(t, 1 == 1); return kTR_Pass; }\n'
          'int test_demo_assert(ITesting *t) {\n    int a = 1;\n    TR_ASSERT(t, a == 2);\n    return kTR_Pass;\n}\n'
          'int test_demo_error(ITesting *t) {\n    t->Error(__LINE__, __FILE__, "bad %d", 42);\n    return kTR_Fail;\n}\n'
          '}\n')
    lib = 'libdemo.dylib' if platform.system() == 'Darwin' else 'libdemo.so'
    built = subprocess.run(['c++', '-std=c++17', '-shared', '-fPIC', '-I', header, '-o', lib, 'src/test_demo.cpp'],
                           cwd=work, capture_output=True, text=True)
    if built.returncode != 0:
        n.skip(f'the test library didn\'t build: {built.stderr[:200]}')
    g = Goat(sb, ['notes.txt'], cwd=work)
    try:
        g.ready()
        g.send(alt('t'), 0.5)
        g.type(f'trun --sequential ./{lib}', wait=0.2)
        g.send(ENTER, 0.3)
        n(g.wait_text('Tests Failed', 8), 'trun ran in the terminal')
        n(g.wait_text('build: 2 errors, 0 warnings', 4), 'when it ends: "build: 2 errors, 0 warnings" (each failure once, the summary is a repeat)')
        g.send(F8, 0.6)
        said = g.status_line().strip()
        g.wait_for(lambda g: 'l: ' in g.status_line(), 6)
        n('error 1/2: test_demo_assert: a == 2' in said and g.cursor_line() == 7,
          f'F8: the failed assert, at its line (status "{said[:80]}", line {g.cursor_line()})')
        g.send(F8, 0.6)
        said = g.status_line().strip()
        g.wait_for(lambda g: 'l: ' in g.status_line(), 6)
        n('error 2/2: test_demo_error: bad 42' in said and g.cursor_line() == 11, f'F8: the Error() (status "{said[:80]}")')
        g.send(CTRL_F8, 0.8)
        n(g.wait_text('test_demo.cpp:7', 3), 'Ctrl+F8: the list')
        g.send(ESC, 0.4)
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#53 Terminal tabs: new / switch / close in the lower panel, kept across a restart')
def terminal_tabs(n):
    sb, work = sandbox('terminal_tabs')
    write(os.path.join(work, 'a.txt'), 'x\n')
    new_tab, close_tab = b'\x1b[116;6u', b'\x1b[119;6u'     # Ctrl+Shift+T / W (kitty CSI u)

    def tab_row(g):
        return [l for l in g.screen.lines() if 'Shell | Shell 2' in l]

    g = Goat(sb, ['.'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        g.send(alt('t'), 0.5)
        n(not tab_row(g), 'one terminal: no tab row')
        g.type('echo first-tab', wait=0.2)
        g.send(ENTER, 0.8)
        g.send(new_tab, 1.5)
        n(g.wait_for(lambda g: tab_row(g), 3), f'Ctrl+Shift+T: a second tab, the row shows: {tab_row(g)}')
        g.type('echo second-tab', wait=0.2)
        g.send(ENTER, 0.8)
        n(g.wait_text('second-tab', 2) and g.screen.find('first-tab') is None, 'the new one is shown, and focused (typing went there)')
        g.send(alt('1'), 0.8)
        n(g.wait_text('first-tab', 2) and g.screen.find('second-tab') is None, 'Alt+1: the first tab again')
        g.send(alt('2'), 0.8)
        n(g.wait_text('second-tab', 2), 'Alt+2: the second')
        n(g.quit(), 'quits')
    finally:
        g.kill()
    sessions = [os.path.join(dp, f) for dp, _, fs in os.walk(os.path.join(work, '.goatedit')) for f in fs if f.endswith('.yml')]
    text = ''.join(open(f).read() for f in sessions)
    n('Shell 2' in text and 'activeTab: 1' in text, f'the session has both tabs, the second shown ({sessions})')

    g = Goat(sb, ['.'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        n(g.wait_for(lambda g: tab_row(g), 3), 'restarted: both tabs are back')
        g.send(alt('t'), 0.5)
        g.type('sleep 30', wait=0.2)
        g.send(ENTER, 1.0)
        g.send(close_tab, 1.0)
        n(g.wait_text('A job is running', 2), 'Ctrl+Shift+W with a job running: asks')
        g.send(b'k', 0.8)
        n(tab_row(g), 'Keep it: still two tabs')
        g.send(close_tab, 1.0)
        g.wait_text('A job is running', 2)
        g.send(b'c', 1.5)
        n(g.wait_for(lambda g: not tab_row(g), 3), 'Close: one tab left, no row')
        g.send(close_tab, 0.8)
        n(g.wait_text('The last terminal stays', 2), 'the last one stays')
        g.type('echo still-here', wait=0.2)
        g.send(ENTER, 0.8)
        n(g.wait_text('still-here', 2), 'and still works')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#53 step 4 Configured tabs: panel.tabs, a section that inherits terminal, the promote target')
def agent_tab(n):
    sb, work = sandbox('agent_tab')
    write(os.path.join(work, 'p.sh'), 'echo promoted-$((40+2))\n')
    # A bootstrap's output isn't shown (the prompt comes after it) - what it ran is seen by the files it touches
    user_config(sb, 'panel:\n'
                    '  tabs:\n'
                    '    - name: Shell\n'
                    '    - name: Agent\n'
                    '      config: agent_test\n'
                    'agent_test:\n'
                    '  inherit: terminal\n'
                    '  bootstrap: [ "touch agent-ran" ]\n'
                    '  promote_target: yes\n')
    # A project config never runs a command - in any section
    write(os.path.join(work, '.goatedit', 'config.yml'), 'agent_test:\n  bootstrap: [ "touch project-ran" ]\n')

    def tab_row(g):
        return [l for l in g.screen.lines() if 'Shell | Agent' in l]

    # The folder alone - a project (#23): its config is read, its session written
    g = Goat(sb, ['.'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        n(g.wait_for(lambda g: tab_row(g), 3), f'two configured tabs: {tab_row(g)}')
        n(g.wait_for(lambda g: os.path.exists(os.path.join(work, 'agent-ran')), 3), "the Agent tab's own bootstrap ran")
        n(not os.path.exists(os.path.join(work, 'project-ran')), "the project config's bootstrap did not")
        g.send(alt('t'), 0.5)
        g.type('.o p.sh', wait=0.2)
        g.send(ENTER, 1.0)
        g.send(alt('t'), 0.5)
        g.send(alt('2'), 0.8)
        g.type('echo in-$((3*3))-agent', wait=0.2)
        g.send(ENTER, 0.8)
        n(g.wait_text('in-9-agent', 2), 'Alt+2: the Agent tab')
        g.send(alt('1'), 0.8)
        n(g.wait_for(lambda g: g.screen.find('in-9-agent') is None, 2), 'Alt+1: the Shell tab')
        g.send(alt('e'), 0.5)
        g.send(ctrl('enter'), 0.3)
        n(g.wait_text('promoted-42', 4) and g.screen.find('in-9-agent') is not None,
          'Cmd/Ctrl+Enter from the editor: runs in the Agent tab (promote_target), which is shown')
        n(g.quit(), 'quits')
    finally:
        g.kill()
    sessions = [os.path.join(dp, f) for dp, _, fs in os.walk(os.path.join(work, '.goatedit')) for f in fs if f.startswith('session')]
    text = ''.join(open(f).read() for f in sessions)
    n('config: agent_test' in text, f"the session keeps the tab's section ({sessions})")


@check('#77 Landscape: the right panel (panel_right) beside the editor, Alt+T between the panels, homes kept in portrait')
def right_panel(n):
    sb, work = sandbox('right_panel')
    write(os.path.join(work, 'p.sh'), 'echo promoted-$((40+2))\n')
    config = ('main:\n'
              '  layout: {layout}\n'
              'panel:\n'
              '  tabs:\n'
              '    - name: Shell\n'
              'panel_right:\n'
              '  tabs:\n'
              '    - name: Agent\n'
              '      config: agent_test\n'
              'agent_test:\n'
              '  inherit: terminal\n'
              '  promote_target: yes\n')
    user_config(sb, config.format(layout='landscape'))

    def status_row(g):
        return next((r for r in range(g.rows) if 'GoatEdit v' in g.screen.text(r)), -1)

    g = Goat(sb, ['.'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        # From the tree: the bottom panel (the one focused last - none yet)
        g.send(alt('t'), 0.5)
        g.type('echo bottom-$((2*3))', wait=0.2)
        g.send(ENTER, 0.8)
        g.send(alt('t'), 0.5)
        g.type('echo right-$((4*4))', wait=0.2)
        g.send(ENTER, 0.8)
        n(g.wait_text('bottom-6', 2) and g.wait_text('right-16', 2), 'both panels shown at once, Alt+T went from one to the other')
        bottom, right, row = g.screen.find('bottom-6'), g.screen.find('right-16'), status_row(g)
        n(right is not None and right[1] >= g.cols // 2 and right[0] < row,
          f'the right panel is on the right, above the status line (at {right}, status row {row})')
        n(bottom is not None and bottom[0] > row and bottom[1] < 10, f'the bottom panel below it (at {bottom})')
        # Out to the editor and back: Alt+T goes to the panel focused last (the right one)
        g.send(alt('e'), 0.4)
        g.send(alt('t'), 0.5)
        g.type('echo again-$((5*5))', wait=0.2)
        g.send(ENTER, 0.8)
        again = g.screen.find('again-25')
        n(again is not None and again[1] >= g.cols // 2, f'Alt+T from the editor: the panel focused last ({again})')
        # Promote: the right panel's target
        g.send(alt('t'), 0.5)
        g.type('.o p.sh', wait=0.2)
        g.send(ENTER, 1.0)
        g.send(alt('e'), 0.4)
        g.send(ctrl('enter'), 0.3)
        n(g.wait_text('promoted-42', 4) and g.screen.find('promoted-42')[1] >= g.cols // 2,
          'Cmd/Ctrl+Enter from the editor: runs in the right panel (promote_target)')
        n(g.quit(), 'quits')
    finally:
        g.kill()
    sessions = [os.path.join(dp, f) for dp, _, fs in os.walk(os.path.join(work, '.goatedit')) for f in fs if f.startswith('session')]
    text = ''.join(open(f).read() for f in sessions)
    n('home: right' in text and 'split.panel_right' in text, f'the session keeps the tab\'s home and the right divide ({sessions})')

    # Portrait: the same session - the Agent tab joins the bottom panel, still homed right
    user_config(sb, config.format(layout='portrait'))
    g = Goat(sb, ['.'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        n(g.wait_for(lambda g: any('Shell | Agent' in l for l in g.screen.lines()), 3), 'portrait: both tabs in the bottom panel')
        n(g.quit(), 'quits')
    finally:
        g.kill()
    text = ''.join(open(f).read() for f in sessions)
    n('home: right' in text, 'still homed right after a portrait run')

    # Landscape again: back on the right
    user_config(sb, config.format(layout='landscape'))
    g = Goat(sb, ['.'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        n(g.wait_for(lambda g: not any('Shell | Agent' in l for l in g.screen.lines()), 3), 'landscape again: the Agent tab is back on the right')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#77 Layout while running: .layout portrait / landscape keeps the shells, .panel move')
def right_panel_live(n):
    sb, work = sandbox('right_panel_live')
    user_config(sb, 'main:\n'
                    '  layout: landscape\n'
                    'panel:\n'
                    '  tabs:\n'
                    '    - name: Shell\n'
                    'panel_right:\n'
                    '  tabs:\n'
                    '    - name: Agent\n')

    def tab_row(g):
        return [l for l in g.screen.lines() if 'Shell | Agent' in l]

    g = Goat(sb, ['.'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        g.send(alt('t'), 0.4)
        g.send(alt('t'), 0.4)
        g.type('echo kept-$((7*6))', wait=0.2)
        g.send(ENTER, 0.8)
        kept = g.screen.find('kept-42') if g.wait_text('kept-42', 2) else None
        n(kept is not None and kept[1] >= g.cols // 2, f'typed in the right panel ({kept})')
        g.type('.layout portrait', wait=0.2)
        g.send(ENTER, 1.0)
        n(g.wait_for(lambda g: tab_row(g), 3), f'.layout portrait: the Agent tab joins the bottom panel ({tab_row(g)})')
        g.send(alt('t'), 0.4)
        g.send(alt('2'), 0.8)
        n(g.wait_text('kept-42', 2), 'its shell lived on - the output is still there')
        g.type('.layout landscape', wait=0.2)
        g.send(ENTER, 1.0)
        n(g.wait_for(lambda g: not tab_row(g), 3), '.layout landscape: back on the right')
        kept = g.screen.find('kept-42')
        n(kept is not None and kept[1] >= g.cols // 2, f'with its output ({kept})')
        # The bottom panel was used last (.layout was typed there): Alt+T goes there, again to the right
        g.send(alt('e'), 0.3)
        g.send(alt('t'), 0.4)
        g.send(alt('t'), 0.4)
        g.type('.panel move', wait=0.2)
        g.send(ENTER, 1.0)
        n(g.wait_for(lambda g: tab_row(g), 3), '.panel move from the right panel: the tab moves to the bottom, the right panel goes')
        n(g.quit(), 'quits')
    finally:
        g.kill()
    sessions = [os.path.join(dp, f) for dp, _, fs in os.walk(os.path.join(work, '.goatedit')) for f in fs if f.startswith('session')]
    text = ''.join(open(f).read() for f in sessions)
    n('home: right' not in text and 'Agent' in text, 'the move is kept: the Agent tab is homed at the bottom now')


@check('#77 main.layout: auto - the window\'s shape decides, while it is resized')
def right_panel_auto(n):
    sb, work = sandbox('right_panel_auto')
    user_config(sb, 'main:\n'
                    '  layout: auto\n'
                    'panel_right:\n'
                    '  tabs:\n'
                    '    - name: Agent\n')

    def tab_row(g):
        return [l for l in g.screen.lines() if 'Shell | Agent' in l]

    # 160x40 cells is 2:1 - landscape
    g = Goat(sb, ['.'], cwd=work, skip_user_config=False, rows=40, cols=160)
    try:
        g.ready()
        g.send(alt('t'), 0.4)
        g.send(alt('t'), 0.4)
        g.type('echo auto-$((6*7))', wait=0.2)
        g.send(ENTER, 0.8)
        found = g.screen.find('auto-42') if g.wait_text('auto-42', 2) else None
        n(found is not None and found[1] >= g.cols // 2, f'a wide window starts landscape ({found})')
        # 90x45 is 1:1 - portrait
        g.resize(45, 90, 1.5)
        n(g.wait_for(lambda g: tab_row(g), 3), f'made narrow: portrait, the Agent tab in the bottom panel ({tab_row(g)})')
        g.resize(40, 160, 1.5)
        n(g.wait_for(lambda g: not tab_row(g) and 'GoatEdit v' in g.status_line(), 3), 'wide again: landscape')
        found = g.screen.find('auto-42')
        n(found is not None and found[1] >= g.cols // 2, f'the agent\'s shell lived through it ({found})')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#78 Agent integration: main.ai_integration none (default) / claude, never from a project config; .agent')
def agent_integration(n):
    sb, work = sandbox('agent_integration')

    def agent_lines(g):
        g.send(alt('t'), 0.4)
        g.type('.agent', wait=0.2)
        g.send(ENTER, 1.0)
        g.wait_text('Agent: ', 2)
        return '\n'.join(g.screen.lines())

    # Default: none - and a project config naming one is not used
    write(os.path.join(work, '.goatedit', 'config.yml'), 'main:\n  ai_integration: claude\n')
    g = Goat(sb, ['.'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        text = agent_lines(g)
        n('Agent: none' in text, 'default (and a project config saying claude): none')
        n('Question agent' not in text, 'none: nothing that would be started')
        n(g.quit(), 'quits')
    finally:
        g.kill()

    # The user config picks claude: the commands for this project, nothing started
    user_config(sb, 'main:\n  ai_integration: claude\n')
    g = Goat(sb, ['.'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        text = agent_lines(g)
        n('Agent: claude' in text, 'user config: claude')
        n('claude --permission-mode dontAsk' in text, 'the question agent is read-only (dontAsk, answers only)')
        n('-work' in text and 'projects' in text, "its transcript folder, named after the project's path")
        n(g.quit(), 'quits')
    finally:
        g.kill()

    # A name there is none of: a start-up notice, none
    user_config(sb, 'main:\n  ai_integration: hal9000\n')
    g = Goat(sb, ['.'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        n(g.wait_text("no agent 'hal9000'", 3), 'an unknown name: a notice in the terminal')
        n('Agent: none' in agent_lines(g), 'and none')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#78 Editor Agent: .agent start - its own tab at the bottom, fixed name, banner, not saved')
def editor_agent(n):
    sb, work = sandbox('editor_agent')
    # A fake 'claude' - first on a PATH that can't reach the real one (agent calls cost money); sh, so no login
    # profile reorders the PATH. It goes full screen and clears it, as the real one does
    fake_bin = os.path.join(sb, 'bin')
    write(os.path.join(fake_bin, 'claude'), '#!/bin/sh\nprintf \'\\033[?1049h\\033[2J\\033[H\'\n'
                                            'echo "fake-claude: $*"\nexec cat\n')
    os.chmod(os.path.join(fake_bin, 'claude'), 0o755)
    env = {'PATH': fake_bin + ':/usr/bin:/bin'}
    user_config(sb, "main:\n  ai_integration: claude\nterminal:\n  shell: /bin/sh\n")
    trust_claude(sb, work)

    def tab_row(g):
        return next((l for l in g.screen.lines() if 'Editor Agent' in l and '|' in l), '')

    g = Goat(sb, ['.'], cwd=work, skip_user_config=False, env=env)
    try:
        g.ready()
        g.send(alt('t'), 0.4)
        g.type('.agent start', wait=0.2)
        g.send(ENTER, 1.5)
        n(g.wait_text('fake-claude: --permission-mode dontAsk', 4), 'the agent runs read-only (dontAsk), in its own tab')
        n(g.wait_text("GoatEdit's own agent", 2), 'the banner says whose it is - above the agent, which cleared its screen')
        n('Shell | Editor Agent' in tab_row(g), f'a tab named Editor Agent next to the shell ({tab_row(g)!r})')
        # Back to the shell tab: the editor's answer landed there, not in the agent's input
        g.send(alt('1'), 0.5)
        n(g.wait_text('Editor Agent started', 2), "'.agent start' answered in the shell, not in the agent's tab")
        g.type('.agent start', wait=0.2)
        g.send(ENTER, 1.0)
        n(g.wait_text('fake-claude', 2), 'a second .agent start shows the running one')
        n(tab_row(g).count('Editor Agent') == 1, 'still one Editor Agent tab')
        g.send(alt('1'), 0.5)
        n(g.wait_text('runs already', 2), '... and says so')
        n(g.quit(), 'quits')
    finally:
        g.kill()
    sessions = [os.path.join(dp, f) for dp, _, fs in os.walk(os.path.join(work, '.goatedit')) for f in fs if f.startswith('session')]
    text = ''.join(open(f).read() for f in sessions)
    n('Editor Agent' not in text and 'view: editor_agent' not in text, f'the Editor Agent is not in the session ({sessions})')


@check('#78 step 2 .ask: consent, the agent started, the prompt typed in once it settles, the answer opened')
def editor_agent_ask(n):
    sb, work = sandbox('editor_agent_ask')
    # A fake 'claude' (agent calls cost money): reads pastes (?2004), and for a line naming its answer file writes it -
    # as the real one is asked to. It notes its arguments where the check can read them.
    fake_bin = os.path.join(sb, 'bin')
    args_file = os.path.join(sb, 'fake-claude-args')
    write(os.path.join(fake_bin, 'claude'), '#!/bin/sh\n'
          f'echo "$*" > \'{args_file}\'\n'
          'echo "fake-claude: $*"\n'
          'printf \'\\033[?2004h> \'\n'
          'while IFS= read -r line; do\n'
          '  case "$line" in\n'
          '    *"to the file "*)\n'
          '      f=$(printf "%s" "$line" | sed -e "s/.*to the file //" -e "s/ - create or replace.*//")\n'
          '      printf "# Fake answer\\n\\nforty-two\\n" > "$f"\n'
          '      echo "fake-claude wrote it"\n'
          '      printf "> ";;\n'
          '  esac\n'
          'done\n')
    os.chmod(os.path.join(fake_bin, 'claude'), 0o755)
    env = {'PATH': fake_bin + ':/usr/bin:/bin'}
    user_config(sb, "main:\n  ai_integration: claude\n  ai_settle_ms: 500\nterminal:\n  shell: /bin/sh\n")
    qa = os.path.join(work, '.goatedit', 'agent_qa')

    def qa_files(prefix):
        return sorted(f for f in os.listdir(qa) if f.startswith(prefix)) if os.path.isdir(qa) else []

    g = Goat(sb, ['.'], cwd=work, skip_user_config=False, env=env)
    try:
        g.ready()
        g.send(alt('t'), 0.4)
        # A folder Claude hasn't been trusted in: refused - its trust dialog would get the editor's Enter
        g.type('.ask what is this', wait=0.2)
        g.send(ENTER, 1.0)
        n(g.wait_text("isn't trusted", 3), "an untrusted folder: refused, no modal")
        n(not os.path.exists(args_file), '... and nothing started')
        trust_claude(sb, work)
        # Declined: nothing written, nothing started
        g.type('.ask what is this', wait=0.2)
        g.send(ENTER, 1.0)
        n(g.wait_text('To the Editor Agent', 3), 'a consent modal')
        n(g.wait_text('it is started first', 1), '... saying the agent is started first')
        g.send(b'c', 0.8)
        n(g.wait_text('Not asked', 2), 'Cancel: not asked')
        n(qa_files('q_') == [] and not os.path.exists(args_file), 'nothing written, no agent started')

        g.type('.ask why is the sky blue', wait=0.2)
        g.send(ENTER, 1.0)
        n(g.wait_text('To the Editor Agent', 3), 'the modal again')
        text = '\n'.join(g.screen.lines())
        n('why is the sky blue' in text and 'no other file' in text, 'it shows the prompt, word for word')
        g.send(b's', 0.5)
        n(g.wait_text('forty-two', 12), 'the answer is opened')
        n(g.wait_text('EA: idle', 4), f"the status line: EA: idle ({[l for l in g.screen.lines() if 'EA:' in l]})")
        qs, ans = qa_files('q_'), qa_files('a_')
        n(len(qs) == 1 and len(ans) == 1 and qs[0][2:] == ans[0][2:], f'one q_/a_ pair with one id ({qs}, {ans})')
        qtext = open(os.path.join(qa, qs[0])).read() if qs else ''
        args = open(args_file).read() if os.path.exists(args_file) else ''
        n('# Question\n\nwhy is the sky blue' in qtext, 'the question file holds the question')
        session = next((l.split(': ', 1)[1] for l in qtext.splitlines() if l.startswith('session: ')), '?')
        n(f'--session-id {session}' in args, f'the agent runs in the conversation the question names ({session})')
        n('--permission-mode dontAsk' in args and '/.goatedit/agent_qa/a_*' in args, f'read-only, answers only ({args.strip()})')
        n(g.quit(), 'quits')
    finally:
        g.kill()


def fake_answering_claude(sb):
    """A fake 'claude' (agent calls cost money): reads pastes (?2004), and for a line naming its answer file writes it"""
    fake_bin = os.path.join(sb, 'bin')
    write(os.path.join(fake_bin, 'claude'), '#!/bin/sh\n'
          'echo "fake-claude: $*"\n'
          'printf \'\\033[?2004h> \'\n'
          'while IFS= read -r line; do\n'
          '  case "$line" in\n'
          '    *"to the file "*)\n'
          '      f=$(printf "%s" "$line" | sed -e "s/.*to the file //" -e "s/ - create or replace.*//")\n'
          '      printf "# Fake answer\\n\\nforty-two\\n" > "$f"\n'
          '      echo "fake-claude wrote it"\n'
          '      printf "> ";;\n'
          '  esac\n'
          'done\n')
    os.chmod(os.path.join(fake_bin, 'claude'), 0o755)
    return {'PATH': fake_bin + ':/usr/bin:/bin'}


@check('#78 step 3 .ask <template>: the tools run first, their output in the prompt you OK; list, a failing tool')
def editor_agent_template(n):
    sb, work = sandbox('editor_agent_template')
    env = fake_answering_claude(sb)
    user_config(sb, "main:\n  ai_integration: claude\n  ai_settle_ms: 500\nterminal:\n  shell: /bin/sh\n")
    trust_claude(sb, work)
    # A repository with a reason on record: the commit names #42
    write(os.path.join(work, 'hello.c'), 'int main() {\n    return 0;\n}\n')
    git(work, 'init', '-q', '-b', 'main')
    git(work, 'add', 'hello.c')
    git(work, 'commit', '-q', '-m', '#42 hello: the smallest program that builds\n\nKept as the build smoke test.')
    # A user template whose tool fails, and a user tool that shadows nothing
    user_dir = os.path.join(sb, 'home', '.config', 'goatedit')
    write(os.path.join(user_dir, 'tools', 'broken.py'), 'import sys\nprint("it broke", file=sys.stderr)\nsys.exit(3)\n')
    write(os.path.join(user_dir, 'questions', 'broken.md'), '---\nname: broken\ntools:\n  out: broken.py\n---\n{tool:out}\n')
    qa = os.path.join(work, '.goatedit', 'agent_qa')

    def qa_files(prefix):
        return sorted(f for f in os.listdir(qa) if f.startswith(prefix)) if os.path.isdir(qa) else []

    g = Goat(sb, ['hello.c'], cwd=work, skip_user_config=False, env=env)
    try:
        g.ready()
        g.send(alt('t'), 0.4)
        g.type('.ask list', wait=0.2)
        g.send(ENTER, 1.0)
        n(g.wait_text('Why is this line here?', 3), '.ask list: why')
        n(g.wait_text('structure [base]', 1), '... structure, with its optional base')
        n(g.wait_text('broken', 1), "... and the user's own")

        # The failing tool: a notice, no modal, nothing written
        g.type('.ask broken', wait=0.2)
        g.send(ENTER, 1.0)
        n(g.wait_text('broken.py failed (exit 3): it broke', 4), 'a failing tool: what it said, nothing asked')
        n(qa_files('q_') == [], '... nothing written')

        # More words than the template takes: a free question, no tools
        g.type('.ask why is the sky blue', wait=0.2)
        g.send(ENTER, 1.0)
        n(g.wait_text('To the Editor Agent', 3), "'.ask why is the sky blue': the free-text modal")
        n('why is the sky blue' in '\n'.join(g.screen.lines()), '... with the question as typed')
        g.send(b'c', 0.8)

        # 'why' on line 1 of hello.c: the chain gathered, shown in the modal, sent, answered
        g.type('.ask why', wait=0.2)
        g.send(ENTER, 0.5)
        n(g.wait_text('To the Editor Agent', 6), "'.ask why': the modal, once the tool has run")
        text = '\n'.join(g.screen.lines())
        n('Why is line 1 of `hello.c` here?' in text, 'the prompt names the caret\'s file and line')
        n('#42 hello: the smallest program' in text, "the tool's output is in it - the commit and its #N")
        g.send(b's', 0.5)
        n(g.wait_text('forty-two', 12), 'the answer is opened')
        qs = qa_files('q_')
        qtext = open(os.path.join(qa, qs[0])).read() if qs else ''
        n('template: why\nargs: hello.c:1\n' in qtext, f'the question file says which template, about what ({qs})')
        n('Kept as the build smoke test.' in qtext, '... and holds the prompt as sent, the chain included')

        # 'structure': the change in the work tree against the branch point - the scan is the editor's own --dump
        write(os.path.join(work, 'hello.c'), 'struct Greeter { int times; };\nint main() {\n    return 0;\n}\n')
        g.send(alt('t'), 0.4)
        g.type('.ask structure', wait=0.2)
        g.send(ENTER, 0.5)
        n(g.wait_text('To the Editor Agent', 8), "'.ask structure': the modal")
        text = '\n'.join(g.screen.lines())
        n('type-diff' in text and 'the work tree' in text, 'the type-diff against the branch point is in the prompt')
        n('types +1 -0' in text, f"... and it saw the new struct ({[l for l in g.screen.lines() if 'types' in l]})")
        g.send(b'c', 0.8)
        n(g.quit(), 'quits')
    finally:
        g.kill()


# --- The real agent: opt-in only (--real-agent), never in the default run - each question costs the user's tokens ---
REAL_AGENT = {'enabled': False, 'model': ''}
# Automated questions to the real agent: at most this many per hour, over all runs (the user's rule, 2026-11-24)
REAL_AGENT_PER_HOUR = 25
REAL_AGENT_LEDGER = os.path.expanduser('~/.cache/goatedit-handtests/real-agent-ledger')


def real_agent_budget():
    """Questions left this hour - the ledger holds one time per question sent, over all runs"""
    now = time.time()
    try:
        with open(REAL_AGENT_LEDGER) as f:
            times = [float(l) for l in f if l.strip()]
    except OSError:
        times = []
    return REAL_AGENT_PER_HOUR - sum(1 for t in times if now - t < 3600)


def real_agent_spend():
    os.makedirs(os.path.dirname(REAL_AGENT_LEDGER), exist_ok=True)
    with open(REAL_AGENT_LEDGER, 'a') as f:
        f.write(f'{time.time()}\n')


def real_trusted(folder):
    """The user's own Claude Code has been trusted in 'folder' (either spelling of the path)"""
    try:
        with open(os.path.expanduser('~/.claude.json')) as f:
            projects = json.load(f).get('projects', {})
    except (OSError, ValueError):
        return False
    return any(projects.get(p, {}).get('hasTrustDialogAccepted') for p in {folder, os.path.realpath(folder)})


@check('#78 step 2 .ask with the REAL agent (--real-agent): two questions, what each cost - opt-in, 25/hour')
def real_agent_ask(n):
    if not REAL_AGENT['enabled']:
        n.skip('the real agent only with --real-agent (each question costs tokens)')
    questions = ['What does hello.c print? Answer in one line.',
                 'Which function in hello.c runs first? Answer in one line.']
    left = real_agent_budget()
    if left < len(questions):
        n.skip(f'the hour\'s budget is spent: {left} of {REAL_AGENT_PER_HOUR} left ({REAL_AGENT_LEDGER})')
    real = shutil.which('claude') or shutil.which('claude', path=os.path.expanduser('~/.local/bin'))
    if real is None:
        n.skip('no claude installed (or its link dangles - see below)')
    if os.path.realpath(real).startswith(os.path.realpath(tempfile.gettempdir())):
        n.skip(f'{real} points into a temp folder ({os.path.realpath(real)}) - an earlier check re-linked it; '
               f'point it back at ~/.local/share/claude/versions/<version>')
    # The same path every run (the sandbox is wiped, not moved), so the user trusts it once
    sb, work = sandbox('real_agent_ask')
    if not real_trusted(work):
        n.skip(f"Claude Code isn't trusted in {work} - run 'claude' there once, accept, quit; then again")
    write(os.path.join(work, 'hello.c'), '#include <stdio.h>\nint main(void) {\n    printf("hello, goat\\n");\n    return 0;\n}\n')
    # The real claude with the user's HOME (their login and its trust record); the editor's HOME stays the sandbox's,
    # with the user's transcripts linked in so the editor can read what each question cost
    # The sandbox's XDG_* must not reach it: with XDG_DATA_HOME in the sandbox, its self-installer put a copy there and
    # re-linked the user's ~/.local/bin/claude to it - dangling once the sandbox was wiped. The user's own XDG settings
    # (or none), and no auto-update from a check.
    fake_bin = os.path.join(sb, 'bin')
    xdg = ''.join(f"{k}='{os.environ[k]}' " if k in os.environ else f'-u {k} '
                  for k in ('XDG_CONFIG_HOME', 'XDG_STATE_HOME', 'XDG_DATA_HOME', 'XDG_CACHE_HOME'))
    write(os.path.join(fake_bin, 'claude'),
          f'#!/bin/sh\nexec env {xdg}HOME=\'{os.path.expanduser("~")}\' DISABLE_AUTOUPDATER=1 \'{real}\' "$@"\n')
    os.chmod(os.path.join(fake_bin, 'claude'), 0o755)
    os.symlink(os.path.expanduser('~/.claude'), os.path.join(sb, 'home', '.claude'))
    trust_claude(sb, work)
    model = f"\n  ai_model: {REAL_AGENT['model']}" if REAL_AGENT['model'] else ''
    user_config(sb, f"main:\n  ai_integration: claude{model}\nterminal:\n  shell: /bin/sh\n")
    qa = os.path.join(work, '.goatedit', 'agent_qa')
    env = {'PATH': fake_bin + ':/usr/bin:/bin'}

    g = Goat(sb, ['.'], cwd=work, skip_user_config=False, env=env)
    try:
        g.ready()
        g.send(alt('t'), 0.4)
        for i, question in enumerate(questions):
            g.type('.ask ' + question, wait=0.2)
            g.send(ENTER, 1.0)
            if not n(g.wait_text('To the Editor Agent', 3), f'question {i + 1}: the consent modal'):
                break
            real_agent_spend()
            g.send(b's', 0.5)
            # Stop on anything unexpected: no answer within two minutes is reported, not retried
            seen = []

            def answered(g):
                ea = g.status_line().strip()
                ea = ea[ea.rfind('EA:'):] if 'EA:' in ea else '(no EA)'
                if not seen or seen[-1] != ea:
                    seen.append(ea)
                return len([f for f in os.listdir(qa) if f.startswith('a_')]) > i if os.path.isdir(qa) else False
            if not n(g.wait_for(answered, 120), f'question {i + 1}: answered within 2 minutes (status: {" -> ".join(seen)})'):
                # What the agent's tab shows - its own tab, brought to the front
                g.type('.agent start', wait=0.2)
                g.send(ENTER, 1.5)
                print('--- the screen, the agent tab in front:\n' + '\n'.join(l.rstrip() for l in g.screen.lines()))
                break
            n(g.wait_text('Answered - a_', 30), f'question {i + 1}: opened')
            # The cost follows in the same message - read it once the screen has it all, not mid-redraw
            g.wait_text('cached, out', 3)
            status = next((l for l in g.screen.lines() if 'Answered - ' in l), '').strip()
            n('cached' in status, f'question {i + 1}: what it cost: {status}')
            g.send(alt('t'), 0.4)
        n(g.quit(10), 'quits')
    finally:
        g.kill()
    for name in sorted(os.listdir(qa)) if os.path.isdir(qa) else []:
        print(f'--- {name}\n' + open(os.path.join(qa, name)).read())


@check('#53 step 5 Messages tab: the build list kept open in the lower panel')
def messages_tab(n):
    if shutil.which('cc') is None:
        n.skip('no cc')
    sb, work = sandbox('messages_tab')
    write(os.path.join(work, 'notes.txt'), 'notes\n')
    write(os.path.join(work, 'bad.c'), 'int h(void) {\n    int unused;\n    return 0;\n}\n'
                                       'int f(int a) {\n    return a + nope;\n}\nint g(void) { return 1 }\n')
    user_config(sb, 'panel:\n  tabs:\n    - name: Shell\n    - view: messages\n')
    close_tab = b'\x1b[119;6u'     # Ctrl+Shift+W

    def tab_row(g):
        return next((l.strip() for l in g.screen.lines() if l.strip().startswith('Shell | Messages')), None)

    g = Goat(sb, ['notes.txt'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        n(g.wait_for(lambda g: tab_row(g) == 'Shell | Messages', 3), f'a Shell and a Messages tab: {tab_row(g)!r}')
        g.send(alt('t'), 0.5)
        g.type('cc -Wall -Wno-return-type -c bad.c', wait=0.2)
        g.send(ENTER, 0.3)
        n(g.wait_text('build: 2 errors, 1 warning', 8), 'the build ran and ended')
        n(g.wait_for(lambda g: tab_row(g) == 'Shell | Messages (3)', 2), f'the label counts: {tab_row(g)!r}')
        g.send(alt('e'), 0.5)
        g.send(CTRL_F8, 0.8)
        listed = g.wait_text('error    bad.c:6:', 3)
        rows = [t for t in g.screen.lines() if 'bad.c:' in t and ('error  ' in t or 'warning  ' in t)]
        n(listed and len(rows) == 3 and 'warning' in rows[-1],
          f'Ctrl+F8: the Messages tab - errors, then the warning ({len(rows)} rows)')
        n(g.screen.find('Filter:') is None, 'not the modal')
        n(code_under(g, 'error    bad.c:6:', 'return a + nope;'), 'the code it points at on the line under it')
        g.send(DOWN, 0.3)
        g.send(ENTER, 0.6)
        said = g.status_line().strip()
        g.wait_for(lambda g: 'l: ' in g.status_line(), 6)
        # The keys went to the tab - it has the focus
        n('error 2/3' in said and g.cursor_line() == 8,
          f'Down, Enter: the second error, in the editor ("{said[:60]}", line {g.cursor_line()})')
        g.send(F8, 0.6)
        g.wait_for(lambda g: 'l: ' in g.status_line(), 6)
        n(g.cursor_line() == 2, f'F8 goes on from there: the warning (line {g.cursor_line()})')
        g.send(CTRL_F8, 0.8)
        g.send(close_tab, 0.8)
        n(g.wait_for(lambda g: tab_row(g) is None, 2), 'Ctrl+Shift+W in the Messages tab closes it')
        n(g.screen.find('notes') is not None, "and not the editor's file")
        g.send(alt('e'), 0.5)
        g.send(CTRL_F8, 0.8)
        n(g.wait_text('Filter:', 2), 'no Messages tab: Ctrl+F8 shows the list as a modal')
        n(code_under(g, 'error    bad.c:6:', 'return a + nope;'), 'the modal has the code under each message too')
        g.send(ESC, 0.4)
        n(g.quit(), 'quits')
    finally:
        g.kill()


def code_under(g, row_text, code):
    """True when the row after the one holding 'row_text' holds 'code' - a build message's source line"""
    r = g.screen.find(row_text)
    return (r is not None) and (r[0] + 1 < g.rows) and (code in g.screen.text(r[0] + 1))


def gutter_colour(g, line):
    """fg of line 'line''s number in the editor's gutter, None when it isn't shown"""
    for r, num, _ in g.editor_rows():
        if num == line:
            t = g.screen.text(r)
            return g.screen.colour_of(r, str(line), t.find('│'))
    return None


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
        n(g.screen.find('goatedit: theme nosuch.theme.yml: no such theme file') is not None,
          'the terminal panel says it too (#50)')
        lines = [l.rstrip() for l in g.screen.lines() if l.strip()]
        n(lines[-1].endswith('$'), f'the prompt is below it, the last line: {lines[-1:]}')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#51 Console output above the shell prompt')
def prompt_above(n):
    sb, work = sandbox('prompt_above')
    write(os.path.join(work, 'a.txt'), 'x\n')
    # A coloured PS1 where the system's bashrc leaves it (Ubuntu's sets its own) - the prompt is read off the screen
    g = Goat(sb, ['a.txt'], cwd=work, env={'PS1': r'\[\e[32m\]bash\$\[\e[0m\] '})
    try:
        n(g.ready(), 'start-up: the banner, then the prompt below it - no key pressed')
        lines = [l.rstrip() for l in g.screen.lines() if l.strip()]
        prompt = lines[-1] if lines else ''
        n.info(f'the prompt: {prompt!r}')
        g.send(alt('t'), 0.5)
        g.type('echo hal', wait=0.5)
        g.send(alt('e'), 0.5)
        g.quick('.ll', 1.0)
        lines = [l.rstrip() for l in g.screen.lines() if l.strip()]
        n(any('built-in' in l for l in lines), 'the .ll output is in the terminal')
        n(lines and lines[-1] == prompt + ' echo hal', f'the half-typed line stays below it: {lines[-1:]}')
        g.send(alt('t'), 0.5)
        g.type('f', wait=0.3)
        g.send(ENTER, 1.0)
        lines = [l.rstrip() for l in g.screen.lines() if l.strip()]
        n(lines[-2:] == ['half', prompt], f'finished and run: {lines[-3:]}')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#57 A dot-command typed at the prompt: the command, its output below, the next prompt')
def dot_at_prompt(n):
    for shell in ('bash', 'zsh'):
        exe = shutil.which(shell)
        if exe is None:
            n.info(f'{shell}: not installed - skipped')
            continue
        sb, work = sandbox(f'dot_at_prompt_{shell}')
        write(os.path.join(work, 'a.txt'), 'x\n')
        write(os.path.join(sb, 'home', '.zshrc'), "PROMPT='zsh%% '\n")
        g = Goat(sb, ['a.txt'], cwd=work, env={'SHELL': exe, 'PS1': r'\[\e[32m\]bash\$\[\e[0m\] '})
        try:
            g.ready()
            lines = [l.rstrip() for l in g.screen.lines() if l.strip()]
            prompt = lines[-1] if lines else ''
            n.info(f'{shell}: the prompt {prompt!r}')
            g.send(alt('t'), 0.5)
            for _ in range(2):
                g.type('.theme bogus', wait=0.3)
                g.send(ENTER, 1.0)
            # The panel shows the bottom: the end of the first output, the second command, its output, the prompt
            lines = [l.rstrip() for l in g.screen.lines()]
            at = [i for i, l in enumerate(lines) if l == prompt + ' .theme bogus']
            n(len(at) >= 1, f'{shell}: the command stays on its prompt line')
            if at:
                i = at[-1]
                # Or the panel's top row (the status line above it) when the second output fills the panel
                n(lines[i - 1].strip().startswith('--force') or 'GoatEdit v' in lines[i - 1],
                  f'{shell}: right below the last output (or at the panel\'s top): {lines[i - 1:i + 1]}')
                rest = lines[i + 1:]
                while rest and not rest[-1].strip():
                    rest.pop()
                n(rest and rest[0].startswith('use: .theme list') and all(l.strip() for l in rest)
                  and rest[-1] == prompt, f'{shell}: the output below it, then the prompt - no blank rows: {rest}')
            n(g.quit(), f'{shell}: quits')
        finally:
            g.kill()


def bg_at(g, needle):
    """The background behind the first cell of 'needle' on screen, or None"""
    pos = g.screen.find(needle)
    if pos is None:
        return None
    r, c = pos
    return g.screen.cells[r][c][2]


@check('#19 Theme list and use, live')
def theme_use(n):
    sb, work = sandbox('theme_use')
    themes = os.path.join(sb, 'home', '.config', 'goatedit', 'themes')
    write(os.path.join(themes, 'test-red.theme.yml'), 'name: "Test Red"\nauthor: "checks"\nlicense: "MIT"\ncolorfile: "test-red.colors.json"\n')
    write(os.path.join(themes, 'test-red.colors.json'),
          '{ "colors": { "globals": { "background": "rgb(80,0,0)", "foreground": "rgb(220,220,220)" },'
          ' "content": { "regular": "rgb(220,220,220)" } } }')
    user_config(sb, '# my settings\nmain:\n  tabsize: 4\n')
    write(os.path.join(work, 'a.txt'), 'hello theme\n')
    g = Goat(sb, ['a.txt'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        before = bg_at(g, 'hello theme')
        g.quick('.theme list', 1.0)
        n(g.screen.find('Test Red') is not None and g.screen.find('* default theme') is not None,
          '.theme list: the user theme and the shipped default, the current one marked')
        g.quick('.theme use test red', 1.0)
        n(g.wait_text("Using 'Test Red'", 3), '.theme use test red: switched')
        after = bg_at(g, 'hello theme')
        n(after == '500000', f'the editor\'s background follows at once ({before} -> {after}, want 500000)')
        prompt = [l for l in g.screen.lines() if l.rstrip().endswith('$')]
        n(prompt and bg_at(g, prompt[-1].strip()) == '500000', 'and the terminal\'s')
        with open(os.path.join(sb, 'home', '.config', 'goatedit', 'config.yml')) as f:
            text = f.read()
        n('theme: test-red.theme.yml' in text and '# my settings' in text, f'saved in the user config, comments kept: {text!r}')
        n(g.quit(), 'quits')
    finally:
        g.kill()
    g = Goat(sb, ['a.txt'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        n(bg_at(g, 'hello theme') == '500000', 'started again: still the chosen theme')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#59 .theme use at a level: a project\'s own theme wins there, --user asks')
def theme_project(n):
    sb, work = sandbox('theme_project')
    themes = os.path.join(sb, 'home', '.config', 'goatedit', 'themes')
    write(os.path.join(themes, 'test-red.theme.yml'), 'name: "Test Red"\nauthor: "checks"\nlicense: "MIT"\ncolorfile: "test-red.colors.json"\n')
    write(os.path.join(themes, 'test-red.colors.json'),
          '{ "colors": { "globals": { "background": "rgb(80,0,0)", "foreground": "rgb(220,220,220)" },'
          ' "content": { "regular": "rgb(220,220,220)" } } }')
    user_config(sb, '# my settings\nmain:\n  tabsize: 4\n')
    project = os.path.join(work, '.goatedit', 'config.yml')
    write(project, '# project settings\nmain:\n  theme: test-red.theme.yml\n')
    user = os.path.join(sb, 'home', '.config', 'goatedit', 'config.yml')

    def read(path):
        with open(path) as f:
            return f.read()

    def prompt_bg(g):
        prompt = [l for l in g.screen.lines() if l.rstrip().endswith('$')]
        return bg_at(g, prompt[-1].strip()) if prompt else None

    g = Goat(sb, ['.'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        n(prompt_bg(g) == '500000', f'the project\'s theme at start ({prompt_bg(g)})')
        g.quick('.theme list', 1.0)
        n(g.wait_text("from this project's config", 2), '.theme list: main.theme comes from the project config')
        g.quick('.theme use default', 1.0)
        n(g.wait_text('sets its own theme', 2), '.theme use (user): asks - the project sets its own')
        g.send(b'k', 1.0)
        n(g.wait_text('that one stays here', 2), 'Keep: says the project keeps its theme here')
        n('theme: default.theme.yml' in read(user) and '# my settings' in read(user), f'saved in the user config: {read(user)!r}')
        n('theme: test-red.theme.yml' in read(project), 'the project config untouched')
        n(prompt_bg(g) == '500000', 'still the project\'s theme here')
        g.quick('.theme use default', 1.0)
        n(g.wait_text('sets its own theme', 2), 'asks again')
        g.send(b'r', 1.0)
        n(g.wait_text("Using 'default", 2), 'Remove: switched')
        n('theme:' not in read(project) and '# project settings' in read(project),
          f'main.theme taken out of the project config, its comment kept: {read(project)!r}')
        n(prompt_bg(g) not in (None, '500000'), f'the theme follows at once ({prompt_bg(g)})')
        g.quick('.theme list', 1.0)
        n(g.wait_text('from your user config', 2), '.theme list: now from the user config')
        g.quick('.theme use --project test red', 1.0)
        n(g.wait_text("Using 'Test Red'", 2) and not g.screen.find('sets its own theme'), '--project: no question, switched')
        n('theme: test-red.theme.yml' in read(project), 'written to the project config')
        n(prompt_bg(g) == '500000', 'red again')
        n(g.quit(), 'quits')
    finally:
        g.kill()


@check('#20 The generated themes match their sources')
def themes_gen(n):
    script = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'gen-themes.py')
    r = subprocess.run([sys.executable, script, '--exe', Goat.EXE, '--check'], capture_output=True, text=True)
    n(r.returncode == 0, f'Assets/Resources/Themes is what scripts/gen-themes.py writes: {(r.stdout + r.stderr).strip()}')


@check('#20 Dark 2026 (from VS Code)')
def vscode_theme(n):
    sb, work = sandbox('vscode_theme')
    write(os.path.join(work, 'a.txt'), 'hello vscode\n')
    g = Goat(sb, ['a.txt'], cwd=work, skip_user_config=False)
    try:
        g.ready()
        g.quick('.theme list', 1.0)
        n(g.screen.find('Dark 2026 (from VS Code)') is not None, '.theme list shows it (shipped)')
        g.quick('.theme use vscode-dark-2026', 1.0)
        n(g.wait_text("Using 'Dark 2026 (from VS Code)'", 3), 'switched to it')
        n(bg_at(g, 'hello vscode') == '121314', f"the editor's background is the theme's ({bg_at(g, 'hello vscode')}, want 121314)")
        g.send(alt('t'), 0.4)
        g.type("printf '\\033[31mREDTEXT\\033[0m\\n'", wait=0.2)
        g.send(ENTER, 1.0)
        pos = None
        for r in range(g.rows):
            line = g.screen.text(r)
            if line.strip() == 'REDTEXT':
                pos = (r, line.find('REDTEXT'))
        fg = g.screen.cells[pos[0]][pos[1]][1] if pos else None
        n(fg == 'cd3131', f"the terminal's red is VS Code's ({fg}, want cd3131)")
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


def sgr_mouse(kind, col, row):
    """An SGR mouse report (gansi asks for ?1000/?1002/?1006) - col/row 0-based screen cells"""
    code = {'press': 0, 'move': 32, 'release': 0}[kind]
    return f'\x1b[<{code};{col + 1};{row + 1}{"m" if kind == "release" else "M"}'.encode()


@check('#41 Drag to select in the editor')
def drag_select(n):
    sb, work = sandbox('drag_select')
    write(os.path.join(work, 'd.txt'), 'first line here\nsecond line here\nthird line here\nfourth line here\n')
    g = Goat(sb, ['d.txt'], cwd=work)
    try:
        g.ready()
        g.send(alt('e'), 0.5)
        start = g.screen.find('second line')
        end = g.screen.find('third line')
        n(start is not None and end is not None, f'the text is on screen ({start}, {end})')
        if start is None or end is None:
            return
        (r0, c0), (r1, c1) = start, end
        g.send(sgr_mouse('press', c0 + 7, r0), 0.3)
        g.send(sgr_mouse('move', c0 + 9, r0), 0.2)
        g.send(sgr_mouse('move', c1 + 5, r1), 0.2)
        g.send(sgr_mouse('release', c1 + 5, r1), 0.4)
        rows = selected_rows(g)
        n(rows == [r0, r1], f'press on "second", drag to "third": both rows selected ({rows}, want {[r0, r1]})')
        g.send(sgr_mouse('press', c0 + 2, r0), 0.2)
        g.send(sgr_mouse('release', c0 + 2, r0), 0.4)
        n(selected_rows(g) == [], f'a click drops it ({selected_rows(g)})')
        n(g.quit(), 'quits')
    finally:
        g.kill()


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
        n(g.screen.find('goatedit: not a single-folder launch') is not None,
          'the terminal panel says it too (#50 - stderr is lost through goat / a launcher)')
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
    g = Goat(sb, ['CMakeLists.txt', 'extra.cmake'], cwd=work, rows=60, cols=140)   # .ll lists every language - room for all of them in the panel
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


@check('#11 Python: highlighted, ":" indents, a crash in python3 -> F8 to its line')
def python(n):
    sb, work = sandbox('python')
    write(os.path.join(work, 'game.py'), '@dataclass\nclass Player(Sprite):\n    pass\n\n'
                                        'def greet(name):\n    print(f"hi {name}", 42)  # says hi\n')
    write(os.path.join(work, 'crash.py'), 'def divide(a, b):\n    return a / b\n\nprint(divide(1, 0))\n')
    g = Goat(sb, ['game.py'], cwd=work)
    try:
        g.ready()
        g.send(alt('e'), 0.5)
        r = g.screen.find('def greet')
        if n(r is not None, 'game.py shown'):
            row = r[0]
            colours = {w: g.screen.colour_of(row, w) for w in ('def', 'greet')}
            row = g.screen.find('print(')[0]
            colours.update({w: g.screen.colour_of(row, w) for w in ('print', 'f"hi', '42', '#')})
            deco = g.screen.colour_of(g.screen.find('@dataclass')[0], '@dataclass')
            klass = g.screen.colour_of(g.screen.find('Player')[0], 'Player')
            n.info(f'colours: {colours}, decorator {deco}, class name {klass}')
            n(None not in colours.values() and len(set(colours.values())) >= 5,
              'keyword / def name / built-in / string / number / comment - at least 5 colours')
            n(deco is not None and deco != colours['def'], 'the decorator has a colour of its own')
        # A new line after 'if x:' is indented one level more
        g.send(PGDN, 0.2)
        g.send(END, 0.2)
        g.send(ENTER, 0.2)
        g.type('if x:', wait=0.2)
        g.send(ENTER, 0.2)
        g.type('y', wait=0.4)
        rows = [g.screen.text(r) for r in range(g.screen.rows)]
        line = next((t for t in rows if t.rstrip().endswith('y') and '        y' in t), None)
        n(line is not None, 'after "if x:" + Enter the next line is indented 4 more')
        # Run a script that crashes: the traceback is a build message, F8 goes to the line that failed
        g.send(alt('t'), 0.5)
        g.type('python3 crash.py', wait=0.2)
        g.send(ENTER, 0.3)
        n(g.wait_text('ZeroDivisionError', 8), 'python3 ran in the terminal')
        n(g.wait_text('build: 1 error', 4), 'when it ends: "build: 1 error" on the status line')
        g.send(F8, 0.8)
        g.wait_for(lambda g: 'l: ' in g.status_line(), 6)
        n('crash.py' in g.status_line() or 'crash.py' in ''.join(g.screen.lines()), 'F8 opened crash.py')
        n(g.cursor_line() == 2, f'F8: at line 2, "return a / b" (cursor line {g.cursor_line()})')
        plain, marked = gutter_colour(g, 1), gutter_colour(g, 2)
        n(None not in (plain, marked) and plain != marked, f'the line with the error has its number coloured ({marked} vs {plain})')
        g.send(ESC, 0.3)
        g.quick('.q!', 0.5)
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


@check('#40 The key reference matches the keymaps')
def keys_doc(n):
    script = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'gen-keys.py')
    r = subprocess.run([sys.executable, script, '--check'], capture_output=True, text=True)
    n(r.returncode == 0, f'docs/keys.md is what scripts/gen-keys.py writes: {(r.stdout + r.stderr).strip()}')


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
    ap.add_argument('--real-agent', action='store_true',
                    help=f'run real_agent_ask with the installed claude - costs tokens; at most {REAL_AGENT_PER_HOUR}/hour')
    ap.add_argument('--real-agent-model', default='', help="the Editor Agent's model for --real-agent (main.ai_model)")
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
    REAL_AGENT['enabled'] = args.real_agent
    REAL_AGENT['model'] = args.real_agent_model
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
