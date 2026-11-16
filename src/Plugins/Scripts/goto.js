// '.goto <line>' / '.g <line>' - go to a line of the active buffer. Counts from 1, as compilers and grep do; past the end is the
// last line. The caret lands on the line's first non-blank, and it's a jump: Cmd+[ / Ctrl+[ comes back.
// Quick mode 'g' types '.g ' for you.
function main(args) {
    var doc = Editor.GetActiveDocument();
    if (doc == null) {
        Console.WriteLine("ERR: No active document");
        return;
    }
    var line = (args.length > 0) ? parseInt(args[0], 10) : NaN;
    if (isNaN(line) || (line < 1)) {
        Console.WriteLine("Usage: .goto <line> (or .g <line>) - counts from 1");
        return;
    }
    doc.GotoLine(line);
}
