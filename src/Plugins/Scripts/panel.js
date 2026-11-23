// '.panel move' - the shown tab of the focused panel goes to the other one (hitlist #77): its home changes, so it stays
// there after a restart; in portrait (one panel) only its home changes - it shows on the right in landscape.
// From quick mode: the panel used last.
function main(args) {
    var action = (args.length > 0) ? String(args[0]).trim() : "";
    if (action == "move") {
        Console.WriteLine(Terminal.MoveTab());
        return;
    }
    Console.WriteLine("Usage: .panel move - the shown tab to the other panel");
}
