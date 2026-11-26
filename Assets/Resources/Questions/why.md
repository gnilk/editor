---
name: why
title: Why is this line here?
needs: [file, line]
tools:
  chain: why.py {file}:{line}
---
Why is line {line} of `{file}` here?

This is what the repository records about it - the commit that last changed
the line, its message, the merge that brought it in, where the same text was
first committed when that is another commit (the line may be a copy), the #N
it names and the docs that mention those numbers or the line's rarer names:

{tool:chain}

Answer from that record first. Read the commit (`git show`), the doc
sections it points at or the code around the line when the record isn't
enough - and say which you used. Then:
- the reason for the line, in a few sentences;
- whether it was decided (a #N, a doc section, a commit that argues for it)
  or just happened;
- anything known to be wrong about it, if a known-problems list or the
  hitlist names it.
If the record doesn't say, say that - don't guess. Keep it short; link the
commits and doc sections you rely on.
