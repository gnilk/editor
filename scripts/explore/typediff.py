#!/usr/bin/env python3
# Rough type-diff: the structure (symbols + edges, from `gedit-bench --dump`) of a set of files at two revisions, compared
# by qualified name - so a move between files is not an add + remove. Plain text out.
# Usage: typediff.py --bench <gedit-bench> --base <rev> --head <rev> <merge> [<merge> ...]
#   the files are the merges' own changes (feature_files.py), C/C++ only
import argparse, collections, os, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(__file__))
from feature_files import feature_files, git

EDGE_KINDS = ("inherits", "holds", "includes", "using")
TYPE_KINDS = {"class", "struct", "union", "enum"}
CODE_EXT = (".h", ".hpp", ".cpp", ".cc", ".c")

def dump(bench, rev, files, workdir):
    root = os.path.join(workdir, rev.replace("/", "_").replace("^", "_"))
    present = []
    for path in files:
        try:
            text = git("show", f"{rev}:{path}")
        except subprocess.CalledProcessError:
            continue                                    # not in this revision
        dst = os.path.join(root, path)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, "w").write(text)
        present.append(path)
    if not present:
        return set(), set()
    out = subprocess.run([os.path.abspath(bench), "--dump", root], capture_output=True, text=True,
                         cwd=os.path.dirname(os.path.abspath(bench))).stdout
    return parse(out, root)

def parse(out, root):
    symbols, edges = set(), set()
    current = None
    for line in out.splitlines():
        if line.startswith("== "):
            current = os.path.relpath(line[3:].rsplit(" (", 1)[0], root)
            continue
        tokens = line.split()
        if len(tokens) < 3 or not tokens[0].isdigit():
            continue
        if tokens[2].startswith(EDGE_KINDS):
            kind = tokens[2]
            to = " ".join(tokens[3:])
            if kind.startswith("includes"):
                edges.add((kind, os.path.dirname(current), to))    # from the file's folder: a module edge
            else:
                edges.add((kind, tokens[1], to))
        else:
            kind, name = tokens[1], tokens[2]
            rest = " ".join(tokens[3:])
            stype = rest.split(":", 1)[1].split("[decl]")[0].strip() if rest.startswith(":") else ""
            stype = stype.rsplit(" ", 1)[0] if stype and stype.split()[-1][0].isdigit() and "-" in stype.split()[-1] else stype
            symbols.add((kind, name, stype))
    return symbols, edges

def section(title, rows):
    print(f"\n## {title} ({len(rows)})")
    for row in sorted(rows):
        print("  " + row)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bench", required=True)
    ap.add_argument("--base", required=True)
    ap.add_argument("--head", required=True)
    ap.add_argument("merges", nargs="+")
    a = ap.parse_args()
    files = sorted({p for _, p in feature_files(a.merges, ["src"]) if p.endswith(CODE_EXT)})
    with tempfile.TemporaryDirectory() as work:
        bs, be = dump(a.bench, a.base, files, work)
        hs, he = dump(a.bench, a.head, files, work)
    print(f"# type-diff {a.base}..{a.head}, {len(files)} files from {len(a.merges)} merges")
    # one name per function: an out-of-line definition 'function X::f' is the method 'gedit::X::f' declared in the class
    def norm(symbols):
        methods = {n.split("::", 1)[1] for k, n, _ in symbols if k == "method" and "::" in n}
        return {("method" if k == "function" and n in methods else k, n) for k, n, _ in symbols
                if not (k == "function" and n in methods)}
    bnames, hnames = norm(bs), norm(hs)
    def names(kinds, sel):
        return {f"{k:8} {n}" for k, n in sel if k in kinds}
    print(f"types +{len(names(TYPE_KINDS, hnames - bnames))} -{len(names(TYPE_KINDS, bnames - hnames))}, "
          f"functions +{len(names({'method', 'function'}, hnames - bnames))} -{len(names({'method', 'function'}, bnames - hnames))}, "
          f"members +{len(names({'member'}, hnames - bnames))} -{len(names({'member'}, bnames - hnames))}")
    rel = lambda es: {f"{f} {k} {t}" for k, f, t in es if not k.startswith("includes")}
    section("inherits / holds added", rel(he - be))
    section("inherits / holds removed", rel(be - he))
    mod = lambda es: {f"{f} -> {t}" for k, f, t in es if k == "includes" and "/" in t}
    section("folder includes added (project headers)", mod(he - be))
    section("folder includes removed (project headers)", mod(be - he))
    section("types added", names(TYPE_KINDS, hnames - bnames))
    section("types removed", names(TYPE_KINDS, bnames - hnames))
    section("functions / methods added", names({"method", "function"}, hnames - bnames))
    section("functions / methods removed", names({"method", "function"}, bnames - hnames))
    section("members added", names({"member"}, hnames - bnames))
    section("members removed", names({"member"}, bnames - hnames))
    btypes = {(k, n): t for k, n, t in bs if k == "member"}
    changed = {f"{n}: {btypes[(k, n)]} -> {t}" for k, n, t in hs if k == "member" and (k, n) in btypes and btypes[(k, n)] != t}
    section("members whose type changed", changed)


if __name__ == "__main__":
    main()
