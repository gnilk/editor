// '.layout [portrait | landscape | auto]' - where the panels go (hitlist #77). portrait: one panel below the editor with
// every tab; landscape: panel_right's tabs in a full-height panel on the right; auto: landscape when the window is
// wider than 3:2, following the window as it is resized. Now, for this run - 'main.layout' in the config is the start.
// No argument: the layout now.
function main(args) {
    if (args.length == 0) {
        Console.WriteLine("Layout: " + Terminal.GetLayout());
        return;
    }
    var setting = String(args[0]).trim();
    if (!Terminal.SetLayout(setting)) {
        Console.WriteLine("Usage: .layout [portrait | landscape | auto]");
        return;
    }
    Console.WriteLine("Layout: " + Terminal.GetLayout() + " - for this run ('.cfg set main.layout " + setting + "' keeps it)");
}
