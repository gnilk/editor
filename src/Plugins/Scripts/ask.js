// '.ask <question>' - a question to the Editor Agent (agent-integration.md §6.3). The prompt is shown for your OK first;
// then the agent is started if it doesn't run, the prompt is typed into it once it has settled, and its answer is
// opened. The question is kept with its answer: .goatedit/agent_qa/q_<id>.md and a_<id>.md.
function main(args) {
    // the command line is split on spaces - glue the question back together
    var words = [];
    for (var i = 0; i < args.length; i++) {
        if (String(args[i]) != "") {
            words.push(String(args[i]));
        }
    }
    Console.WriteLine(Editor.AskEditorAgent(words.join(" ")));
}
