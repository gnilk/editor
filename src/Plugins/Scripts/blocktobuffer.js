// Open the terminal's currently selected command-block output in a new, editable buffer.
// Select a block first: scroll the terminal up (or Alt+Up/Down to jump per command) so a block is
// highlighted, then run this command. Following the live bottom = nothing selected.
function main(args) {
    var text = Terminal.GetSelectedBlock();
    if (text === null || text === undefined) {
        Console.WriteLine("No command block selected - scroll the terminal up and select a block first");
        return;
    }
    Editor.NewDocumentFromText("output", text);
}
