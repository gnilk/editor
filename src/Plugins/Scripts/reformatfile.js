// '.fm_file' / '.fmf' - reformat (re-derive the indentation of) every line of the active buffer - one undo step.
// docs/usability-round.md §8. The indent engine does the work; a language with reformat off (YAML) is left alone.
function main(args) {
    var doc = Editor.GetActiveDocument();
    if (!doc.IsReformatEnabled()) {
        Console.WriteLine("Reformat is off for '" + doc.GetLanguage() + "' - nothing changed");
        return;
    }
    if (!doc.ReformatFile()) {
        Console.WriteLine("ERR: Unable to reformat '" + doc.GetName() + "'");
        return;
    }
    Console.WriteLine("Reformatted '" + doc.GetName() + "'");
}
