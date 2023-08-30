function main(args) {
    var docs = Editor.GetDocuments();
    Console.WriteLine("Documents: ", docs.length);
    for(var i=0;i<docs.length;i++) {
        var doc = docs[i];
        var name = doc.GetName();
        Console.WriteLine(i, "  - ", name);
    }
}