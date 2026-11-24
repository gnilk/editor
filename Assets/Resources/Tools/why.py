#!/usr/bin/env python3
# "Why is this line here?" - the chain the repository already records: blame -> the commit -> its message, the #N /
# open-bugs #N it names, the merge that brought it in -> the Markdown docs that mention those numbers or the line's rarer
# names -> the Claude session. Plain text, for a person and for the agent (agent-integration.md §6.4, the 'why' question).
# Shipped with GoatEdit (Tools/); run in the project root.
# Usage: why.py <file>:<line> [...]
import re, subprocess, sys

# A name named in more docs than this says nothing about the line ('ViewBase', 'Core')
MAX_DOCS_FOR_A_NAME = 3

def git(*args):
    return subprocess.run(["git", *args], capture_output=True, text=True).stdout

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
