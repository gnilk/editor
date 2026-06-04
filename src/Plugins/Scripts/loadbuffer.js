function main(args) {
    if (args.length != 1) {
        Console.WriteLine("You must supply a filename!");
        return;
    }
    var doc = Editor.LoadDocument(args[0]);
    if (doc == null) {
        Console.WriteLine("Unable to load file");
        return;
    }
}