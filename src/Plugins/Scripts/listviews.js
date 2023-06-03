function main(args) {
    var views = Editor.GetViewNames();
    Console.WriteLine("Views: ", views.length);
    for(var i=0;i<views.length;i++) {
        Console.WriteLine(i, "  - ", views[i]);
    }
}