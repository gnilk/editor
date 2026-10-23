// '.cb [!]' - close the active buffer. With changes not yet saved (or autosaved) it asks: Save (then
// close), Discard (close, the changes are dropped) or Cancel. '!' closes at once, dropping them.
function main(args) {
    var doc = Editor.GetActiveDocument();
    var force = false;
    for (var i = 0; i < args.length; i++) {
        if (args[i] == "!") {
            force = true;
        }
    }
    if (!doc.IsModified() || force) {
        Editor.CloseActiveDocument();
        return;
    }
    // The dialog has the input until it closes - the active buffer is still 'doc' when this runs
    Editor.Confirm("'" + doc.GetName() + "' has changes not yet saved", ["Save", "Discard", "Cancel"], function(choice) {
        if (choice == 0) {
            if (!doc.Save()) {
                Console.WriteLine("ERR: Unable to save '" + doc.GetName() + "' - not closed");
                return;
            }
            Editor.CloseActiveDocument();
        } else if (choice == 1) {
            Editor.CloseActiveDocument();
        }
    });
}
