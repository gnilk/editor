//
// Created by gnilk on 15.06.26.
//
// Markdown (.md / .markdown) syntax support.
//
// SKELETON: this only wires up language detection and a 'main' start state so .md files route here
// instead of the default language. The actual tokenizer/highlighting rules are not implemented yet -
// see docs/support-markdown.md for the staged plan (§2 state config + §2.B post-process pass).
//

#include "MarkdownLanguage.h"

using namespace gedit;

bool MarkdownLanguage::Initialize() {
    // An empty 'main' state - parses everything as regular text for now. The tokenizer requires a
    // valid start state, so this is the minimum to be a functioning (no-op) language.
    tokenizer.GetOrAddState("main");
    tokenizer.SetStartState("main");

    // Grab configuration (if any) - also the key into autopairs.yml
    ConfigFromNodeName("markdown");

    return true;
}
