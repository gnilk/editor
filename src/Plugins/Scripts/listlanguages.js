function main(args) {
    var languages = Config.GetLanguages();
    Console.WriteLine("Supported languages");
    foreach(l : languages) {
        Console.WriteLine(l.GetExtension() + ", " + l.GetDescription());
    }
}