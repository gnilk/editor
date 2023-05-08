function main(args) {
    var textBuffer = Editor.GetActiveTextBuffer();
    if (args.length == 1) {
        Console.WriteLine("Saving as:",args[0]);
        textBuffer.SetFileName(args[0]);
        if (textBuffer.SaveBuffer()) {
            Console.WriteLine("Buffer saved");
        }
    } else if (textBuffer.HasFileName()) {
        if (textBuffer.SaveBuffer()) {
            Console.WriteLine("Buffer saved");
        }
    } else {
        Console.WriteLine("Can't save - no filename..");
    }
}