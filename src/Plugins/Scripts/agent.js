// '.agent' - the AI agent the editor works with (hitlist #78): 'main.ai_integration' in the user config (none | claude),
// whether its command is found, and what the editor would start for this project - the working agent, the read-only
// question agent - and where the agent keeps this project's transcripts.
// '.agent start' - the Editor Agent's tab (agent-integration.md §6.2): the read-only agent the editor will ask its
// questions, in the bottom panel - typing the command is the user's OK to start it. Shown when it runs already.
function main(args) {
    var action = (args.length > 0) ? String(args[0]).trim() : "";
    if (action == "start") {
        Console.WriteLine(Editor.StartEditorAgent());
        return;
    }
    if (action != "") {
        Console.WriteLine("Usage: .agent - which agent, what it would run; .agent start - the Editor Agent's tab");
        return;
    }
    var lines = Editor.GetAgentInfo();
    for (var i = 0; i < lines.length; i++) {
        Console.WriteLine(lines[i]);
    }
}
