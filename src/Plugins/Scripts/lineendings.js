// '.line_endings [unix|windows|platform]' - the active buffer's line endings (docs/windows-port.md §4.2). Without a name:
// what the file has - unix, windows or mixed. With one: convert every line on purpose, written on the next save.
function main(args) {
    var doc = Editor.GetActiveDocument();
    if (doc == null) {
        Console.WriteLine("ERR: No active document");
        return;
    }
    if (args.length == 0) {
        Console.WriteLine("Line endings: " + doc.GetLineEndings());
        return;
    }
    if (!doc.SetLineEndings(args[0])) {
        Console.WriteLine("ERR: Unknown line endings '" + args[0] + "' - unix, windows or platform");
        return;
    }
    Console.WriteLine("Line endings: " + doc.GetLineEndings() + " - written on the next save");
}
