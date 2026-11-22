#!/usr/bin/env python3
# Files a feature touched, from its merges (each merge's own change: M^1..M), not merge-base..HEAD - a feature merged
# in steps across a busy trunk would otherwise drag in every unrelated merge between them.
# Usage: feature_files.py <merge> [<merge> ...] [-- <path> ...]
import subprocess, sys

def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True, check=True).stdout

def feature_files(merges, paths):
    files = set()
    for merge in merges:
        out = git("diff", "--name-status", f"{merge}^1", merge, "--", *paths)
        for line in out.splitlines():
            parts = line.split("\t")
            files.add((parts[0][0], parts[-1]))
    return sorted(files, key=lambda f: f[1])

if __name__ == "__main__":
    args = sys.argv[1:]
    paths = args[args.index("--") + 1:] if "--" in args else []
    merges = args[:args.index("--")] if "--" in args else args
    for status, path in feature_files(merges, paths):
        print(f"{status}\t{path}")
