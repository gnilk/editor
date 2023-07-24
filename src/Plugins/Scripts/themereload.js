function main(args) {
    var theme = Editor.GetCurrentTheme();
    if (theme == null) {
        Console.WriteLine("Current Theme is null!");
        return;
    }
    if (theme.Reload()) {
        Console.WriteLine("Theme reloaded ok!");
    }
}