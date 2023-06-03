function main(args) {
    if (args.length != 1) {
        Console.WriteLine("No name!");
        return;
    }
    var view = Editor.GetViewByName(args[0]);
    if (!view.IsValid()) {
        Console.WriteLine("No view named (case-sensitive): ", args[0]);
        return;
    }
    var isVisible = view.IsVisible();
    Console.WriteLine("IsVisible: ", isVisible);
}