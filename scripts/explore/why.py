#!/usr/bin/env python3
# "Why is this line here?" - the chain the repo already records: blame -> the commit -> its message, the #N / open-bugs #N
# it names, the merge that brought it in -> the docs that mention those numbers or the line's names -> the Claude session.
# Usage: why.py <file>:<line> [...]
import re, subprocess, sys

def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True).stdout

def why(path, line):
    blame = git("blame", "-L", f"{line},{line}", "--porcelain", path)
    sha = blame.split()[0]
    text = next((l[1:] for l in blame.splitlines() if l.startswith("\t")), "")
    print(f"== {path}:{line}\n   {text.strip()}")
    if set(sha) == {"0"}:
        print("   not committed yet")
        return
    msg = git("show", "-s", "--format=%h %ad %an%n%s%n%n%b", "--date=short", sha)
    head, _, body = msg.partition("\n")
    subject, _, body = body.partition("\n")
    print(f"   commit  {head}\n           {subject}")
    reasons = [l.strip() for l in body.splitlines() if l.strip() and not l.startswith(("Co-Authored-By", "Claude-Session"))]
    for r in reasons[:8]:
        print(f"           | {r}")
    if len(reasons) > 8:
        print(f"           | ... {len(reasons) - 8} more lines")
    session = re.search(r"Claude-Session: (\S+)", body)
    merge = git("log", "--merges", "--ancestry-path", "--reverse", "--format=%h %s", f"{sha}..HEAD").splitlines()
    if merge:
        print(f"   merged  {merge[0]}")
    refs = sorted(set(re.findall(r"(?:hitlist |open-bugs )?#\d+", subject + " " + body + " " + (merge[0] if merge else ""))))
    print(f"   refs    {', '.join(refs) or '-'}")
    names = [n for n in re.findall(r"[A-Z]\w{3,}", text)][:3]
    for ref in refs:
        num = ref.split("#")[1]
        hits = git("grep", "-n", "-E", rf"(^|[^0-9&])#{num}([^0-9]|$)", "--", "docs/*.md").splitlines()
        docs = sorted({h.split(":")[0] for h in hits})
        print(f"   {ref:8} in {', '.join(docs) or '-'}")
    for name in names:
        hits = git("grep", "-l", "-w", name, "--", "docs/*.md", "CLAUDE.md").split()
        if hits:
            print(f"   {name} named in {', '.join(hits)}")
    print(f"   session {session.group(1) if session else '-'}")

for arg in sys.argv[1:]:
    p, l = arg.rsplit(":", 1)
    why(p, int(l))
    print()
