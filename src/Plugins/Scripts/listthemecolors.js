function main(args) {
    if (args.length != 1) {
        Console.WriteLine("No color class (use: globals, ui or content)");
        return;
    }

    var theme = Editor.GetCurrentTheme();
    if (theme == null) {
        Console.WriteLine("Current Theme is null!");
        return;
    }
    var colors = theme.GetColorsForClass(args[0]);
    if (colors == null) {
        Console.WriteLine("Unable to fetch colors");
    }
    var namedColorVector = colors.ToVector();
    for(var i=0;i<namedColorVector.length;i++) {
        var name = namedColorVector[i].GetName();
        var color = namedColorVector[i].GetColor();
        Console.WriteLine(name, " - (",color.r, color.g, color.b, color.a, ")");
    }

}