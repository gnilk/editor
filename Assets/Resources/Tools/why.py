#!/usr/bin/env python3
# "Why is this line here?" - the chain the repository already records: blame -> the commit -> its message, the #N /
# open-bugs #N it names, the merge that brought it in -> the Markdown docs that mention those numbers or the line's rarer
# names -> the Claude session; and where the line's text was first committed, in any file - the blamed commit may only have
# copied it. Plain text, for a person and for the agent (agent-integration.md §6.4, the 'why' question).
# Shipped with GoatEdit (Tools/); run in the project root.
# Usage: why.py <file>:<line> [...]
import re, subprocess, sys

# A name named in more docs than this says nothing about the line ('ViewBase', 'Core')
MAX_DOCS_FOR_A_NAME = 3
# A line in more files than this is everyday code - where it was first seen says nothing about this one; nor does a
# short one ('return true;')
MAX_FILES_FOR_A_LINE = 20
MIN_LINE_LENGTH = 16
ORIGIN_TIMEOUT_S = 10
MAX_REASONS = 8

def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True).stdout

def reason_lines(body):
    return [l.strip() for l in body.splitlines() if l.strip() and not l.startswith(("Co-Authored-By", "Claude-Session"))]

def print_reasons(body):
    reasons = reason_lines(body)
    for r in reasons[:MAX_REASONS]:
        print(f"           | {r}")
    if len(reasons) > MAX_REASONS:
        print(f"           | ... {len(reasons) - MAX_REASONS} more lines")

# The first commit that has the line's text, in any file but the docs - a copy's blame stops at the copy
def print_origin(text, sha):
    if len(text) < MIN_LINE_LENGTH:
        return
    files = git("grep", "-lF", "-e", text, "--", ":/", ":(top,exclude)*.md").splitlines()
    if len(files) > MAX_FILES_FOR_A_LINE:
        print(f"   origin  not searched: the same line is in {len(files)} files")
        return
    try:
        run = subprocess.run(["git", "log", "--reverse", f"-S{text}", "--format=%H", "--", ":/", ":(top,exclude)*.md"],
                             capture_output=True, text=True, timeout=ORIGIN_TIMEOUT_S)
    except subprocess.TimeoutExpired:
        print(f"   origin  not found: the search of the history took more than {ORIGIN_TIMEOUT_S} s")
        return
    first = next(iter(run.stdout.split()), "")
    if not first:
        return
    if first == sha:
        print("   origin  this commit - the line's text is not in the history before it")
        return
    print(git("show", "-s", "--format=   origin  the same text was first committed in %h %ad %an%n           %s",
              "--date=short", first).rstrip())
    print_reasons(git("show", "-s", "--format=%b", first))
    where = git("grep", "-lF", "-e", text, first, "--", ":/", ":(top,exclude)*.md").splitlines()
    print(f"           there in {', '.join(w.split(':', 1)[1] for w in where)}")

def why(path, line):
    blame = git("blame", "-L", f"{line},{line}", "--porcelain", "--", path)
    if not blame:
        print(f"== {path}:{line}\n   not in git (no such file or line, or not a repository)")
        return False
    sha = blame.split()[0]
    text = next((l[1:] for l in blame.splitlines() if l.startswith("\t")), "")
    print(f"== {path}:{line}\n   {text.strip()}")
    if set(sha) == {"0"}:
        print("   not committed yet")
        return True
    msg = git("show", "-s", "--format=%h %ad %an%n%s%n%n%b", "--date=short", sha)
    head, _, body = msg.partition("\n")
    subject, _, body = body.partition("\n")
    print(f"   commit  {head}\n           {subject}")
    print_reasons(body)
    session = re.search(r"Claude-Session: (\S+)", body)
    merge = git("log", "--merges", "--ancestry-path", "--reverse", "--format=%h %s", f"{sha}..HEAD").splitlines()
    if merge:
        print(f"   merged  {merge[0]}")
    print_origin(text.strip(), sha)
    refs = sorted(set(re.findall(r"(?:hitlist |open-bugs )?#\d+", subject + " " + body + " " + (merge[0] if merge else ""))))
    print(f"   refs    {', '.join(refs) or '-'}")
    for ref in refs:
        num = ref.split("#")[1]
        hits = git("grep", "-n", "-E", rf"(^|[^0-9&])#{num}([^0-9]|$)", "--", "*.md").splitlines()
        docs = sorted({h.split(":")[0] for h in hits})
        print(f"   {ref:8} in {', '.join(docs) or '-'}")
    common = []
    for name in dict.fromkeys(re.findall(r"[A-Z]\w{3,}", text)):
        hits = git("grep", "-l", "-w", name, "--", "*.md").split()
        if len(hits) > MAX_DOCS_FOR_A_NAME:
            common.append(name)
        elif hits:
            print(f"   {name} named in {', '.join(hits)}")
    if common:
        print(f"   named in more than {MAX_DOCS_FOR_A_NAME} docs, not listed: {', '.join(common)}")
    print(f"   session {session.group(1) if session else '-'}")
    return True

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: why.py <file>:<line> [...]", file=sys.stderr)
        sys.exit(2)
    isOk = True
    for arg in sys.argv[1:]:
        p, _, l = arg.rpartition(":")
        if not p or not l.isdigit():
            print(f"not <file>:<line>: {arg}", file=sys.stderr)
            sys.exit(2)
        isOk = why(p, int(l)) and isOk
        print()
    sys.exit(0 if isOk else 1)
