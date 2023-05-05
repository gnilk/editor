function main(args) {
    var languages = Editor.GetRegisteredLanguages();
    Console.WriteLine("Supported languages: ", languages.length);
    for(i = 0; i<languages.length;i++) {
        Console.WriteLine(languages[i]);
    }
}