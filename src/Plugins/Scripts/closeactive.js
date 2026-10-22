// '.cb [!]' - close the active buffer. Refuses when it has unsaved changes, unless '!' (discard them).
function main(args) {
    var doc = Editor.GetActiveDocument();
    var force = false;
    for (var i = 0; i < args.length; i++) {
        if (args[i] == "!") {
            force = true;
        }
    }
    if (doc.IsModified() && !force) {
        Console.WriteLine("'" + doc.GetName() + "' has unsaved changes - save it first ('.s'), or '.cb !' to discard them");
        return;
    }
    Editor.CloseActiveDocument();
}
