// .theme list | .theme use [--user | --project] <name> | .theme --import <file> [--name <name>] [--force]
//   list   - the themes 'use' can switch to: the user's (themes/ next to the user config) and the shipped ones, and
//            where main.theme comes from
//   use    - switch to one now, and save it as main.theme in the user config (--project: the project's) - <name>: the
//            file name without .theme.yml, or the theme's own name (words with spaces are glued back together). A
//            project whose config sets another theme keeps it there: --user asks whether to take it out (hitlist #59)
//   --import - convert a VS Code theme (.json) or Sublime color scheme (.sublime-color-scheme) into a GoatEdit theme
//   (<name>.theme.yml + <name>.colors.json in themes/ next to the user config) - same as 'goatedit --import-theme'.
//   The command line is split on spaces: a file name with spaces may be typed as is or in quotes.
function main(args) {
    var usage = [
        "use: .theme list | .theme use [--user | --project] <name> | .theme --import <file> [--name <name>] [--force]",
        "  list             - the themes there are (yours and the shipped ones), the current one marked *",
        "  use <name>       - switch to one now and keep it (main.theme in your user config)",
        "    --project      - keep it in this project's config instead",
        "  --import <file>  - convert a VS Code theme (.json) or Sublime color scheme (.sublime-color-scheme)",
        "  --name <name>    - file name for the result (default: from the theme's name)",
        "  --force          - overwrite an earlier import of the same name"
    ];
    // .theme list / .theme use <name>
    var words = [];
    for (var w = 0; w < args.length; w++) {
        if (args[w] != "") {
            words.push(args[w]);
        }
    }
    if ((words.length == 1) && (words[0] == "list")) {
        writeLines(Themes.List());
        return;
    }
    if ((words.length >= 2) && (words[0] == "use")) {
        use(words.slice(1));
        return;
    }
    if (words.length == 0) {
        writeLines(Themes.List());
        return;
    }

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
    writeLines(lines);
}

// .theme use [--user | --project] <name>
function use(words) {
    var level = "user";
    var nameWords = [];
    for (var i = 0; i < words.length; i++) {
        if (words[i] == "--user") {
            level = "user";
        } else if (words[i] == "--project") {
            level = "project";
        } else {
            nameWords.push(words[i]);
        }
    }
    var name = nameWords.join(" ").replace(/^["']|["']$/g, "");
    var fileName = Themes.Find(name);
    var projectTheme = Themes.ProjectTheme();
    // The project's own theme wins here: take it out, or keep it and let the choice apply in other projects
    if ((level == "user") && (fileName != "") && (projectTheme != "") && (projectTheme != fileName)) {
        Editor.Confirm("This project's config sets its own theme (" + projectTheme + ") - it wins here",
                       ["Remove it from the project", "Keep the project's", "Cancel"], function(choice) {
            if (choice == 0) {
                writeLines(Config.Unset("project", "main.theme"));
                writeLines(Themes.Use(name, level));
            } else if (choice == 1) {
                writeLines(Themes.Use(name, level));
            }
        });
        return;
    }
    writeLines(Themes.Use(name, level));
}

function writeLines(lines) {
    for (var l = 0; l < lines.length; l++) {
        Console.WriteLine(lines[l]);
    }
}
