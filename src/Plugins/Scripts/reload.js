// '.reload [!]' - take in the file as it is on disk now (docs/usability-round.md §0b). A buffer with changes not yet
// saved asks first: Reload (the changes are dropped - one undo step brings them back) or Cancel. '!' reloads at once.
function main(args) {
    var doc = Editor.GetActiveDocument();
    var force = false;
    for (var i = 0; i < args.length; i++) {
        if (args[i] == "!") {
            force = true;
        }
    }
    if (!doc.IsModified() || force) {
        reload(doc);
        return;
    }
    Editor.Confirm("Reload '" + doc.GetName() + "' and drop the changes not yet saved?", ["Reload", "Cancel"], function(choice) {
        if (choice == 0) {
            reload(doc);
        }
    });
}

function reload(doc) {
    if (!doc.Reload()) {
        Console.WriteLine("ERR: Unable to reload '" + doc.GetName() + "' - is it still on disk?");
        return;
    }
    Console.WriteLine("Reloaded '" + doc.GetName() + "'");
}
