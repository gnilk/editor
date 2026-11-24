// '.ask <question>' - a question to the Editor Agent (agent-integration.md §6.3). The prompt is shown for your OK first;
// then the agent is started if it doesn't run, the prompt is typed into it once it has settled, and its answer is
// opened. The question is kept with its answer: .goatedit/agent_qa/q_<id>.md and a_<id>.md.
// '.ask <template> [args]' - a question from a template (§6.4): 'why' (the line under the caret), 'structure [base]'
// (what this change did to the code's structure). Its tools run first; what they found is in the prompt you OK.
// '.ask list' - the templates.
function main(args) {
    // the command line is split on spaces - glue the question back together
    var words = [];
    for (var i = 0; i < args.length; i++) {
        if (String(args[i]) != "") {
            words.push(String(args[i]));
        }
    }
    var lines = Editor.AskEditorAgent(words.join(" "));
    for (var i = 0; i < lines.length; i++) {
        Console.WriteLine(lines[i]);
    }
}
