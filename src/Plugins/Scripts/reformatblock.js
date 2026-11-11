// '.fm_block' / '.fmb' - reformat as Cmd+I does: the selection, else the enclosing '{ }' block, else the current line.
// docs/usability-round.md §8. One undo step; a language with reformat off (YAML) is left alone.
function main(args) {
    var doc = Editor.GetActiveDocument();
    if (!doc.IsReformatEnabled()) {
        Console.WriteLine("Reformat is off for '" + doc.GetLanguage() + "' - nothing changed");
        return;
    }
    if (!doc.ReformatBlock()) {
        Console.WriteLine("ERR: Unable to reformat '" + doc.GetName() + "'");
    }
}
