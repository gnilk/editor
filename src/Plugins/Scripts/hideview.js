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
    if (isVisible) {
        Console.WriteLine("Hiding view: ", args[0]);
        view.SetVisible(false);
    } else {
        Console.WriteLine("View '",args[0],"' already hidden");
    }
}