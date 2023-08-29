function main(args) {

    var doc = Editor.GetActiveDocument();
    if (args.length == 1) {
        Console.WriteLine("Saving as:",args[0]);
        if (doc.SaveAs(args[0])) {
            Console.WriteLine("Buffer saved as:", doc.GetFileName());
        } else {
            Console.WriteLine("ERR: Unable to save buffer as: ", args[0]);
        }
    } else {
        if (doc.Save()) {
            Console.WriteLine("Buffer saved");
        } else {
            Console.WriteLine("ERR: Unable to save buffer");
        }
    }
}