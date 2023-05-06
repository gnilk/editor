function main(args) {
    if (args.length != 1) {
        Console.WriteLine("You must supply a filename!");
        return;
    }
    var textBuffer = Editor.LoadBuffer(args[0]);
    if (textBuffer == null) {
        Console.WriteLine("Unable to load file");
        return;
    }
    Editor.SetActiveBuffer(textBuffer);
}