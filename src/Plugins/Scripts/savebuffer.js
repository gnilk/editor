// '.s [!]' / '.sa <name>' - save the active buffer. When another program rewrote the file since it was read (an agent,
// a checkout - the header shows '[!]'), '.s' asks first: Overwrite (what the other program wrote is lost), Reload
// (take the file, drop the changes here) or Cancel. '!' saves at once (docs/usability-round.md §0b, §5).
function main(args) {
    var doc = Editor.GetActiveDocument();
    var force = false;
    var names = [];
    for (var i = 0; i < args.length; i++) {
        if (args[i] == "!") {
            force = true;
        } else {
            names.push(args[i]);
        }
    }
    if (names.length == 1) {
        Console.WriteLine("Saving as:", names[0]);
        if (doc.SaveAs(names[0])) {
            Console.WriteLine("Buffer saved as:", doc.GetFileName());
        } else {
            Console.WriteLine("ERR: Unable to save buffer as: ", names[0]);
        }
        return;
    }
    if (!doc.IsChangedOnDisk() || force) {
        save(doc);
        return;
    }
    Editor.Confirm("'" + doc.GetName() + "' was changed on disk since it was read", ["Overwrite", "Reload", "Cancel"], function(choice) {
        if (choice == 0) {
            save(doc);
        } else if (choice == 1) {
            if (doc.Reload()) {
                Console.WriteLine("Reloaded '" + doc.GetName() + "'");
            } else {
                Console.WriteLine("ERR: Unable to reload '" + doc.GetName() + "'");
            }
        }
    });
}

function save(doc) {
    if (doc.Save()) {
        Console.WriteLine("Buffer saved");
    } else {
        Console.WriteLine("ERR: Unable to save buffer");
    }
}
