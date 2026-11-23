"""
goatpty - drive goatedit's ANSI backend in a pty and read the screen back.

A Goat runs one editor in an isolated sandbox (own HOME / XDG dirs, so the user's config, session and shell rc
never leak in), feeds it keys, and keeps a small VT screen model (chars + fg/bg per cell) of what it drew.
"""
import os
import pty
import re
import select
import shutil
import signal
import struct
import fcntl
import termios
import time

# Keys as xterm / kitty send them (gansi asks for the kitty protocol, so CSI u chords are understood)
ESC = b'\x1b'
ENTER = b'\r'
TAB = b'\t'
BACKSPACE = b'\x7f'
UP, DOWN, RIGHT, LEFT = b'\x1b[A', b'\x1b[B', b'\x1b[C', b'\x1b[D'
HOME, END = b'\x1b[H', b'\x1b[F'
PGUP, PGDN = b'\x1b[5~', b'\x1b[6~'
SHIFT_DOWN, SHIFT_UP = b'\x1b[1;2B', b'\x1b[1;2A'
SHIFT_END = b'\x1b[1;2F'


def ctrl(ch):
    """Ctrl+<ch> as a kitty CSI u sequence (unambiguous, also for '[', '/', Enter)"""
    code = 13 if ch == 'enter' else ord(ch)
    return f'\x1b[{code};5u'.encode()


def alt(ch):
    return ESC + ch.encode()


class Screen:
    def __init__(self, rows, cols):
        self.rows, self.cols = rows, cols
        self.clear()
        self.y = self.x = 0
        self.fg = self.bg = None
        self.inverse = False
        self.pending = ''   # an escape sequence cut by the end of a read - completed by the next feed

    def clear(self):
        self.cells = [[(' ', None, None) for _ in range(self.cols)] for _ in range(self.rows)]

    # The start of a sequence that the end of the input may have cut: CSI, OSC, DCS/APC/PM, or a lone ESC
    INCOMPLETE = re.compile(r'\x1b(\[[?>=<]?[0-9;:]*[ -/]*|\][^\x07\x1b]*\x1b?|[P_^].*|[()#])?\Z', re.S)

    def feed(self, s):
        s = self.pending + s
        self.pending = ''
        i, n = 0, len(s)
        while i < n:
            c = s[i]
            if c == '\x1b':
                m = re.match(r'\x1b\[([?>=<]?)([0-9;:]*)([ -/]*)([@-~])', s[i:])
                if m:
                    self._csi(m.group(1), m.group(2), m.group(4))
                    i += m.end()
                    continue
                m = re.match(r'\x1b\][^\x07\x1b]*(\x07|\x1b\\)', s[i:]) or re.match(r'\x1b[P_^].*?\x1b\\', s[i:], re.S)
                if m:
                    i += m.end()
                    continue
                if self.INCOMPLETE.match(s, i):
                    # A read ended inside it (the pty splits writes where it likes - more often on macOS)
                    self.pending = s[i:]
                    return
                i += 3 if i + 1 < n and s[i + 1] in '()#' else 2
                continue
            if c == '\r':
                self.x = 0
            elif c == '\n':
                self.y = min(self.rows - 1, self.y + 1)
            elif c == '\b':
                self.x = max(0, self.x - 1)
            elif ord(c) >= 32:
                if self.x < self.cols:
                    fg, bg = (self.bg, self.fg) if self.inverse else (self.fg, self.bg)
                    self.cells[self.y][self.x] = (c, fg, bg)
                self.x = min(self.cols - 1, self.x + 1)
            i += 1

    def _csi(self, p, args, fin):
        a = [int(v) if v else 0 for v in args.replace(':', ';').split(';')] if args else []
        arg0 = a[0] if a and a[0] else 1
        if p:
            return
        if fin == 'm':
            self._sgr(a or [0])
        elif fin in 'Hf':
            self.y = max(0, min(self.rows - 1, arg0 - 1))
            self.x = max(0, min(self.cols - 1, (a[1] if len(a) > 1 and a[1] else 1) - 1))
        elif fin == 'K':
            mode = a[0] if a else 0
            span = range(self.x, self.cols) if mode == 0 else range(0, self.x + 1) if mode == 1 else range(self.cols)
            for q in span:
                self.cells[self.y][q] = (' ', None, self.bg)
        elif fin == 'J':
            mode = a[0] if a else 0
            if mode in (2, 3):
                self.clear()
            elif mode == 0:
                for q in range(self.x, self.cols):
                    self.cells[self.y][q] = (' ', None, None)
                for r in range(self.y + 1, self.rows):
                    self.cells[r] = [(' ', None, None) for _ in range(self.cols)]
        elif fin == 'G':
            self.x = max(0, min(self.cols - 1, arg0 - 1))
        elif fin == 'd':
            self.y = max(0, min(self.rows - 1, arg0 - 1))
        elif fin == 'C':
            self.x = min(self.cols - 1, self.x + arg0)
        elif fin == 'D':
            self.x = max(0, self.x - arg0)
        elif fin == 'A':
            self.y = max(0, self.y - arg0)
        elif fin == 'B':
            self.y = min(self.rows - 1, self.y + arg0)
        elif fin == 'X':
            for q in range(self.x, min(self.cols, self.x + arg0)):
                self.cells[self.y][q] = (' ', None, self.bg)

    def _sgr(self, a):
        k = 0
        while k < len(a):
            v = a[k]
            if v == 0:
                self.fg = self.bg = None
                self.inverse = False
            elif v == 7:
                self.inverse = True
            elif v == 27:
                self.inverse = False
            elif v == 39:
                self.fg = None
            elif v == 49:
                self.bg = None
            elif v in (38, 48) and k + 1 < len(a):
                if a[k + 1] == 2 and k + 4 < len(a):
                    col = '%02x%02x%02x' % tuple(a[k + 2:k + 5])
                    k += 4
                elif a[k + 1] == 5 and k + 2 < len(a):
                    col = 'idx%d' % a[k + 2]
                    k += 2
                else:
                    col = None
                if v == 38:
                    self.fg = col
                else:
                    self.bg = col
            elif 30 <= v <= 37 or 90 <= v <= 97:
                self.fg = 'ansi%d' % v
            elif 40 <= v <= 47 or 100 <= v <= 107:
                self.bg = 'ansi%d' % v
            k += 1

    def text(self, row):
        return ''.join(c for c, _, _ in self.cells[row]).rstrip()

    def lines(self):
        return [self.text(r) for r in range(self.rows)]

    def dump(self):
        return '\n'.join(f'{r:2d}|{t}' for r, t in enumerate(self.lines()))

    def runs(self, row, start=0):
        """[(text, fg, bg)] - the row cut into runs of one colour pair, blanks dropped"""
        out = []
        for ch, fg, bg in self.cells[row][start:]:
            if out and out[-1][1] == fg and out[-1][2] == bg:
                out[-1][0] += ch
            else:
                out.append([ch, fg, bg])
        return [(t, fg, bg) for t, fg, bg in out if t.strip()]

    def colour_of(self, row, word, start=0):
        """fg of the first cell of 'word' on 'row' (searching from column 'start')"""
        line = ''.join(c for c, _, _ in self.cells[row])
        col = line.find(word, start)
        if col < 0:
            return None
        return self.cells[row][col][1]

    def find(self, needle):
        for r in range(self.rows):
            col = self.text(r).find(needle)
            if col >= 0:
                return r, col
        return None


class Goat:
    EXE = None          # set by the runner

    def __init__(self, sandbox, args, cwd=None, rows=40, cols=140, term='xterm-256color', env=None,
                 skip_user_config=True, backend='ansi', exe=None):
        sandbox = os.path.abspath(sandbox)
        self.sandbox = sandbox
        cwd = os.path.abspath(cwd) if cwd else None
        self.rows, self.cols = rows, cols
        self.raw = bytearray()
        self.dead = False
        self.status = None
        self.stderr_path = os.path.join(sandbox, f'stderr.{int(time.time() * 1000)}.txt')
        exe = exe or Goat.EXE
        argv = [exe]
        if backend:
            argv += ['--backend', backend]
        if skip_user_config:
            argv += ['--skip-user-config']
        argv += list(args)
        child_env = sandbox_env(sandbox, term)
        if env:
            for k, v in env.items():
                if v is None:
                    child_env.pop(k, None)
                else:
                    child_env[k] = v
        self.argv = argv
        self.started = time.time()
        pid, fd = pty.fork()
        if pid == 0:
            try:
                os.chdir(cwd or sandbox)
                errfd = os.open(self.stderr_path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)
                os.dup2(errfd, 2)
                os.execve(exe, argv, child_env)
            finally:
                os._exit(127)
        self.pid, self.fd = pid, fd
        fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
        self.screen = Screen(rows, cols)
        self._decoder_tail = b''

    # --- io
    def pump(self, seconds=0.3):
        end = time.time() + seconds
        while time.time() < end and not self.dead:
            r, _, _ = select.select([self.fd], [], [], 0.03)
            if r:
                try:
                    data = os.read(self.fd, 65536)
                except OSError:
                    self.dead = True
                    break
                if not data:
                    self.dead = True
                    break
                self.raw.extend(data)
                chunk = self._decoder_tail + data
                # keep an incomplete UTF-8 tail for the next read
                cut = len(chunk)
                for back in range(1, 4):
                    if cut - back >= 0 and (chunk[cut - back] & 0xC0) == 0xC0:
                        need = 2 if chunk[cut - back] < 0xE0 else 3 if chunk[cut - back] < 0xF0 else 4
                        if back < need:
                            cut -= back
                        break
                self._decoder_tail = chunk[cut:]
                self.screen.feed(chunk[:cut].decode('utf-8', 'replace'))
        self.reap()

    def send(self, data, wait=0.35):
        if isinstance(data, str):
            data = data.encode()
        if not self.dead:
            try:
                os.write(self.fd, data)
            except OSError:
                self.dead = True
        self.pump(wait)

    def type(self, text, per_key=0.04, wait=0.3):
        for ch in text.encode() if isinstance(text, str) else text:
            self.send(bytes([ch]), per_key)
        self.pump(wait)

    def wait_for(self, predicate, timeout=5.0):
        end = time.time() + timeout
        while time.time() < end:
            if predicate(self):
                return True
            if self.dead:
                return predicate(self)
            self.pump(0.1)
        return predicate(self)

    def wait_text(self, needle, timeout=5.0):
        return self.wait_for(lambda g: g.screen.find(needle) is not None, timeout)

    def reap(self):
        if self.status is not None:
            return True
        try:
            wp, st = os.waitpid(self.pid, os.WNOHANG)
        except ChildProcessError:
            return True
        if wp == self.pid:
            self.status = st
            self.ended = time.time()
            return True
        return False

    def wait_exit(self, timeout=5.0):
        end = time.time() + timeout
        while time.time() < end:
            self.pump(0.1)
            if self.reap():
                return True
        return False

    def kill(self):
        if not self.reap():
            try:
                os.kill(self.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            try:
                os.waitpid(self.pid, 0)
            except ChildProcessError:
                pass
        try:
            os.close(self.fd)
        except OSError:
            pass

    # --- editor helpers
    def resize(self, rows, cols, wait=1.0):
        """The terminal window changes size: the pty's size, SIGWINCH, and a fresh screen to read the redraw into"""
        self.rows, self.cols = rows, cols
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack('HHHH', rows, cols, 0, 0))
        os.kill(self.pid, signal.SIGWINCH)
        self.screen = Screen(rows, cols)
        self.pump(wait)

    def status_line(self):
        for r in range(self.rows):
            t = self.screen.text(r)
            if 'GoatEdit v' in t:
                return t
        return ''

    def ready(self, timeout=8.0):
        """The status line is up and the terminal's shell has drawn its prompt"""
        return self.wait_for(lambda g: 'GoatEdit v' in g.status_line() and any(t.endswith('$') for t in g.screen.lines()), timeout)

    def view(self):
        m = re.search(r'\| (\w{3}) \|', self.status_line())
        return m.group(1) if m else None

    def cursor_line(self):
        m = re.search(r'l: (\d+)', self.status_line())
        return int(m.group(1)) if m else None

    def quick(self, command, wait=0.8):
        """Esc, then a quick-mode command line (e.g. '.s'), then Enter"""
        self.send(ESC, 0.3)
        self.type(command, wait=0.1)
        self.send(ENTER, wait)

    def quit(self, timeout=5.0):
        self.quick('.q!', 0.2)
        return self.wait_exit(timeout)

    def stderr(self):
        try:
            with open(self.stderr_path, encoding='utf-8', errors='replace') as f:
                return f.read()
        except OSError:
            return ''

    def editor_rows(self):
        """Rows of the editor pane: (screen row, gutter line number or None, text after the gutter)"""
        out = []
        for r in range(self.rows):
            t = self.screen.text(r)
            m = re.match(r'^.*?│\s{0,6}(\d+)?\s+[*|‾_ ]?\s?(.*)$', t)
            if m and '│' in t:
                out.append((r, int(m.group(1)) if m.group(1) else None, m.group(2)))
        return out


def sandbox_env(sandbox, term):
    home = os.path.join(sandbox, 'home')
    for sub in ('', '.config', '.local/state', '.local/share', '.cache'):
        os.makedirs(os.path.join(home, sub), exist_ok=True)
    env = {
        'HOME': home,
        'XDG_CONFIG_HOME': os.path.join(home, '.config'),
        'XDG_STATE_HOME': os.path.join(home, '.local/state'),
        'XDG_DATA_HOME': os.path.join(home, '.local/share'),
        'XDG_CACHE_HOME': os.path.join(home, '.cache'),
        'PATH': os.environ.get('PATH', '/usr/bin:/bin'),
        'LANG': os.environ.get('LANG', 'C.UTF-8'),
        'TERM': term,
        'COLORTERM': 'truecolor',
        'SHELL': '/bin/bash',
        'USER': os.environ.get('USER', 'user'),
    }
    return env


def fresh_dir(path):
    shutil.rmtree(path, ignore_errors=True)
    os.makedirs(path)
    return path
