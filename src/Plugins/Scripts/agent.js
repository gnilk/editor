// '.agent' - the AI agent the editor works with (hitlist #78): 'main.ai_integration' in the user config (none | claude),
// whether its command is found, and what the editor would start for this project - the working agent, the read-only
// question agent - and where the agent keeps this project's transcripts. Nothing is started or sent.
function main(args) {
    var lines = Editor.GetAgentInfo();
    for (var i = 0; i < lines.length; i++) {
        Console.WriteLine(lines[i]);
    }
}
