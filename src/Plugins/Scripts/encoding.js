// '.encoding [name]' - the active buffer's encoding (open-bugs #15). Without a name: what the file was read in, and what
// a save writes. With one: convert on purpose - the next save writes that encoding ('.encoding utf-8' for an old
// Latin-1 file, or when a character it can't hold stops the save).
function main(args) {
    var doc = Editor.GetActiveDocument();
    if (doc == null) {
        Console.WriteLine("ERR: No active document");
        return;
    }
    if (args.length == 0) {
        Console.WriteLine("Encoding: " + doc.GetEncoding());
        return;
    }
    if (!doc.SetEncoding(args[0])) {
        Console.WriteLine("ERR: Unknown encoding '" + args[0] + "' - utf-8, utf-8-bom, utf-16le, utf-16be, cp1252, latin1");
        return;
    }
    Console.WriteLine("Encoding: " + doc.GetEncoding() + " - written on the next save");
}
