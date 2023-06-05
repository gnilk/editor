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
    if (!isVisible) {
        Console.WriteLine("Showing view: ", args[0]);
        view.SetVisible(true);
    } else {
        Console.WriteLine("View '",args[0],"' already visible");
    }
}