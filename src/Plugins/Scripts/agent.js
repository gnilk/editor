// '.agent' - the AI agent the editor works with (hitlist #78): 'main.ai_integration' in the user config (none | claude),
// whether its command is found, and what the editor would start for this project - the working agent, the read-only
// question agent - and where the agent keeps this project's transcripts.
// '.agent start' - the Editor Agent's tab (agent-integration.md §6.2): the read-only agent the editor will ask its
// questions, in the bottom panel - typing the command is the user's OK to start it. Shown when it runs already.
// '.agent model [name]' - the Editor Agent's model for this session ('default': back to main.ai_model); one that runs is
// started over with it. Without a name: which it is.
function main(args) {
    var action = (args.length > 0) ? String(args[0]).trim() : "";
    if (action == "start") {
        Console.WriteLine(Editor.StartEditorAgent());
        return;
    }
    if (action == "model") {
        printLines(Editor.SetEditorAgentModel((args.length > 1) ? String(args[1]).trim() : ""));
        return;
    }
    if (action != "") {
        Console.WriteLine("Usage: .agent - which agent, what it would run; .agent start - the Editor Agent's tab; .agent model [name] - its model");
        return;
    }
    printLines(Editor.GetAgentInfo());
}

function printLines(lines) {
    for (var i = 0; i < lines.length; i++) {
        Console.WriteLine(lines[i]);
    }
}
