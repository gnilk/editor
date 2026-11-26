#!/usr/bin/env python3
# Type-diff: the structure (symbols + edges, from `goatedit --dump`) of the C/C++ files a change touched, at the base and
# now, compared by qualified name - so a move between files is not an add + remove. Plain text out, architecture first
# (agent-integration.md §6.4, the 'structure' question; the probe is agentic-questions.md §3.1).
# Shipped with GoatEdit (Tools/); run in the project root.
# Usage: typediff.py (--goatedit <exe> | --bench <gedit-bench>) [<revision>]
#   revision: a git revision to compare from (a branch, a tag, a hash); default the branch point - 'git merge-base HEAD
#   <trunk>', the closest of main / master / dev / develop.
#   now: the work tree as saved - the files git tracks ('git add' is enough), and the untracked ones a tracked build file
#   names (with the header next to one it names). Other untracked C/C++ files are named, not scanned: code lying around
#   for reference would read as part of the change.
#   --merges <m> [<m> ...]: the files of these merges (each M^1..M) instead, at --base and --head (the old probe's mode)
import argparse, os, re, subprocess, sys, tempfile

EDGE_KINDS = ("inherits", "holds", "includes", "using")
TYPE_KINDS = {"class", "struct", "union", "enum"}
CODE_EXT = (".h", ".hpp", ".cpp", ".cc", ".c")
MAX_FILES_LISTED = 40
HEADER_EXT = (".h", ".hpp")
# Build files, all with '#' comments
BUILD_FILES = ("CMakeLists.txt", "*.cmake", "Makefile", "makefile", "GNUmakefile", "*.mk", "meson.build")
MAX_UNTRACKED_LISTED = 20
WORK_TREE = None        # 'rev' for the files as they are on disk

def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True, check=True).stdout

# The trunks a change may branch from; the closest branch point (fewest commits from it to HEAD) wins - a branch off
# 'dev' whose 'main' lags behind would otherwise drag in all of dev's work
TRUNKS = ("main", "master", "dev", "develop")

def is_revision(rev):
    run = subprocess.run(["git", "rev-parse", "--verify", "--quiet", f"{rev}^{{commit}}"], capture_output=True)
    return run.returncode == 0

def default_base():
    best = None
    for trunk in TRUNKS:
        try:
            git("rev-parse", "--verify", "--quiet", trunk)
        except subprocess.CalledProcessError:
            continue
        base = git("merge-base", "HEAD", trunk).strip()
        distance = int(git("rev-list", "--count", f"{base}..HEAD").strip())
        if (best is None) or (distance < best[0]):
            best = (distance, base, trunk)
    if best is None:
        raise SystemExit(f"typediff: none of {', '.join(TRUNKS)} is a branch here - name a base revision")
    return best[1], f"the branch point with {best[2]}"

def changed_files(base):
    # --relative: as 'git ls-files' has them, from this folder - a project root inside a larger repository
    return code_files(git("diff", "--name-only", "--relative", "-z", base, "--"))

def untracked_files():
    return code_files(git("ls-files", "--others", "--exclude-standard", "-z"))

# From a '-z' listing: names with anything but ASCII come as they are, not quoted
def code_files(listing):
    return sorted({f for f in listing.split("\0") if f.endswith(CODE_EXT)})

# The untracked files that are part of the build: a tracked build file names them - by their path from the project root
# or from its own folder, not in a comment - or they are the header next to one it names
def files_the_build_names(untracked):
    named = set()
    for build in git("ls-files", "-z", "--", *[f":(glob)**/{name}" for name in BUILD_FILES]).split("\0"):
        if not build or not os.path.isfile(build):
            continue
        lines = open(build, encoding="utf-8", errors="replace").read().splitlines()
        words = set(re.split(r"[\s\"'();:]+", "\n".join(line.split("#", 1)[0] for line in lines)))
        # '${CMAKE_CURRENT_SOURCE_DIR}/a.cpp', '$(SRC)/a.cpp', './a.cpp' - what follows is the path
        paths = {re.sub(r"^(\$\{\w+\}|\$|\w*\)|\.)?/", "", word) for word in words}
        folder = os.path.dirname(build)
        for path in untracked:
            # A bare name is one in the build file's own folder, never the root's
            is_from_root = "/" in path and path in paths
            if is_from_root or os.path.relpath(path, folder or ".") in paths:
                named.add(path)
    stems = {os.path.splitext(path)[0] for path in named}
    named.update(path for path in untracked if path.endswith(HEADER_EXT) and os.path.splitext(path)[0] in stems)
    return sorted(named)

# By name; by folder when there are many
def print_untracked(files):
    if not files:
        return
    print(f"not scanned: {len(files)} untracked C/C++ files ('git add' one to have it read as part of the change)")
    rows = files
    if len(files) > MAX_UNTRACKED_LISTED:
        counts = {}
        for path in files:
            folder = os.path.dirname(path) or "."
            counts[folder] = counts.get(folder, 0) + 1
        rows = [f"{folder}/ ({count})" for folder, count in sorted(counts.items())]
    for row in rows[:MAX_UNTRACKED_LISTED]:
        print(f"  {row}")
    if len(rows) > MAX_UNTRACKED_LISTED:
        print(f"  ... and {len(rows) - MAX_UNTRACKED_LISTED} more folders")

def merge_files(merges):
    files = set()
    for merge in merges:
        files.update(git("diff", "--name-only", f"{merge}^1", merge).splitlines())
    return sorted(f for f in files if f.endswith(CODE_EXT))

def scan_command(a, root):
    if a.goatedit:
        return [a.goatedit, "--skip-user-config", "--backend", "headless", "--dump", root], None
    return [os.path.abspath(a.bench), "--dump", root], os.path.dirname(os.path.abspath(a.bench))

def dump(a, rev, files, workdir):
    root = os.path.join(workdir, "now" if rev is WORK_TREE else rev.replace("/", "_").replace("^", "_"))
    present = []
    for path in files:
        if rev is WORK_TREE:
            if not os.path.isfile(path):
                continue                                # deleted since the base
            text = open(path, encoding="utf-8", errors="replace").read()
        else:
            try:
                text = git("show", f"{rev}:./{path}")
            except subprocess.CalledProcessError:
                continue                                # not in this revision
        dst = os.path.join(root, path)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, "w").write(text)
        present.append(path)
    if not present:
        return set(), set()
    command, cwd = scan_command(a, root)
    run = subprocess.run(command, capture_output=True, text=True, cwd=cwd)
    if run.returncode != 0:
        raise SystemExit(f"typediff: the scan failed ({' '.join(command[:1])}): {run.stderr.strip()}")
    return parse(run.stdout, root)

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
    ap.add_argument("--goatedit")
    ap.add_argument("--bench")
    ap.add_argument("--merges", nargs="+")
    ap.add_argument("--head")
    ap.add_argument("base", nargs="?")
    a = ap.parse_args()
    if not (a.goatedit or a.bench):
        ap.error("--goatedit or --bench")
    for rev in (a.base, a.head):
        if rev and not is_revision(rev):
            raise SystemExit(f"'{rev}' is not a git revision (a branch, a tag, a hash, HEAD~3) - the type-diff compares "
                             f"the code since that revision; without one, since the branch point")
    if a.merges:
        if not (a.base and a.head):
            ap.error("--merges needs a base and --head")
        base, head, files = a.base, a.head, merge_files(a.merges)
        what = f"{len(files)} files from {len(a.merges)} merges"
        title = f"{base}..{head}"
    else:
        base, how = (a.base, a.base) if a.base else default_base()
        head, files = WORK_TREE, changed_files(base)
        untracked = untracked_files()
        in_build = files_the_build_names(untracked)
        files = sorted({*files, *in_build})
        what = f"{len(files)} C/C++ files changed"
        title = f"{git('rev-parse', '--short', base).strip()} ({how})..the work tree"
    with tempfile.TemporaryDirectory() as work:
        work = os.path.realpath(work)
        bs, be = dump(a, base, files, work)
        hs, he = dump(a, head, files, work)
    print(f"# type-diff {title}, {what}")
    if len(files) <= MAX_FILES_LISTED:
        for path in files:
            print(f"  {path}")
    if head is WORK_TREE:
        if in_build:
            print(f"of these, untracked but named in a build file - not committed with the rest until 'git add': {len(in_build)}")
            for path in in_build:
                print(f"  {path}")
        print_untracked([path for path in untracked if path not in in_build])
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
    btypes = {(k, n): t for k, n, t in bs if k == "member"}
    changed = {f"{n}: {btypes[(k, n)]} -> {t}" for k, n, t in hs if k == "member" and (k, n) in btypes and btypes[(k, n)] != t}
    section("members whose type changed", changed)
    section("members added", names({"member"}, hnames - bnames))
    section("members removed", names({"member"}, bnames - hnames))
    # Last: the longest lists and the least telling - a long output is cut at its end
    section("functions / methods added", names({"method", "function"}, hnames - bnames))
    section("functions / methods removed", names({"method", "function"}, bnames - hnames))


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        # git's own words, not a traceback - the editor shows the first line of stderr
        raise SystemExit(f"git {' '.join(error.cmd[1:])}: {(error.stderr or '').strip() or f'exit {error.returncode}'}")
