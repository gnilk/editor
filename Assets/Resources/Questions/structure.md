---
name: structure
title: What did this change do to the structure?
args: [base]
tools:
  typediff: typediff.py --goatedit {goatedit} {base}
max_lines: 300
---
What did the change in progress do to the structure of the code?

This is a type-diff of the C/C++ files the change touched, at its base and in the work tree as saved - types, the
inherits / holds edges (with ownership) and the includes between folders, then the functions and members (those lists
are long and may be cut):

{tool:typediff}

Judge what it shows - the type-diff only lists. Look at the code where the list isn't enough. Answer:
- what the change did to the architecture, in a few sentences: new types and what they are for, who owns what now;
- new dependencies between folders, and whether they point the right way (a lower layer reaching up is wrong);
- ownership that looks risky - a borrowed pointer to something another type owns, shared where one owner would do;
- anything that breaks the rules the project wrote down (CLAUDE.md, AGENTS.md, its docs), with the line;
- what you are unsure of.
Keep it short. Name files and types; skip what looks fine.
