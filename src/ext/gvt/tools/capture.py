#!/usr/bin/env python3
"""
Record everything a program writes to its terminal, as a gvt test fixture.

Runs the command under a pty of a fixed size (TERM=xterm-256color, like the editor's Shell), sends scripted
input with delays, and writes every byte the program produced to --out. Stdlib only.

  tools/capture.py --out tests/fixtures/vim.bin --cols 80 --rows 24 \
      --step 1.0 ':q!\r' -- vim --clean somefile

Steps are (delay-seconds, text) pairs; text takes Python escapes ('\\x1b[B', '\\r', '\\x03').
--reply-da answers the primary device-attributes query (CSI c / CSI 0 c) like xterm, for programs that wait on it.
"""
import argparse
import fcntl
import os
import pty
import select
import signal
import struct
import sys
import termios
import time


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--out', required=True)
    parser.add_argument('--cols', type=int, default=80)
    parser.add_argument('--rows', type=int, default=24)
    parser.add_argument('--cwd', default=None)
    parser.add_argument('--env', action='append', default=[], help='KEY=VALUE for the child')
    parser.add_argument('--step', nargs=2, action='append', default=[], metavar=('DELAY', 'TEXT'))
    parser.add_argument('--tail', type=float, default=1.0, help='seconds to keep reading after the last step')
    parser.add_argument('--reply-da', action='store_true')
    parser.add_argument('command', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.command and args.command[0] == '--':
        args.command = args.command[1:]
    if not args.command:
        parser.error('no command given')
    return args


def read_for(fd, seconds, sink, reply_da):
    deadline = time.monotonic() + seconds
    while True:
        left = deadline - time.monotonic()
        if left <= 0:
            return True
        ready, _, _ = select.select([fd], [], [], left)
        if not ready:
            continue
        try:
            data = os.read(fd, 65536)
        except OSError:
            return False        # child closed the pty
        if not data:
            return False
        sink.extend(data)
        if reply_da and (b'\x1b[c' in data or b'\x1b[0c' in data):
            os.write(fd, b'\x1b[?62;22c')


def main():
    args = parse_args()
    pid, fd = pty.fork()
    if pid == 0:
        os.environ['TERM'] = 'xterm-256color'
        os.environ.pop('COLORTERM', None)
        for item in args.env:
            key, _, value = item.partition('=')
            os.environ[key] = value
        if args.cwd:
            os.chdir(args.cwd)
        os.execvp(args.command[0], args.command)

    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack('HHHH', args.rows, args.cols, 0, 0))
    output = bytearray()
    alive = True
    for delay, text in args.step:
        alive = read_for(fd, float(delay), output, args.reply_da)
        if not alive:
            break
        os.write(fd, text.encode('utf-8').decode('unicode_escape').encode('latin-1'))
    if alive:
        alive = read_for(fd, args.tail, output, args.reply_da)
    if alive:
        os.kill(pid, signal.SIGHUP)
    try:
        os.waitpid(pid, 0)
    except ChildProcessError:
        pass

    with open(args.out, 'wb') as f:
        f.write(output)
    print(f'{args.out}: {len(output)} bytes', file=sys.stderr)


if __name__ == '__main__':
    main()
