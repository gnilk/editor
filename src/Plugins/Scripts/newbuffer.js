function main(args) {
    var name = "no name";
    if (args.length != 1) {
        Console.WriteLine("No name given");
    } else {
        name = args[0];
    }
    Editor.NewBuffer(name);
}