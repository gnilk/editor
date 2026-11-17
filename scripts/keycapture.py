#!/usr/bin/env python3
# Prints the bytes the terminal sends per key, with the keyboard modes goatedit's ANSI backend turns on.
#   python3 scripts/keycapture.py            # the modes as goatedit picks them for this terminal
#   python3 scripts/keycapture.py --plain    # no modes (what the terminal sends by default)
# Press keys (Opt+E, Esc, ...); Ctrl+D twice in a row ends it (or 30 s without a key).
import os, sys, termios, tty, select

plain = '--plain' in sys.argv
termProgram = os.environ.get('TERM_PROGRAM', '')
term = os.environ.get('TERM', '')
isApple = termProgram == 'Apple_Terminal'
isDumb = term in ('', 'dumb')
on, off = '', ''
if not plain and not isDumb:
    if not isApple:
        on += '\x1b[>1u'          # Kitty keyboard, flag 1
        off = '\x1b[<u' + off
        on += '\x1b[>4;2m'        # xterm modifyOtherKeys 2
        off = '\x1b[>4m' + off
    on += '\x1b[?1036s\x1b[?1036h'  # metaSendsEscape
    off = '\x1b[?1036r' + off

fd = sys.stdin.fileno()
saved = termios.tcgetattr(fd)
print(f'TERM={term} TERM_PROGRAM={termProgram} modes: {"none" if not on else repr(on)}')
print('press keys; Ctrl+D twice ends\r')
try:
    tty.setraw(fd)
    os.write(1, on.encode())
    lastEot = False
    while True:
        r, _, _ = select.select([fd], [], [], 30)
        if not r:
            break
        data = os.read(fd, 1024)
        os.write(1, (repr(data) + '\r\n').encode())
        # Ctrl+D as a plain byte, as Kitty reports it (CSI 100;5u) or as modifyOtherKeys does (CSI 27;5;100~)
        if data in (b'\x04', b'\x1b[100;5u', b'\x1b[27;5;100~'):
            if lastEot:
                break
            lastEot = True
        else:
            lastEot = False
finally:
    os.write(1, off.encode())
    termios.tcsetattr(fd, termios.TCSADRAIN, saved)
