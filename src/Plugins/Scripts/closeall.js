// '.closeall [!]' / '.ca' - close every open buffer. With changes not yet saved in any of them it asks, listing them:
// Save all (then close), Discard (close, the changes are dropped) or Cancel. '!' closes them all at once.
function main(args) {
    var force = false;
    for (var i = 0; i < args.length; i++) {
        if (args[i] == "!") {
            force = true;
        }
    }
    var modified = [];
    var docs = Editor.GetDocuments();
    for (var i = 0; i < docs.length; i++) {
        if (docs[i].IsModified()) {
            modified.push(docs[i]);
        }
    }
    if ((modified.length == 0) || force) {
        Editor.CloseAllDocuments();
        return;
    }

    var names = [];
    for (var i = 0; i < modified.length; i++) {
        names.push("  " + modified[i].GetName());
    }
    var message = "Changes not yet saved in:\n" + names.join("\n");
    Editor.Confirm(message, ["Save all", "Discard", "Cancel"], function(choice) {
        if (choice == 0) {
            for (var i = 0; i < modified.length; i++) {
                if (!modified[i].Save()) {
                    Console.WriteLine("ERR: Unable to save '" + modified[i].GetName() + "' - nothing closed");
                    return;
                }
            }
            Editor.CloseAllDocuments();
        } else if (choice == 1) {
            Editor.CloseAllDocuments();
        }
    });
}
