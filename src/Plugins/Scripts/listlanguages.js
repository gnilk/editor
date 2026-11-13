// '.ll' - the languages: name, extensions, and for a language file its version, author and where it was loaded from
function main(args) {
    var languages = Editor.GetLanguageDescriptions();
    Console.WriteLine("Supported languages: ", languages.length);
    for (var i = 0; i < languages.length; i++) {
        Console.WriteLine(languages[i]);
    }
}
