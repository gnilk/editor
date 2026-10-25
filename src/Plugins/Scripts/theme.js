// .theme --import <file> [--name <name>] [--force]
//   Convert a VS Code theme (.json) or Sublime color scheme (.sublime-color-scheme) into a GoatEdit theme
//   (<name>.theme.yml + <name>.colors.json in the user config dir) - same as 'goatedit --import-theme'.
//   The command line is split on spaces: a file name with spaces may be typed as is or in quotes.
function main(args) {
    var usage = [
        "use: .theme --import <file> [--name <name>] [--force]",
        "  --import <file>  - convert a VS Code theme (.json) or Sublime color scheme (.sublime-color-scheme)",
        "  --name <name>    - file name for the result (default: from the theme's name)",
        "  --force          - overwrite an earlier import of the same name"
    ];
    var file = "";
    var name = "";
    var isForced = false;
    var isValid = true;
    for (var i = 0; i < args.length; i++) {
        if (args[i] == "") {
            continue;
        }
        if (args[i] == "--import") {
            // Everything up to the next option is the file - glue the words back together
            var words = [];
            while ((i + 1 < args.length) && (args[i + 1].indexOf("--") != 0)) {
                words.push(args[++i]);
            }
            file = words.join(" ").replace(/^\s+|\s+$/g, "").replace(/^["']|["']$/g, "");
        } else if (args[i] == "--name" && (i + 1 < args.length)) {
            name = args[++i];
        } else if (args[i] == "--force") {
            isForced = true;
        } else {
            isValid = false;
        }
    }

    var lines = usage;
    if (isValid && (file != "")) {
        lines = Themes.Import(file, name, isForced);
    }
    for (var l = 0; l < lines.length; l++) {
        Console.WriteLine(lines[l]);
    }
}
