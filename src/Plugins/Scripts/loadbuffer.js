function main(args) {
    if (args.length != 1) {
        Console.WriteLine("You must supply a filename!");
        return;
    }
    Editor.LoadBuffer(args[0]);
}