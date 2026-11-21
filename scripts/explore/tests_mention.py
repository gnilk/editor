#!/usr/bin/env python3
# "No test mentions it": for the types and functions a type-diff (typediff.py's text) added, which names appear in no
# test source. A mention is not coverage - a name in a test can be a comment - but no mention is a strong hint.
# Usage: typediff.py ... | tests_mention.py [--tests utests]
import argparse, collections, os, re, sys

def added(lines, title):
    out, inside = [], False
    for line in lines:
        if line.startswith("## "):
            inside = line.startswith(f"## {title} ")
            continue
        if inside and line.strip():
            out.append(line.split()[-1])
    return out

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tests", default="utests")
    a = ap.parse_args()
    lines = sys.stdin.read().splitlines()
    text = ""
    for root, _, files in os.walk(a.tests):
        for f in files:
            if f.endswith((".cpp", ".h")):
                text += open(os.path.join(root, f), errors="replace").read()
    words = collections.Counter(re.findall(r"[A-Za-z_]\w*", text))
    types = added(lines, "types added")
    funcs = added(lines, "functions / methods added")
    print("# no test mentions it")
    print("\n## types")
    for t in types:
        n = words[t.split("::")[-1]]
        print(f"  {n:4}  {t}")
    byclass = collections.defaultdict(list)
    for f in funcs:
        parts = f.split("::")
        byclass["::".join(parts[:-1]) or "<free>"].append(parts[-1])
    print("\n## functions, per class: mentioned / added - the ones no test names")
    for cls in sorted(byclass):
        names = byclass[cls]
        missing = sorted(n for n in names if words[n] == 0 and not n.startswith("~"))
        print(f"  {len(names) - len(missing):3}/{len(names):<3} {cls}")
        if missing:
            print("          " + ", ".join(missing))

if __name__ == "__main__":
    main()
