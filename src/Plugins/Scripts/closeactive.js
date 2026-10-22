// '.cb [!]' - close the active buffer. Refuses when it has changes not yet saved (or autosaved), unless '!'
// (close anyway - those changes are dropped).
function main(args) {
    var doc = Editor.GetActiveDocument();
    var force = false;
    for (var i = 0; i < args.length; i++) {
        if (args[i] == "!") {
            force = true;
        }
    }
    if (doc.IsModified() && !force) {
        Console.WriteLine("'" + doc.GetName() + "' has changes not yet saved - save it ('.s'), or '.cb !' to close anyway (they are dropped)");
        return;
    }
    Editor.CloseActiveDocument();
}
