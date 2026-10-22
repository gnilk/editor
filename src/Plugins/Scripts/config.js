// .cfg [--user | --project] <action> [args]
//   .cfg list [prefix]                        - running config, with the level each value comes from
//   .cfg --user list [prefix]                 - the user config file as-is (--project: the project's)
//   .cfg [--user | --project] get <key>
//   .cfg --user set <section.key> <value>     - edits the file in place (comments are kept)
//   .cfg --project edit                       - open the config file in a buffer (--user: the user's)
function main(args) {
    var usage = [
        "use: .cfg [--user | --project] <action> [args]",
        "  list [prefix]         - list the running config (or the --user/--project file)",
        "  get <key>             - show a value, e.g. editorview.wrap",
        "  set <key> <value>     - write a value (--user or --project)",
        "  edit                  - open the config file in a buffer (--user or --project)"
    ];
    var level = "";
    var rest = [];
    for (var i = 0; i < args.length; i++) {
        if (args[i] == "") {
            continue;
        }
        if (args[i] == "--user") {
            level = "user";
        } else if (args[i] == "--project") {
            level = "project";
        } else {
            rest.push(args[i]);
        }
    }

    var lines = usage;
    var action = (rest.length > 0) ? rest[0] : "";
    if (action == "list" && rest.length <= 2) {
        lines = Config.List(level, (rest.length == 2) ? rest[1] : "");
    } else if (action == "get" && rest.length == 2) {
        lines = Config.Get(level, rest[1]);
    } else if (action == "set" && rest.length >= 3) {
        // the command line is split on spaces - glue the value back together
        lines = Config.Set(level, rest[1], rest.slice(2).join(" "));
    } else if (action == "edit" && rest.length == 1) {
        lines = Config.Edit(level);
    }
    for (var l = 0; l < lines.length; l++) {
        Console.WriteLine(lines[l]);
    }
}
